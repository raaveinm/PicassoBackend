//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <map>
#include <optional>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::steam {
    /*
     * The one Steam-facing job this server has. The client talks to Steam's Web API
     * directly for everything else, but it cannot verify Steam's signature on an
     * OpenID assertion - that check has to happen here.
     */
    class OpenIdVerifier {
    public:
        explicit OpenIdVerifier(std::string publicUrl);

        /* URL to redirect the user to in order to start the Steam login. */
        std::string buildAuthUrl() const;

        /*
         * Posts the assertion back to Steam with mode=check_authentication and
         * returns the steamId only if Steam confirms it. std::nullopt means the
         * assertion was forged, replayed or malformed - all indistinguishable and
         * all equally rejected.
         */
        std::optional<domain::SteamId> verify(const std::map<std::string, std::string>& params) const;

    private:
        /* realm / return_to must match what Steam saw, hence the configured public URL. */
        std::string publicUrl_;
    };
} // namespace picasso::steam
