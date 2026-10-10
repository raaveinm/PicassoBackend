//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <string>

#include "oatpp/core/Types.hpp"

#include "domain/Contact.hpp"
#include "domain/Conversation.hpp"
#include "domain/Message.hpp"
#include "domain/Ids.hpp"
#include "dto/Envelope.hpp"

namespace picasso::dto {

    inline oatpp::String wire(const domain::SteamId id) {
        return oatpp::String(std::to_string(id.value())); // NOLINT(*-return-braced-init-list)
    }

    ///////////////////////////////////////////////
    /// Domain -> DTO
    ///////////////////////////////////////////////

    inline oatpp::Object<ConversationDto> toDto(const domain::Conversation& conversation,
                                                     const bool writable = true) {
        auto body = ConversationDto::createShared();
        body->id = oatpp::String(std::to_string(conversation.id.value()));
        body->kind = oatpp::String(std::string(domain::toWireString(conversation.kind)));
        if (conversation.name) { body->name = oatpp::String(*conversation.name); }
        body->members = oatpp::List<oatpp::String>::createShared();
        for (const auto& member : conversation.members) { body->members->push_back(wire(member)); }
        body->invited = oatpp::List<oatpp::String>::createShared();
        for (const auto& invitee : conversation.invited) { body->invited->push_back(wire(invitee)); }
        body->writable = writable;
        body->createdAt = conversation.created_at_epoch_ms;
        return body;
    }

    inline oatpp::Object<dto::MessageDto> toDto(const domain::Message& message) {
        auto body = dto::MessageDto::createShared();
        body->id = oatpp::String(std::to_string(message.id.value()));
        body->conversationId = oatpp::String(std::to_string(message.conversation_id.value()));
        body->senderSteamId = wire(message.sender_steam_id);
        body->clientMessageId = oatpp::String(message.client_message_id);
        body->body = oatpp::String(message.text_message);
        body->createdAt = message.created_at_epoch_ms;
        return body;
    }

    inline oatpp::Object<dto::PaletteInviteDto> toDto(const domain::PaletteInvite& invite) {
        auto body = dto::PaletteInviteDto::createShared();
        body->conversationId = oatpp::String(std::to_string(invite.conversation_id.value()));
        body->name = oatpp::String(invite.name);
        body->inviterSteamId = wire(invite.inviter);
        body->createdAt = invite.created_at_epoch_ms;
        return body;
    }

    inline oatpp::Object<dto::ContactRequestDto> toDto(const domain::ContactRequest& request) {
        auto body = dto::ContactRequestDto::createShared();
        body->steamId = wire(request.other);
        body->createdAt = request.created_at_epoch_ms;
        return body;
    }

} // namespace picasso::dto
