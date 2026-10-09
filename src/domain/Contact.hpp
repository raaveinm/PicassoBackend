//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/Ids.hpp"

namespace picasso::domain {
    ///////////////////////////////////////////////
    /// Policy
    ///////////////////////////////////////////////

    inline constexpr std::int64_t CONTACT_REQUEST_TTL_MS = 30LL * 24 * 60 * 60 * 1000;
    inline constexpr std::int64_t PALETTE_INVITE_TTL_MS = CONTACT_REQUEST_TTL_MS;
    inline constexpr std::int64_t DECLINE_COOLDOWN_MS = 7LL * 24 * 60 * 60 * 1000;
    inline constexpr std::int64_t RATE_WINDOW_MS = 24LL * 60 * 60 * 1000;
    inline constexpr int MAX_REQUESTS_PER_DAY = 20;
    inline constexpr int MAX_PENDING_REQUESTS = 50;
    inline constexpr std::size_t MAX_PALETTE_NAME_BYTES = 64;
    inline constexpr std::size_t MAX_INVITEES_PER_CREATE = 50;

    ///////////////////////////////////////////////
    /// Levels
    ///////////////////////////////////////////////

    /*
     * What the OWNER regards the other person as. "Stranger" is not a value: it is the
     * absence of a row, and is never stored (see contacts in 0001_init.sql).
     *
     * Friend behaves like Ally today - nothing reads it yet. The planned use is "this
     * person may add me to a palette without my confirmation".
     */
    enum class ContactLevel {
        Imposter,
        Ally,
        Friend,
    };

    inline std::string_view toWireString(const ContactLevel level) {
        switch (level) {
            case ContactLevel::Imposter: return "imposter";
            case ContactLevel::Ally: return "ally";
            case ContactLevel::Friend: return "friend";
        }
        return "imposter";
    }

    inline std::optional<ContactLevel> parseContactLevel(const std::string_view wire) {
        if (wire == "imposter") return ContactLevel::Imposter;
        if (wire == "ally") return ContactLevel::Ally;
        if (wire == "friend") return ContactLevel::Friend;
        return std::nullopt;
    }

    inline bool allowsCommunication(const ContactLevel level) {
        return level == ContactLevel::Ally || level == ContactLevel::Friend;
    }

    struct Contact {
        SteamId other;
        ContactLevel level{ContactLevel::Ally};
        std::int64_t since_epoch_ms{};
    };

    struct ContactRequest {
        SteamId other;
        std::int64_t created_at_epoch_ms{};
    };

    struct PaletteInvite {
        ConversationId conversation_id;
        std::string name;
        SteamId inviter;
        std::int64_t created_at_epoch_ms{};
    };

    struct ContactsSnapshot {
        std::vector<Contact> contacts;
        std::vector<ContactRequest> incoming;
        std::vector<ContactRequest> outgoing;
        std::vector<PaletteInvite> palette_invites;
    };
} // namespace picasso::domain
