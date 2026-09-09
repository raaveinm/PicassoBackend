//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "dto/MessageType.hpp"

#include <array>
#include <utility>

namespace picasso::dto {
    namespace {
        constexpr std::array<std::pair<MessageType, const char*>, 12> kWireNames{{
            {MessageType::Unknown, ""},
            {MessageType::ChatMessage, "chat_message"},
            {MessageType::ChatAck, "chat_ack"},
            {MessageType::CallInvite, "call_invite"},
            {MessageType::IncomingCall, "incoming_call"},
            {MessageType::CallAccept, "call_accept"},
            {MessageType::CallDecline, "call_decline"},
            {MessageType::PeerJoined, "peer_joined"},
            {MessageType::CallLeave, "call_leave"},
            {MessageType::SdpOffer, "sdp_offer"},
            {MessageType::SdpAnswer, "sdp_answer"},
            {MessageType::IceCandidate, "ice_candidate"},
        }};
    } // namespace

    MessageType parseMessageType(const std::string& wire) {
        for (const auto& [type, name] : kWireNames) {
            if (type != MessageType::Unknown && wire == name) {
                return type;
            }
        }
        return MessageType::Unknown;
    }

    std::string toWireString(const MessageType type) {
        for (const auto& [candidate, name] : kWireNames) {
            if (candidate == type) {
                return name;
            }
        }
        return "";
    }
} // namespace picasso::dto
