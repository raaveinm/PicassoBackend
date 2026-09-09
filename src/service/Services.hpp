//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "domain/ports/PresenceRegistry.hpp"
#include "domain/ports/SignalTransport.hpp"
#include "service/AuthService.hpp"
#include "service/CallSignalService.hpp"
#include "service/ChatService.hpp"
#include "steam/OpenIdVerifier.hpp"
#include "storage/Repositories.hpp"

namespace picasso::service {
    /* Module entry point - the bundle transport/ needs and nothing more. */
    struct Services {
        std::shared_ptr<ChatService> chat;
        std::shared_ptr<CallSignalService> callSignal;
        std::shared_ptr<AuthService> auth;
    };

    /*
     * Presence and transport are passed in rather than built here: both are
     * implemented by the WS connection hub, which lives a layer above. The services
     * only ever see the ports.
     */
    Services makeServices(const storage::Repositories& repositories,
                          const std::shared_ptr<domain::PresenceRegistry>& presence,
                          const std::shared_ptr<domain::SignalTransport>& transport,
                          const std::shared_ptr<steam::OpenIdVerifier>& verifier);
} // namespace picasso::service
