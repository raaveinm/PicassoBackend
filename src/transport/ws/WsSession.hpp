//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "oatpp/core/data/stream/BufferStream.hpp"
#include "oatpp-websocket/WebSocket.hpp"

#include "domain/Ids.hpp"
#include "service/Services.hpp"
#include "transport/ws/EnvelopeCodec.hpp"
#include "transport/ws/Outbound.hpp"

namespace picasso::transport::ws {
    /**
     * One connected client. The identity is a constructor argument and a const
     * member: it was established during the upgrade handshake and cannot be changed
     * by anything that arrives afterwards. Every service call this session makes
     * passes steamId_, never a value read out of a frame.
     */
    class WsSession final : public oatpp::websocket::WebSocket::Listener, public Outbound {
    public:
        WsSession(domain::SteamId steamId,
                  const oatpp::websocket::WebSocket* socket,
                  std::shared_ptr<EnvelopeCodec> codec,
                  service::Services services);

        domain::SteamId steamId() const { return steamId_; }

        void send(const std::string& frame) override;

        void onPing(const WebSocket& socket, const oatpp::String& message) override;
        void onPong(const WebSocket& socket, const oatpp::String& message) override;
        void onClose(const WebSocket& socket, v_uint16 code, const oatpp::String& message) override;
        void readMessage(const WebSocket& socket, v_uint8 opcode, p_char8 data, oatpp::v_io_size size) override;

    private:
        void dispatch(const std::string& frame);

        const domain::SteamId steamId_;
        const oatpp::websocket::WebSocket* socket_;
        std::shared_ptr<EnvelopeCodec> codec_;
        service::Services services_;

        /**
         * Fan-out runs on the sending peer's thread, so several threads can reach
         * this socket at once and sendOneFrameText is not safe under that.
         *
         * TODO(roadmap step 2): replace with a bounded queue drained by one writer.
         * A mutex makes concurrent sends safe but gives no backpressure - a slow
         * consumer currently blocks whoever is fanning out to it.
         */
        std::mutex sendMutex_;

        /* WS messages arrive fragmented; frames accumulate here until size == 0. */
        oatpp::data::stream::BufferOutputStream inbound_;
    };
} // namespace picasso::transport::ws
