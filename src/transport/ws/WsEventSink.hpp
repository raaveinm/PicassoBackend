//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <memory>

#include "domain/ports/EventSink.hpp"
#include "domain/ports/SignalTransport.hpp"
#include "transport/ws/EnvelopeCodec.hpp"

namespace picasso::transport::ws {
    /**
     * Turns domain events into WS frames and hands them to whoever is connected. This
     * is the only place an event gains a wire shape; the services that publish them
     * never see JSON or a socket.
     */
    class WsEventSink final : public domain::EventSink {
    public:
        WsEventSink(std::shared_ptr<EnvelopeCodec> codec, std::shared_ptr<domain::SignalTransport> transport);

        void publish(const domain::Event& event) override;

    private:
        std::shared_ptr<EnvelopeCodec> codec_;
        std::shared_ptr<domain::SignalTransport> transport_;
    };
} // namespace picasso::transport::ws
