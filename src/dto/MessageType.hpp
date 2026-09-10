//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>

namespace picasso::dto {
    /*
     * A plain enum class rather than oat++'s ENUM(...). Not a workaround - it is
     * the wire vocabulary, and keeping it free of oat++ types means the services
     * that switch on it stay linkable without the framework.
     */
    enum class MessageType {
        Unknown,

        ChatMessage,
        ChatAck,

        CallInvite,
        IncomingCall,
        CallAccept,
        CallDecline,
        PeerJoined,
        CallLeave,
        CallHangup,

        SdpOffer,
        SdpAnswer,
        IceCandidate,
    };

    /* Returns MessageType::Unknown for anything unrecognised - never throws. */
    MessageType parseMessageType(const std::string& wire);

    /* Returns an empty string for MessageType::Unknown. */
    std::string toWireString(MessageType type);
} // namespace picasso::dto
