//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <optional>
#include <string>

#include "domain/Session.hpp"

namespace picasso::domain {
    class SessionRepository {
    public:
        virtual ~SessionRepository() = default;

        virtual void store(const Session& session) = 0;

        /* Lookup is by hash only - the plaintext token never reaches this layer. */
        virtual std::optional<Session> findByTokenHash(const std::string& tokenHash) = 0;

        virtual void revoke(const std::string& tokenHash) = 0;
    };
} // namespace picasso::domain
