//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include "transport/ws/WsEventSink.hpp"

#include <exception>
#include <string>
#include <utility>
#include <variant>

#include "oatpp/core/base/Environment.hpp"

#include "dto/Mappers.hpp"
#include "dto/MessageType.hpp"

namespace picasso::transport::ws {
    namespace {
        const std::string TAG{"WS_EVENT_SINK"};

        /* std::visit with several lambdas. */
        template <typename... Ts>
        struct Overloaded : Ts... {
            using Ts::operator()...;
        };
        template <typename... Ts>
        Overloaded(Ts...) -> Overloaded<Ts...>;
    } // namespace

    WsEventSink::WsEventSink(std::shared_ptr<EnvelopeCodec> codec, std::shared_ptr<domain::SignalTransport> transport)
        : codec_(std::move(codec)), transport_(std::move(transport)) {}

    void WsEventSink::publish(const domain::Event& event) {
        try {
            std::visit(Overloaded{
                [this](const domain::ContactRequested& e) {
                    auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::ContactRequest);
                    envelope->contactRequest = dto::ContactRequestDto::createShared();
                    envelope->contactRequest->steamId = dto::wire(e.from);
                    envelope->contactRequest->createdAt = e.created_at_epoch_ms;
                    transport_->sendTo(e.to, codec_->encode(envelope));
                },
                [this](const domain::ContactChanged& e) {
                    auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::ContactUpdated);
                    envelope->contactUpdated = dto::ContactUpdatedDto::createShared();
                    envelope->contactUpdated->steamId = dto::wire(e.other);

                    if (e.level) {
                        envelope->contactUpdated->level = oatpp::String(std::string(domain::toWireString(*e.level)));
                    }
                    transport_->sendTo(e.to, codec_->encode(envelope));
                },
                [this](const domain::PaletteInviteChanged& e) {
                    auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::PaletteInvite);
                    envelope->paletteInvite = dto::PaletteInviteEventDto::createShared();
                    envelope->paletteInvite->conversationId = oatpp::String(std::to_string(e.invite.conversation_id.value()));
                    envelope->paletteInvite->name = oatpp::String(e.invite.name);
                    envelope->paletteInvite->inviterSteamId = dto::wire(e.invite.inviter);
                    envelope->paletteInvite->createdAt = e.invite.created_at_epoch_ms;
                    envelope->paletteInvite->state = e.pending ? "pending" : "resolved";
                    transport_->sendTo(e.to, codec_->encode(envelope));
                },
                [this](const domain::ConversationAdded& e) {
                    auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::ConversationAdded);
                    envelope->conversation = dto::toDto(e.conversation);
                    transport_->sendTo(e.to, codec_->encode(envelope));
                },
                [this](const domain::ConversationUpdated& e) {
                    auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::ConversationUpdated);
                    envelope->conversation = dto::toDto(e.conversation);
                    transport_->sendTo(e.to, codec_->encode(envelope));
                },
                [this](const domain::ChatDelivered& e) {
                    const auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::ChatMessageOut);
                    envelope->chatMessageOut = dto::ChatMessageOutDto::createShared();
                    envelope->chatMessageOut->message = dto::toDto(e.message);
                    const auto frame = codec_->encode(envelope);
                    if (e.except) {
                        transport_->sendToAllExcept(e.to, frame, *e.except);
                    } else {
                        transport_->sendToAll(e.to, frame);
                    }
                },
                [this](const domain::MessageRemoved& e) {
                    const auto envelope = EnvelopeCodec::envelopeOf(dto::MessageType::MessageDeleted);
                    envelope->messageDeleted = dto::MessageDeletedDto::createShared();
                    envelope->messageDeleted->conversationId = oatpp::String(std::to_string(e.conversation_id.value()));
                    envelope->messageDeleted->messageId = oatpp::String(std::to_string(e.message_id.value()));
                    transport_->sendToAll(e.to, codec_->encode(envelope));
                },
            }, event);
        } catch (const std::exception& error) {
            OATPP_LOGW(TAG, "dropping event: %s", error.what());
        }
    }
} // namespace picasso::transport::ws
