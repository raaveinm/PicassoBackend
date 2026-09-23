//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>

#include "domain/ports/ChatRepository.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/SessionRepository.hpp"

namespace picasso::storage {
    /**
     * Module entry point. Everything above this layer sees domain ports and never
     * the driver - the oatpp-postgresql DbClient stops here.
     */
    struct Repositories {
        std::shared_ptr<domain::ChatRepository> chat;
        std::shared_ptr<domain::ConversationRepository> conversations;
        std::shared_ptr<domain::SessionRepository> sessions;
    };
} // namespace picasso::storage
