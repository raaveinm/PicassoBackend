//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "domain/Ids.hpp"

namespace picasso::domain {
    inline constexpr std::size_t MAX_BODY_BYTES = 4096;
    inline constexpr std::size_t MAX_CLIENT_MESSAGE_ID_BYTES = 64;

    inline constexpr int DEFAULT_PAGE_SIZE = 100;
    inline constexpr int MAX_PAGE_SIZE = 200;

    inline constexpr double CHAT_RATE_BURST = 10.0;                     // messages a user may send back to back
    inline constexpr double CHAT_RATE_PER_MINUTE = 30.0;                // ...and the sustained rate after that

    // How far behind \"now\" the deletedCursor of POST /sync is placed, so a delete is re-sent rather than missed
    inline constexpr std::int64_t DELETED_CURSOR_OVERLAP_MS = 30'000;

    // Maps to the message_data table; text_message / created_at_epoch_ms are its text / timestamp columns
    struct Message {
        MessageId id;
        ConversationId conversation_id;
        SteamId sender_steam_id;
        std::string client_message_id;  // the sender's idempotency key; lets a device recognise its own message
        std::string text_message;       // empty for a deleted message, which is never handed out anyway
        std::int64_t created_at_epoch_ms;
    };
} // namespace picasso::domain
