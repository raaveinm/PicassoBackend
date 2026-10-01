//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/AuthService.hpp"

#include <array>
#include <chrono>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "domain/Clock.hpp"

namespace picasso::service {
    namespace {
        constexpr std::chrono::hours SESSION_TTL{24 * 30};

        /* How long a minted-but-unclaimed token waits for its app to poll for it. */
        constexpr std::int64_t PENDING_LOGIN_TTL_MS = 5 * 60 * 1000;

        std::string toHex(const unsigned char* bytes, const unsigned int length) {
            static constexpr char DIGITS[] = "0123456789abcdef";
            std::string out;
            out.reserve(length * 2);
            for (unsigned int i = 0; i < length; ++i) {
                out.push_back(DIGITS[bytes[i] >> 4]);
                out.push_back(DIGITS[bytes[i] & 0x0F]);
            }
            return out;
        }

        std::string sha256Hex(const std::string& input) {
            std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
            unsigned int digestLen = 0;

            EVP_MD_CTX* ctx = EVP_MD_CTX_new();
            EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
            EVP_DigestUpdate(ctx, input.data(), input.size());
            EVP_DigestFinal_ex(ctx, digest.data(), &digestLen);
            EVP_MD_CTX_free(ctx);

            return toHex(digest.data(), digestLen);
        }

        std::string generateOpaqueToken() {
            std::array<unsigned char, 32> bytes{};
            if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
                throw std::runtime_error("RAND_bytes failed");
            }
            return toHex(bytes.data(), bytes.size());
        }

        std::string stripBearerPrefix(const std::string& token) {
            constexpr std::string_view prefix = "Bearer ";
            return token.starts_with(prefix) ? token.substr(prefix.size()) : token;
        }
    } // namespace

    AuthService::AuthService(
        std::shared_ptr<domain::SessionRepository> sessions,
        std::shared_ptr<steam::OpenIdVerifier> verifier)
        : sessions_(std::move(sessions)), verifier_(std::move(verifier)) {
    }

    std::string AuthService::beginLoginUrl(const std::string& state) const {
        return verifier_->buildAuthUrl(state);
    }

    std::optional<IssuedToken> AuthService::completeLogin(const std::map<std::string, std::string>& params) const {
        const auto steamId = steam::OpenIdVerifier::verify(params);

        if (!steamId) return std::nullopt;

        const auto plaintext = generateOpaqueToken();
        const auto now = domain::nowEpochMs();

        domain::Session session;
        session.token_hash = sha256Hex(plaintext);
        session.steam_id = *steamId;
        session.created_at_epoch_ms = now;
        session.expires_at_epoch_ms = now + std::chrono::duration_cast<std::chrono::milliseconds>(SESSION_TTL).count();
        sessions_->store(session);

        return IssuedToken{.token = plaintext, .steamId = *steamId, .expiresAtEpochMs = session.expires_at_epoch_ms};
    }

    std::optional<domain::SteamId> AuthService::authenticate(const std::string& token) const {
        const auto plaintext = stripBearerPrefix(token);
        const auto session = sessions_->findByTokenHash(sha256Hex(plaintext));

        if (!session || !session->isUsableAt(domain::nowEpochMs())) return std::nullopt;

        return session->steam_id;
    }

    void AuthService::logout(const std::string& token) const {
        sessions_->revoke(sha256Hex(stripBearerPrefix(token)));
    }

    ///////////////////////////////////////////////
    /// Pending logins (the poll handoff)
    ///////////////////////////////////////////////

    void AuthService::prunePendingLocked(const std::int64_t nowEpochMs) const {
        std::erase_if(pending_, [nowEpochMs](const auto& entry) {
            return nowEpochMs - entry.second.parkedAtEpochMs > PENDING_LOGIN_TTL_MS;
        });
    }

    void AuthService::parkLogin(const std::string& state, const IssuedToken& issued) const {
        if (state.empty()) return;

        const auto now = domain::nowEpochMs();
        std::lock_guard lock(pendingMutex_);
        prunePendingLocked(now);
        pending_[state] = PendingLogin{.issued = issued, .parkedAtEpochMs = now};
    }

    std::optional<IssuedToken> AuthService::claimLogin(const std::string& state) const {
        if (state.empty()) return std::nullopt;

        const auto now = domain::nowEpochMs();
        std::lock_guard lock(pendingMutex_);
        prunePendingLocked(now);

        const auto it = pending_.find(state);
        if (it == pending_.end()) return std::nullopt;

        /* Single use: a second poll with the same nonce gets nothing. */
        auto issued = it->second.issued;
        pending_.erase(it);
        return issued;
    }
} // namespace picasso::service