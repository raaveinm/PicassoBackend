//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

namespace picasso::dto {
    /*
     * Inbound chat. There is deliberately no sender field: the server derives the
     * sender from the authenticated connection, so a client-supplied one would be
     * either ignored or a spoofing hole. Nothing to ignore is the safer shape.
     */
    class ChatMessageInDto : public oatpp::DTO {
        DTO_INIT(ChatMessageInDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, localMessageId);
        DTO_FIELD(String, body);
    };

    /* Outbound chat - server-stamped sender and id. */
    class ChatMessageOutDto : public oatpp::DTO {
        DTO_INIT(ChatMessageOutDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, messageId);
        DTO_FIELD(String, senderSteamId);
        DTO_FIELD(String, body);
        DTO_FIELD(Int64, createdAt);
    };

    /*
     * Carries messageId as well as the echoed localMessageId: the client needs the
     * server id to resolve its PENDING row *and* to know where its cache now ends.
     */
    class ChatAckDto : public oatpp::DTO {
        DTO_INIT(ChatAckDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, localMessageId);
        DTO_FIELD(String, messageId);
    };

    class CallInviteDto : public oatpp::DTO {
        DTO_INIT(CallInviteDto, DTO)

        DTO_FIELD(String, conversationId);
    };

    /* incoming_call / call_accept / call_decline / peer_joined / call_leave. */
    class CallSignalDto : public oatpp::DTO {
        DTO_INIT(CallSignalDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, steamId);
    };

    /*
     * sdp and candidate stay opaque - the server never parses them. fromSteamId is
     * stamped by the server on the way out; a client-supplied one is discarded.
     */
    class SdpDto : public oatpp::DTO {
        DTO_INIT(SdpDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, toSteamId);
        DTO_FIELD(String, fromSteamId);
        DTO_FIELD(String, sdp);
    };

    class IceCandidateDto : public oatpp::DTO {
        DTO_INIT(IceCandidateDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, toSteamId);
        DTO_FIELD(String, fromSteamId);
        DTO_FIELD(String, candidate);
    };

    /*
     * Directed like SdpDto/IceCandidateDto (addressed by toSteamId), no extra
     * payload - the frame itself is the "hang up now" signal.
     */
    class CallHangupDto : public oatpp::DTO {
        DTO_INIT(CallHangupDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, toSteamId);
        DTO_FIELD(String, fromSteamId);
    };

    /*
     * Ids are strings on the wire: SteamIDs (~7.6e16) exceed 2^53, and one rule beats
     * two. `name` is null for a dm; `invited` is always [] for a dm. `writable` is
     * false for a dm whose pair no longer satisfies the contact rule (a frozen dm).
     */
    class ConversationDto : public oatpp::DTO {
        DTO_INIT(ConversationDto, DTO)

        DTO_FIELD(String, id);
        DTO_FIELD(String, kind);                        // "dm" | "palette"
        DTO_FIELD(String, name);
        DTO_FIELD(List<String>, members);
        DTO_FIELD(List<String>, invited);
        DTO_FIELD(Boolean, writable);
        DTO_FIELD(Int64, createdAt);                    // epoch ms
    };

    /* For incoming the steamId is the sender, for outgoing the target. */
    class ContactRequestDto : public oatpp::DTO {
        DTO_INIT(ContactRequestDto, DTO)

        DTO_FIELD(String, steamId);
        DTO_FIELD(Int64, createdAt);                    // epoch ms
    };

    class PaletteInviteDto : public oatpp::DTO {
        DTO_INIT(PaletteInviteDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, name);
        DTO_FIELD(String, inviterSteamId);
        DTO_FIELD(Int64, createdAt);                    // epoch ms
    };

    /* contact_updated: `steamId` is the OTHER person; level is null for "stranger". */
    class ContactUpdatedDto : public oatpp::DTO {
        DTO_INIT(ContactUpdatedDto, DTO)

        DTO_FIELD(String, steamId);
        DTO_FIELD(String, level);                       // "imposter" | "ally" | "friend" | null
    };

    /* palette_invite: state is "pending", or "resolved" once accepted/declined on another device. */
    class PaletteInviteEventDto : public oatpp::DTO {
        DTO_INIT(PaletteInviteEventDto, DTO)

        DTO_FIELD(String, conversationId);
        DTO_FIELD(String, name);
        DTO_FIELD(String, inviterSteamId);
        DTO_FIELD(Int64, createdAt);                    // epoch ms
        DTO_FIELD(String, state);
    };

    /* Every WS frame in either direction. `type` selects the populated payload. */
    class EnvelopeDto : public oatpp::DTO {
        DTO_INIT(EnvelopeDto, DTO)

        DTO_FIELD(String, type);

        DTO_FIELD(Object<ChatMessageInDto>, chatMessage);
        DTO_FIELD(Object<ChatMessageOutDto>, chatMessageOut);
        DTO_FIELD(Object<ChatAckDto>, chatAck);
        DTO_FIELD(Object<CallInviteDto>, callInvite);
        DTO_FIELD(Object<CallSignalDto>, callSignal);
        DTO_FIELD(Object<SdpDto>, sdp);
        DTO_FIELD(Object<IceCandidateDto>, iceCandidate);
        DTO_FIELD(Object<CallHangupDto>, callHangup);
        DTO_FIELD(Object<ContactRequestDto>, contactRequest);
        DTO_FIELD(Object<ContactUpdatedDto>, contactUpdated);
        DTO_FIELD(Object<PaletteInviteEventDto>, paletteInvite);
        DTO_FIELD(Object<ConversationDto>, conversation);
    };
} // namespace picasso::dto

#include OATPP_CODEGEN_END(DTO)
