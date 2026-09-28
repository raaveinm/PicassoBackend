//
// Created by Kirill "Raaveinm" on 9/9/26.
//

// otherwise throws an compilation err
// ReSharper disable CppVariableCanBeMadeConstexpr
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

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

        [[nodiscard]] std::string beginLoginUrl() const;

        // Verifies the Steam assertion, mints a session token
        [[nodiscard]]std::optional<IssuedToken> completeLogin(const std::map<std::string, std::string>& params) const;

        /**
         * The single gate for the WS upgrade. Returns the identity that the session
         * will carry for its entire lifetime - every message it later sends is
         * attributed to this steamId and to nothing the client can influence.
         */
        [[nodiscard]]std::optional<domain::SteamId> authenticate(const std::string& token) const;

    private:
        std::shared_ptr<domain::SessionRepository> sessions_;
        std::shared_ptr<steam::OpenIdVerifier> verifier_;
    };
} // namespace picasso::service
