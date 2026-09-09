//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "steam/OpenIdVerifier.hpp"

#include <utility>

#include "domain/Errors.hpp"

namespace picasso::steam {
    OpenIdVerifier::OpenIdVerifier(std::string publicUrl) : publicUrl_(std::move(publicUrl)) {}

    std::string OpenIdVerifier::buildAuthUrl() const {
        notImplemented("steam: OpenIdVerifier::buildAuthUrl", "3: auth");
    }

    std::optional<domain::SteamId> OpenIdVerifier::verify(const std::map<std::string, std::string>&) const {
        /*
         * Returning std::nullopt here would be a silent "login failed" that is
         * indistinguishable from a real rejection. Throw until it is real.
         */
        notImplemented("steam: OpenIdVerifier::verify", "3: auth");
    }
} // namespace picasso::steam
