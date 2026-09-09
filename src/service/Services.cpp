//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "service/Services.hpp"

namespace picasso::service {
    Services makeServices(const storage::Repositories& repositories,
                          const std::shared_ptr<domain::PresenceRegistry>& presence,
                          const std::shared_ptr<domain::SignalTransport>& transport,
                          const std::shared_ptr<steam::OpenIdVerifier>& verifier) {
        return Services{
            std::make_shared<ChatService>(repositories.chat,
                                          repositories.conversations,
                                          presence,
                                          transport),
            std::make_shared<CallSignalService>(repositories.conversations, presence, transport),
            std::make_shared<AuthService>(repositories.sessions, verifier),
        };
    }
} // namespace picasso::service
