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
        /* The client's outbox row id, echoed back in the ack. */
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
    };
} // namespace picasso::dto

#include OATPP_CODEGEN_END(DTO)
