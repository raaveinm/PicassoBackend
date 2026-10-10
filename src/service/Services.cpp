//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/Services.hpp"

namespace picasso::service {
    Services makeServices(
        const storage::Repositories& repositories,
        const std::shared_ptr<domain::PresenceRegistry>& presence,
        const std::shared_ptr<domain::SignalTransport>& transport,
        const std::shared_ptr<steam::OpenIdVerifier>& verifier,
        const std::shared_ptr<domain::EventSink>& events) {

        return Services{
            .chat = std::make_shared<ChatService>(
                repositories.chat,
                repositories.conversations,
                repositories.contacts,
                events),
            .callSignal = std::make_shared<CallSignalService>(repositories.conversations, presence, transport),
            .auth = std::make_shared<AuthService>(repositories.sessions, verifier),
            .contacts = std::make_shared<ContactService>(repositories.contacts, repositories.conversations, events),
            .conversations = std::make_shared<ConversationService>(repositories.conversations, repositories.contacts, events),
        };
    }
} // namespace picasso::service
