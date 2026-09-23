//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <optional>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::domain {
    struct Session {
        std::string token_hash;
        SteamId steam_id;
        std::int64_t created_at_epoch_ms{};
        std::int64_t expires_at_epoch_ms{};
        std::optional<std::int64_t> revoke_at_epoch_ms{};

        [[nodiscard]] bool isUsableAt(const std::int64_t nowEpochMs) const {
            return !revoke_at_epoch_ms.has_value() && nowEpochMs < expires_at_epoch_ms;
        }
    };
} // namespace picasso::domain
