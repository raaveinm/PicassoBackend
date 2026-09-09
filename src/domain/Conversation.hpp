//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::domain {
    enum class ConversationKind {
        Dm,
        Palette,
    };

    struct Conversation {
        ConversationId id;
        ConversationKind kind{ConversationKind::Dm};
        std::string title;
        SteamId createdBy;
        std::int64_t createdAtEpochMs{};
    };
} // namespace picasso::domain
