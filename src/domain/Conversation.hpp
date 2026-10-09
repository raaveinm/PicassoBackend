//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/Ids.hpp"

namespace picasso::domain {
    enum class ConversationKind {
        Dm,
        Palette,
    };

    inline std::string_view toWireString(const ConversationKind kind) {
        return kind == ConversationKind::Dm ? "dm" : "palette";
    }

    inline std::optional<ConversationKind> parseConversationKind(const std::string_view wire) {
        if (wire == "dm") return ConversationKind::Dm;
        if (wire == "palette") return ConversationKind::Palette;
        return std::nullopt;
    }

    struct Conversation {
        ConversationId id;
        ConversationKind kind{ConversationKind::Dm};
        std::optional<std::string> name;        // palette only; a dm has none
        std::vector<SteamId> members;           // actual members only - a pending invitee is NOT here
        std::vector<SteamId> invited;           // pending invitations (palette only, empty for a dm)
        std::int64_t created_at_epoch_ms{};
    };
} // namespace picasso::domain
