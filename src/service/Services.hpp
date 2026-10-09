//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "domain/ports/EventSink.hpp"
#include "domain/ports/PresenceRegistry.hpp"
#include "domain/ports/SignalTransport.hpp"
#include "service/AuthService.hpp"
#include "service/CallSignalService.hpp"
#include "service/ChatService.hpp"
#include "service/ContactService.hpp"
#include "service/ConversationService.hpp"
#include "steam/OpenIdVerifier.hpp"
#include "storage/Repositories.hpp"

namespace picasso::service {
    /* Module entry point - the bundle transport/ needs and nothing more. */
    struct Services {
        std::shared_ptr<ChatService> chat;
        std::shared_ptr<CallSignalService> callSignal;
        std::shared_ptr<AuthService> auth;
        std::shared_ptr<ContactService> contacts;
        std::shared_ptr<ConversationService> conversations;
    };

    /*
     * Presence and transport are passed in rather than built here: both are
     * implemented by the WS connection hub, which lives a layer above. The services
     * only ever see the ports. Same for `events`: the WS layer owns what a frame looks like.
     */
    Services makeServices(
        const storage::Repositories& repositories,
        const std::shared_ptr<domain::PresenceRegistry>& presence,
        const std::shared_ptr<domain::SignalTransport>& transport,
        const std::shared_ptr<steam::OpenIdVerifier>& verifier,
        const std::shared_ptr<domain::EventSink>& events);
} // namespace picasso::service
