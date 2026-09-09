//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "domain/Ids.hpp"
#include "domain/ports/SessionRepository.hpp"
#include "steam/OpenIdVerifier.hpp"

namespace picasso::service {
    struct IssuedToken {
        /* Plaintext. Returned to the client once and never stored. */
        std::string token;
        domain::SteamId steamId;
        std::int64_t expiresAtEpochMs{};
    };

    class AuthService {
    public:
        AuthService(std::shared_ptr<domain::SessionRepository> sessions,
                    std::shared_ptr<steam::OpenIdVerifier> verifier);

        static std::string beginLoginUrl();

        /* Verifies the Steam assertion and, if it holds, mints a session token. */
        static std::optional<IssuedToken> completeLogin(const std::map<std::string, std::string>& params);

        /*
         * The single gate for the WS upgrade. Returns the identity that the session
         * will carry for its entire lifetime - every message it later sends is
         * attributed to this steamId and to nothing the client can influence.
         */
        static std::optional<domain::SteamId> authenticate(const std::string& token);

    private:
        std::shared_ptr<domain::SessionRepository> sessions_;
        std::shared_ptr<steam::OpenIdVerifier> verifier_;
    };
} // namespace picasso::service
