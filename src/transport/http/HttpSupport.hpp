//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "oatpp/core/Types.hpp"
#include "oatpp/web/protocol/http/Http.hpp"
#include "oatpp/web/protocol/http/incoming/Request.hpp"
#include "oatpp/web/protocol/http/outgoing/ResponseFactory.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include "domain/Contact.hpp"
#include "domain/Conversation.hpp"
#include "domain/Ids.hpp"
#include "dto/Mappers.hpp"
#include "dto/Rest.hpp"
#include "service/AuthService.hpp"
#include "transport/http/ErrorHandler.hpp"

namespace picasso::transport::http {
    ///////////////////////////////////////////////
    /// Identity
    ///////////////////////////////////////////////

    /**
     * The REST twin of the WS upgrade gate: the caller's identity comes from the
     * session token and from nothing in the body or the URL. Missing, malformed,
     * expired and revoked all answer the same 401.
     */
    inline domain::SteamId requireIdentity(
        const service::AuthService& auth,
        const std::shared_ptr<oatpp::web::protocol::http::incoming::Request>& request
    ) {
        const auto header = request->getHeader("Authorization");
        const auto identity = header ? auth.authenticate(*header) : std::nullopt;
        OATPP_ASSERT_HTTP(identity.has_value(),
                          oatpp::web::protocol::http::Status::CODE_401,
                          "missing or invalid session token")
        return *identity;
    }

    ///////////////////////////////////////////////
    /// Parsing
    ///////////////////////////////////////////////

    /* Decimal digits only: stoull would happily accept "-1" or " 12abc". Never throws. */
    inline std::optional<std::uint64_t> parseUnsigned(const std::string& text) {
        if (text.empty() || text.size() > 19) return std::nullopt;
        std::uint64_t value = 0;
        for (const char c : text) {
            if (c < '0' || c > '9') return std::nullopt;
            value = value * 10 + static_cast<std::uint64_t>(c - '0');
        }
        return value;
    }

    inline std::optional<domain::SteamId> parseSteamId(const oatpp::String& text) {
        if (!text) return std::nullopt;
        const auto value = parseUnsigned(*text);
        if (!value) return std::nullopt;
        return domain::SteamId(*value);
    }

    inline std::optional<domain::ConversationId> parseConversationId(const oatpp::String& text) {
        if (!text) return std::nullopt;
        const auto value = parseUnsigned(*text);
        if (!value || *value > static_cast<std::uint64_t>(INT64_MAX)) return std::nullopt;
        return domain::ConversationId(static_cast<std::int64_t>(*value));
    }

    ///////////////////////////////////////////////
    /// Domain -> DTO
    ///////////////////////////////////////////////

    // The mappers shared with the WS side live in dto/Mappers.hpp. ADL won't find them
    // from here (their arguments are domain types), so name them once.
    using dto::toDto;
    using dto::wire;

    inline oatpp::Object<dto::ContactsDto> toDto(const domain::ContactsSnapshot& snapshot) {
        auto body = dto::ContactsDto::createShared();
        body->contacts = oatpp::List<oatpp::Object<dto::ContactDto>>::createShared();
        body->incoming = oatpp::List<oatpp::Object<dto::ContactRequestDto>>::createShared();
        body->outgoing = oatpp::List<oatpp::Object<dto::ContactRequestDto>>::createShared();
        body->paletteInvites = oatpp::List<oatpp::Object<dto::PaletteInviteDto>>::createShared();

        for (const auto& contact : snapshot.contacts) {
            auto row = dto::ContactDto::createShared();
            row->steamId = wire(contact.other);
            row->level = oatpp::String(std::string(domain::toWireString(contact.level)));
            row->since = contact.since_epoch_ms;
            body->contacts->push_back(row);
        }
        for (const auto& request : snapshot.incoming) { body->incoming->push_back(toDto(request)); }
        for (const auto& request : snapshot.outgoing) { body->outgoing->push_back(toDto(request)); }
        for (const auto& invite : snapshot.palette_invites) { body->paletteInvites->push_back(toDto(invite)); }
        return body;
    }
} // namespace picasso::transport::http
