//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "domain/Contact.hpp"
#include "domain/Conversation.hpp"
#include "domain/Ids.hpp"

namespace picasso::domain {
    /**
     * Things the server tells a connected client about, expressed in domain terms.
     * The services publish these; transport/ws decides what a frame looks like. That
     * split is what keeps JSON and sockets out of service/ (@see EventSink).
     *
     * Every event names its own recipient: a block, for instance, is two different
     * events with two different payloads, one per party.
     */

    /* Someone asked `to` to become an ally. */
    struct ContactRequested {
        SteamId to;
        SteamId from;
        std::int64_t created_at_epoch_ms{};
    };

    /*
     * `to`'s row about `other` changed. nullopt means "stranger": the row is gone,
     * which is also exactly what a block looks like to the blocked person.
     */
    struct ContactChanged {
        SteamId to;
        SteamId other;
        std::optional<ContactLevel> level;
    };

    /* An invitation to a palette was issued to `to`, or resolved on another device. */
    struct PaletteInviteChanged {
        SteamId to;
        PaletteInvite invite;
        bool pending{true};
    };

    /* `to` now has this conversation (a dm was created with them, or they joined a palette). */
    struct ConversationAdded {
        SteamId to;
        Conversation conversation;
    };

    /* A conversation `to` is already in changed, e.g. someone joined. */
    struct ConversationUpdated {
        SteamId to;
        Conversation conversation;
    };

    using Event = std::variant<
        ContactRequested,
        ContactChanged,
        PaletteInviteChanged,
        ConversationAdded,
        ConversationUpdated>;
} // namespace picasso::domain
