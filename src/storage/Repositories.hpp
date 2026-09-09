//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "domain/ports/ChatRepository.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/SessionRepository.hpp"

namespace picasso::storage {
    /*
     * Module entry point. Everything above this layer sees domain ports and never
     * the driver - the oatpp-postgresql DbClient stops here.
     */
    struct Repositories {
        std::shared_ptr<domain::ChatRepository> chat;
        std::shared_ptr<domain::ConversationRepository> conversations;
        std::shared_ptr<domain::SessionRepository> sessions;
    };

    /*
     * Builds the Postgres-backed repositories. Constructing them is cheap and does
     * not touch the network, so the rest of the process can be wired up before the
     * database exists - the stubs throw only when a method is actually called.
     */
    Repositories makeRepositories(const std::string& dsn);

    /* Applies the SQL files under storage/migrations. Not implemented (roadmap step 4). */
    void migrate(const std::string& dsn);
} // namespace picasso::storage
