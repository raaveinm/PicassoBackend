//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/AuthService.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "domain/Errors.hpp"

namespace picasso::service {
    AuthService::AuthService(std::shared_ptr<domain::SessionRepository> sessions,
                             std::shared_ptr<steam::OpenIdVerifier> verifier)
        : sessions_(std::move(sessions)), verifier_(std::move(verifier)) {}

    std::string AuthService::beginLoginUrl() {
        notImplemented("service: AuthService::beginLoginUrl", "3: auth");
    }

    std::optional<IssuedToken> AuthService::completeLogin(const std::map<std::string, std::string>&) {
        notImplemented("service: AuthService::completeLogin", "3: auth");
    }

    std::optional<domain::SteamId> AuthService::authenticate(const std::string& token) {
        /*
         * TEMPORARY (roadmap step 3: auth): trusts the bearer token as the literal
         * SteamId, with no verification at all. This exists only to unblock the WS
         * signaling pipeline (roadmap step 5) end to end before real Steam OpenID
         * login is built. Anyone can currently claim any SteamId - do not ship this.
         *
         * `token` is the raw Authorization header value ("Bearer <steamId>"), since
         * WsController forwards it unparsed - the "Bearer " prefix is stripped here.
         */
        constexpr std::string_view prefix = "Bearer ";
        const auto digits = token.starts_with(prefix) ? token.substr(prefix.size()) : token;

        try {
            return domain::SteamId(std::stoull(digits));
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
} // namespace picasso::service
