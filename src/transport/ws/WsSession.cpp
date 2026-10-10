//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/ws/WsSession.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "WsModule.hpp"
#include "oatpp/core/base/Environment.hpp"

#include "dto/Mappers.hpp"
#include "dto/MessageType.hpp"

namespace picasso::transport::ws {
    namespace {
        /* WebSocket close code 1013 "try again later": what a client too slow for its queue is told. */
        constexpr v_uint16 CLOSE_TRY_AGAIN_LATER = 1013;

        std::atomic<std::uint64_t> NEXT_CONNECTION_ID{1};

        std::optional<domain::SteamId> parseSteamId(const oatpp::String& value) {
            if (!value) return std::nullopt;
            try {
                return domain::SteamId(std::stoull(*value));
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }

        std::optional<domain::ConversationId> parseConversationId(const oatpp::String& value) {
            if (!value) return std::nullopt;
            try {
                return domain::ConversationId(static_cast<std::int64_t>(std::stoll(*value)));
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    } // namespace

    WsSession::WsSession(
        const domain::SteamId steam_id,
        const oatpp::websocket::WebSocket* socket,
        std::shared_ptr<EnvelopeCodec> codec,
        service::Services services
    )
        : steam_id_(steam_id),
          connection_id_(NEXT_CONNECTION_ID.fetch_add(1)),
          socket_(socket),
          codec_(std::move(codec)),
          services_(std::move(services)) {
        writer_ = std::thread([this] { writerLoop(); });
    }

    WsSession::~WsSession() {
        shutdown();
    }

    void WsSession::shutdown() {
        queue_.close();
        if (writer_.joinable() && writer_.get_id() != std::this_thread::get_id()) {
            writer_.join();
        }
    }

    ///////////////////////////////////////////////
    /// Writing
    ///////////////////////////////////////////////

    void WsSession::send(const std::string& frame) { queue_.push(frame); }

    void WsSession::writerLoop() {
        while (true) {
            auto [kind, data] = queue_.pop();
            try {
                switch (kind) {
                    case BoundedFrameQueue::Item::Kind::Closed:
                        return;
                    case BoundedFrameQueue::Item::Kind::Overflow:
                        // Too far behind to catch up on this connection. It reconnects and runs POST /sync.
                        OATPP_LOGW(WS_MODULE_TAG, "closing slow consumer %lu", steam_id_.value());
                        socket_->sendClose(CLOSE_TRY_AGAIN_LATER, "slow consumer");
                        return;
                    case BoundedFrameQueue::Item::Kind::Pong:
                        socket_->sendPong(oatpp::String(data));
                        break;
                    case BoundedFrameQueue::Item::Kind::Frame:
                        socket_->sendOneFrameText(oatpp::String(data));
                        break;
                }
            } catch (const std::exception&) {
                // The peer is gone; the reader notices on its own and tears the session down.
                return;
            }
        }
    }

    ///////////////////////////////////////////////
    /// Reading
    ///////////////////////////////////////////////

    void WsSession::onPing(const WebSocket&, const oatpp::String& message) {
        queue_.pushPong(message ? *message : std::string());
    }

    void WsSession::onPong(const WebSocket&, const oatpp::String&) {
        /* Nothing to do - liveness tracking is not wired up yet. */
    }

    void WsSession::onClose(const WebSocket&, const v_uint16, const oatpp::String&) {
        /* Stops the writer from touching a socket the handler is about to tear down. */
        queue_.close();
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

    ///////////////////////////////////////////////
    /// Chat
    ///////////////////////////////////////////////

    void WsSession::sendNack(
        const oatpp::String& conversation_id,
        const oatpp::String& client_message_id,
        const std::string& code
    ) {
        const auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::ChatNack);
        envelope->chatNack = dto::ChatNackDto::createShared();
        envelope->chatNack->conversationId = conversation_id;
        envelope->chatNack->clientMessageId = client_message_id;
        envelope->chatNack->code = oatpp::String(code);
        send(codec_->encode(envelope));
    }

    void WsSession::handleChatMessage(const oatpp::Object<dto::EnvelopeDto>& envelope) {
        const auto& chat = envelope->chatMessage;
        const auto conversation_id = chat ? parseConversationId(chat->conversationId) : std::nullopt;
        if (!chat || !conversation_id || !chat->clientMessageId || !chat->body) {
            sendNack(chat ? chat->conversationId : oatpp::String(), chat ? chat->clientMessageId : oatpp::String(), "invalid");
            return;
        }

        /*
         * Note steam_id_ as the first argument. That is the whole enforcement: the sender comes
         * from this session, and ChatMessageInDto has no field that could override it.
         */
        const auto [status, message, code] = services_.chat->submit(
            steam_id_,
            *conversation_id,
            *chat->clientMessageId,
            *chat->body,
            connection_id_);

        if (status == service::ChatService::SubmitStatus::Rejected) {
            sendNack(chat->conversationId, chat->clientMessageId, std::string(service::toWireString(code)));
            return;
        }

        // Answered on THIS connection only; everyone else was told by the service through the event sink.
        const auto ack = EnvelopeCodec::envelopeOf(dto::MessageType::ChatAck);
        ack->chatAck = dto::ChatAckDto::createShared();
        ack->chatAck->conversationId = chat->conversationId;
        ack->chatAck->clientMessageId = chat->clientMessageId;
        ack->chatAck->message = dto::toDto(*message);
        ack->chatAck->duplicate = status == service::ChatService::SubmitStatus::Duplicate;
        send(codec_->encode(ack));
    }

    void WsSession::dispatch(const std::string& frame) {
        const auto envelope = codec_->decode(frame);
        if (!envelope || !envelope->type) {
            OATPP_LOGD(WS_MODULE_TAG, "dropping malformed frame from %lu", steam_id_.value());
            return;
        }

        const auto type = dto::parseMessageType(*envelope->type);

        try {
            switch (type) {
                case dto::MessageType::ChatMessage:
                    handleChatMessage(envelope);
                    break;

                case dto::MessageType::CallInvite:
                    OATPP_LOGD(WS_MODULE_TAG, "signaling frame from %lu", steam_id_.value());
                    break;

                case dto::MessageType::SdpOffer:
                case dto::MessageType::SdpAnswer: {
                    const auto& sdp = envelope->sdp;
                    const auto to = sdp ? parseSteamId(sdp->toSteamId) : std::nullopt;
                    const auto conversation_id = sdp ? parseConversationId(sdp->conversationId) : std::nullopt;
                    if (!sdp || !to || !conversation_id) {
                        OATPP_LOGD(WS_MODULE_TAG, "dropping malformed sdp frame from %lu", steam_id_.value());
                        break;
                    }
                    sdp->fromSteamId = oatpp::String(std::to_string(steam_id_.value()).c_str());
                    const auto out_envelope = EnvelopeCodec::envelopeOf(type);
                    out_envelope->sdp = sdp;
                    services_.callSignal->relayToPeer(steam_id_, *conversation_id, *to, codec_->encode(out_envelope));
                    break;
                }

                case dto::MessageType::IceCandidate: {
                    const auto& candidate = envelope->iceCandidate;
                    const auto to = candidate ? parseSteamId(candidate->toSteamId) : std::nullopt;
                    const auto conversation_id =
                        candidate ? parseConversationId(candidate->conversationId) : std::nullopt;
                    if (!candidate || !to || !conversation_id) {
                        OATPP_LOGD(WS_MODULE_TAG, "dropping malformed ice frame from %lu", steam_id_.value());
                        break;
                    }
                    candidate->fromSteamId = oatpp::String(std::to_string(steam_id_.value()).c_str());
                    const auto out_envelope = EnvelopeCodec::envelopeOf(type);
                    out_envelope->iceCandidate = candidate;
                    services_.callSignal->relayToPeer(steam_id_, *conversation_id, *to, codec_->encode(out_envelope));
                    break;
                }

                case dto::MessageType::CallHangup: {
                    const auto& hangup = envelope->callHangup;
                    const auto to = hangup ? parseSteamId(hangup->toSteamId) : std::nullopt;
                    const auto conversation_id = hangup ? parseConversationId(hangup->conversationId) : std::nullopt;
                    if (!hangup || !to || !conversation_id) {
                        OATPP_LOGD(WS_MODULE_TAG, "dropping malformed hangup frame from %lu", steam_id_.value());
                        break;
                    }
                    hangup->fromSteamId = oatpp::String(std::to_string(steam_id_.value()).c_str());
                    const auto out_envelope = EnvelopeCodec::envelopeOf(type);
                    out_envelope->callHangup = hangup;
                    services_.callSignal->relayToPeer(steam_id_, *conversation_id, *to, codec_->encode(out_envelope));
                    break;
                }

                default:
                    OATPP_LOGD(WS_MODULE_TAG, "unhandled frame type from %lu", steam_id_.value());
                    break;
            }
        } catch (const std::exception& error) {
            OATPP_LOGE(WS_MODULE_TAG, "frame handling failed: %s", error.what());
        }
    }
} // namespace picasso::transport::ws
