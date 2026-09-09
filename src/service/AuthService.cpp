//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/AuthService.hpp"

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

    std::optional<domain::SteamId> AuthService::authenticate(const std::string&) {
        /*
         * Throws rather than returning nullopt on purpose. nullopt here means
         * "token rejected", and a stub that returns it would look like a working
         * auth check that denies everyone - or, worse, would be "fixed" later by
         * someone making it return a value. Failing loudly cannot be mistaken.
         */
        notImplemented("service: AuthService::authenticate", "3: auth");
    }
} // namespace picasso::service
