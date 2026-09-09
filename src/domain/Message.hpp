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
        ConversationId conversationId;
        SteamId senderSteamId;
        std::string body;
        std::int64_t createdAtEpochMs{};
    };
} // namespace picasso::domain
