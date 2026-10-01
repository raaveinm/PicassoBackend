//
// Created by Kirill "Raaveinm" on 9/9/26.
//

// otherwise throws an compilation err
// ReSharper disable CppVariableCanBeMadeConstexpr
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "domain/Ids.hpp"
#include "domain/ports/SessionRepository.hpp"
#include "steam/OpenIdVerifier.hpp"

namespace picasso::service {

    const std::string AUTH_TAG{"AUTH_SERVICE"};
    struct IssuedToken {
        /* Plaintext. Returned to the client once, never stored. */
        std::string token;
        domain::SteamId steamId;
        std::int64_t expiresAtEpochMs{};
    };

    class AuthService {
    public:
        AuthService(
            std::shared_ptr<domain::SessionRepository> sessions,
            std::shared_ptr<steam::OpenIdVerifier> verifier);

        [[nodiscard]] std::string beginLoginUrl(const std::string& state = "") const;

        // Verifies the Steam assertion, mints a session token
        [[nodiscard]]std::optional<IssuedToken> completeLogin(const std::map<std::string, std::string>& params) const;

        /**
         * The single gate for the WS upgrade. Returns the identity that the session
         * will carry for its entire lifetime - every message it later sends is
         * attributed to this steamId and to nothing the client can influence.
         */
        [[nodiscard]]std::optional<domain::SteamId> authenticate(const std::string& token) const;

        /* Explicit logout. Idempotent: revoking an unknown or already-revoked hash is a no-op. */
        void logout(const std::string& token) const;

        ///////////////////////////////////////////////
        /// Pending logins (the poll handoff)
        ///////////////////////////////////////////////

        void parkLogin(const std::string& state, const IssuedToken& issued) const;

        [[nodiscard]] std::optional<IssuedToken> claimLogin(const std::string& state) const;

    private:
        struct PendingLogin {
            IssuedToken issued;
            std::int64_t parkedAtEpochMs{};
        };

        /* Caller must hold pendingMutex_. */
        void prunePendingLocked(std::int64_t nowEpochMs) const;

        std::shared_ptr<domain::SessionRepository> sessions_;
        std::shared_ptr<steam::OpenIdVerifier> verifier_;

        mutable std::mutex pendingMutex_;
        mutable std::unordered_map<std::string, PendingLogin> pending_;
    };
} // namespace picasso::service
