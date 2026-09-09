//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/ws/WsSession.hpp"

#include <utility>

#include "oatpp/core/base/Environment.hpp"

#include "dto/MessageType.hpp"

namespace picasso::transport::ws {
    WsSession::WsSession(const domain::SteamId steamId,
                         const oatpp::websocket::WebSocket* socket,
                         std::shared_ptr<EnvelopeCodec> codec,
                         service::Services services)
        : steamId_(steamId),
          socket_(socket),
          codec_(std::move(codec)),
          services_(std::move(services)) {}

    void WsSession::send(const std::string& frame) {
        const std::lock_guard lock(sendMutex_);
        if (socket_ != nullptr) {
            socket_->sendOneFrameText(oatpp::String(frame));
        }
    }

    void WsSession::onPing(const WebSocket& socket, const oatpp::String& message) {
        socket.sendPong(message);
    }

    void WsSession::onPong(const WebSocket&, const oatpp::String&) {
        /* Nothing to do - liveness tracking is not wired up yet. */
    }

    void WsSession::onClose(const WebSocket&, const v_uint16, const oatpp::String&) {
        const std::lock_guard lock(sendMutex_);
        /* Stops send() from touching a socket the handler is about to tear down. */
        socket_ = nullptr;
    }

    void WsSession::readMessage(const WebSocket&, v_uint8, p_char8 data, const oatpp::v_io_size size) {
        if (size == 0) {
            /* size == 0 marks the end of a message - everything buffered is one frame. */
            const auto frame = inbound_.toString();
            inbound_.setCurrentPosition(0);
            if (frame && !frame->empty()) {
                dispatch(*frame);
            }
            return;
        }

        if (size > 0) {
            inbound_.writeSimple(data, size);
        }
    }

    void WsSession::dispatch(const std::string& frame) {
        const auto envelope = codec_->decode(frame);
        if (!envelope || !envelope->type) {
            OATPP_LOGD("WsSession", "dropping malformed frame from %lu", steamId_.value());
            return;
        }

        const auto type = dto::parseMessageType(*envelope->type);

        try {
            switch (type) {
                case dto::MessageType::ChatMessage:
                    /*
                     * Note steamId_ as the first argument. That is the whole
                     * enforcement: the sender comes from this session, and
                     * ChatMessageInDto has no field that could override it.
                     */
                    OATPP_LOGD("WsSession", "chat_message from %lu", steamId_.value());
                    break;

                case dto::MessageType::CallInvite:
                case dto::MessageType::SdpOffer:
                case dto::MessageType::SdpAnswer:
                case dto::MessageType::IceCandidate:
                    OATPP_LOGD("WsSession", "signaling frame from %lu", steamId_.value());
                    break;

                default:
                    OATPP_LOGD("WsSession", "unhandled frame type from %lu", steamId_.value());
                    break;
            }
        } catch (const std::exception& error) {
            /*
             * A throwing service must not take the connection down with it - the
             * stubs below this point all throw until their roadmap step lands.
             */
            OATPP_LOGE("WsSession", "frame handling failed: %s", error.what());
        }
    }
} // namespace picasso::transport::ws
