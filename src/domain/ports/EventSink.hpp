//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include "domain/Events.hpp"

namespace picasso::domain {
    /**
     * Best-effort push to whoever is connected right now. Delivery is never required
     * for correctness: a device that was offline reads GET /contacts and POST /sync
     * on reconnect, so an event that reaches nobody costs nothing.
     *
     * Implementations must not throw into the caller - the state change has already
     * been committed by the time an event is published.
     */
    class EventSink {
    public:
        virtual ~EventSink() = default;

        virtual void publish(const Event& event) = 0;
    };

    class NullEventSink final : public EventSink {
    public:
        void publish(const Event&) override {}
    };
} // namespace picasso::domain
