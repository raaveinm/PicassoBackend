//
// Created by Kirill "Raaveinm" on 9/9/26.
//

// otherwise throws an compilation err
// ReSharper disable CppVariableCanBeMadeConstexpr
#pragma once

#include <map>
#include <optional>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::steam {
    const std::string TAG_OPENID{"OPEN_ID_VERIFIER"};

    /**
     * The one Steam-facing job this server has. The client talks to Steam's Web API
     * directly for everything else, but it cannot verify Steam's signature on an
     * OpenID assertion - that check has to happen here.
     */

    class OpenIdVerifier {
    public:
        explicit OpenIdVerifier(std::string publicUrl) : publicUrl_(std::move(publicUrl)) {}

        /**
         * `state` is the client's one-time login nonce. It rides along in the
         * `return_to` query string, so Steam echoes it back on the redirect and
         * `/auth/steam/return` can tell which waiting client the minted token
         * belongs to. Empty means "no nonce" (browser-only login, nothing to claim).
         */
        [[nodiscard]] std::string buildAuthUrl(const std::string& state = "") const;

        /**
         * Posts the assertion back to Steam with mode=check_authentication and
         * returns the steamId only if Steam confirms it. `std::nullopt` means the
         * assertion was forged, replayed or malformed - all indistinguishable and
         * all equally rejected.
         */

        [[nodiscard]] static std::optional<domain::SteamId> verify(const std::map<std::string, std::string>& params);

    private:
        /* realm / return_to must match what Steam saw, hence the configured public URL. */
        std::string publicUrl_;
    };
} // namespace picasso::steam
