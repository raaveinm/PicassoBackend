//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::domain {
    /* Maps to the messages table; body/createdAtEpochMs are its text/timestamp columns. */
    struct Message {
        MessageId id;
        ConversationId conversation_id;
        SteamId sender_steam_id;
        std::string text_message;
        std::int64_t created_at_epoch_ms;
    };
} // namespace picasso::domain
