//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include "dto/Envelope.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

namespace picasso::dto {
    /* Uniform body for every non-2xx response. See transport/http/ErrorHandler. */
    class ErrorDto : public oatpp::DTO {
        DTO_INIT(ErrorDto, DTO)

        DTO_FIELD(Int32, status);
        DTO_FIELD(String, code);
        DTO_FIELD(String, message);
    };

    /* Handed to the client once, after a Steam OpenID assertion checks out. */
    class AuthTokenDto : public oatpp::DTO {
        DTO_INIT(AuthTokenDto, DTO)

        DTO_FIELD(String, token);
        DTO_FIELD(String, steamId);
        DTO_FIELD(Int64, expiresAt);
    };

    /* POST /conversations. A dm uses peerSteamId; a palette uses name + inviteSteamIds. */
    class CreateConversationDto : public oatpp::DTO {
        DTO_INIT(CreateConversationDto, DTO)

        DTO_FIELD(String, kind);
        DTO_FIELD(String, peerSteamId);
        DTO_FIELD(String, name);
        DTO_FIELD(List<String>, inviteSteamIds);
    };

    /* POST /contacts/requests and POST /conversations/{id}/invites. */
    class SteamIdBodyDto : public oatpp::DTO {
        DTO_INIT(SteamIdBodyDto, DTO)

        DTO_FIELD(String, steamId);
    };

    /* PUT /contacts/{steamId}: "ally" | "friend" | "imposter". */
    class ContactLevelBodyDto : public oatpp::DTO {
        DTO_INIT(ContactLevelBodyDto, DTO)

        DTO_FIELD(String, level);
    };

    class ContactDto : public oatpp::DTO {
        DTO_INIT(ContactDto, DTO)

        DTO_FIELD(String, steamId);
        DTO_FIELD(String, level);
        DTO_FIELD(Int64, since);                        // epoch ms
    };

    /* GET /contacts - the caller's own rows only, including their blocklist. */
    class ContactsDto : public oatpp::DTO {
        DTO_INIT(ContactsDto, DTO)

        DTO_FIELD(List<Object<ContactDto>>, contacts);
        DTO_FIELD(List<Object<ContactRequestDto>>, incoming);
        DTO_FIELD(List<Object<ContactRequestDto>>, outgoing);
        DTO_FIELD(List<Object<PaletteInviteDto>>, paletteInvites);
    };

    /*
     * Cache sync payload. nextAfter is what the client passes as ?after= next time;
     * null means it has caught up with the server.
     */
    class MessagePageDto : public oatpp::DTO {
        DTO_INIT(MessagePageDto, DTO)

        DTO_FIELD(List<Object<ChatMessageOutDto>>, messages);
        DTO_FIELD(String, nextAfter);
    };
} // namespace picasso::dto

#include OATPP_CODEGEN_END(DTO)
