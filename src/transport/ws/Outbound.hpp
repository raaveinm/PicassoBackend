//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <string>

namespace picasso::transport::ws {
    /*
     * What the hub is allowed to do to a connection. Deliberately narrower than the
     * session itself, and free of oat++ types, so the hub stays testable without a
     * socket and cannot reach into session state it has no business touching.
     */
    class Outbound {
    public:
        virtual ~Outbound() = default;

        /* Must be safe to call from any thread - fan-out runs on the sender's thread. */
        virtual void send(const std::string& frame) = 0;
    };
} // namespace picasso::transport::ws
