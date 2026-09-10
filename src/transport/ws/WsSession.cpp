//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/ws/WsSession.hpp"

#include <optional>
#include <string>
#include <utility>

#include "oatpp/core/base/Environment.hpp"

#include "dto/MessageType.hpp"

namespace picasso::transport::ws {
    namespace {
        /* Parses a decimal id string off a DTO field. Never throws - nullopt on anything malformed. */
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
                    OATPP_LOGD("WsSession", "signaling frame from %lu", steamId_.value());
                    break;

                case dto::MessageType::SdpOffer:
                case dto::MessageType::SdpAnswer: {
                    const auto& sdp = envelope->sdp;
                    const auto to = sdp ? parseSteamId(sdp->toSteamId) : std::nullopt;
                    const auto conversationId = sdp ? parseConversationId(sdp->conversationId) : std::nullopt;
                    if (!sdp || !to || !conversationId) {
                        OATPP_LOGD("WsSession", "dropping malformed sdp frame from %lu", steamId_.value());
                        break;
                    }
                    /* fromSteamId is server-stamped, not trusted from the client. */
                    sdp->fromSteamId = oatpp::String(std::to_string(steamId_.value()).c_str());
                    auto outEnvelope = EnvelopeCodec::envelopeOf(type);
                    outEnvelope->sdp = sdp;
                    services_.callSignal->relayToPeer(steamId_, *conversationId, *to, codec_->encode(outEnvelope));
                    break;
                }

                case dto::MessageType::IceCandidate: {
                    const auto& candidate = envelope->iceCandidate;
                    const auto to = candidate ? parseSteamId(candidate->toSteamId) : std::nullopt;
                    const auto conversationId =
                        candidate ? parseConversationId(candidate->conversationId) : std::nullopt;
                    if (!candidate || !to || !conversationId) {
                        OATPP_LOGD("WsSession", "dropping malformed ice frame from %lu", steamId_.value());
                        break;
                    }
                    candidate->fromSteamId = oatpp::String(std::to_string(steamId_.value()).c_str());
                    auto outEnvelope = EnvelopeCodec::envelopeOf(type);
                    outEnvelope->iceCandidate = candidate;
                    services_.callSignal->relayToPeer(steamId_, *conversationId, *to, codec_->encode(outEnvelope));
                    break;
                }

                case dto::MessageType::CallHangup: {
                    const auto& hangup = envelope->callHangup;
                    const auto to = hangup ? parseSteamId(hangup->toSteamId) : std::nullopt;
                    const auto conversationId = hangup ? parseConversationId(hangup->conversationId) : std::nullopt;
                    if (!hangup || !to || !conversationId) {
                        OATPP_LOGD("WsSession", "dropping malformed hangup frame from %lu", steamId_.value());
                        break;
                    }
                    hangup->fromSteamId = oatpp::String(std::to_string(steamId_.value()).c_str());
                    auto outEnvelope = EnvelopeCodec::envelopeOf(type);
                    outEnvelope->callHangup = hangup;
                    services_.callSignal->relayToPeer(steamId_, *conversationId, *to, codec_->encode(outEnvelope));
                    break;
                }

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
