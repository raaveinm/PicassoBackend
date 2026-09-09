//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::domain {
    /*
     * The plaintext token exists only in the response that hands it to the client.
     * What is stored, passed around and compared is always the hash - a database
     * dump must not be a set of live sessions.
     */
    struct Session {
        std::string tokenHash;
        SteamId steamId;
        std::int64_t createdAtEpochMs{};

        /*
         * Two separate reasons a session stops working. Expiry is automatic and
         * always set; revocation is an explicit logout and usually absent.
         */
        std::int64_t expiresAtEpochMs{};
        std::optional<std::int64_t> revokedAtEpochMs;

        bool isUsableAt(const std::int64_t nowEpochMs) const {
            return !revokedAtEpochMs.has_value() && nowEpochMs < expiresAtEpochMs;
        }
    };
} // namespace picasso::domain
