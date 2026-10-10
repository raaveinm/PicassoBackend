//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <thread>

#include "oatpp/core/data/stream/BufferStream.hpp"
#include "oatpp-websocket/WebSocket.hpp"

#include "domain/Ids.hpp"
#include "service/Services.hpp"
#include "transport/ws/BoundedFrameQueue.hpp"
#include "transport/ws/EnvelopeCodec.hpp"
#include "transport/ws/Outbound.hpp"

namespace picasso::transport::ws {
    /**
     * One connected client. The identity is a constructor argument and a const
     * member: it was established during the upgrade handshake and cannot be changed
     * by anything that arrives afterwards. Every service call this session makes
     * passes steamId_, never a value read out of a frame.
     *
     * Reading happens on oat++'s connection thread (readMessage); writing happens on this
     * session's own writer thread, fed by a bounded queue. Nothing else touches the socket,
     * so there is exactly one writer without a lock around the write, and a slow client
     * costs its own thread - never the thread that is fanning a message out to it.
     */
    class WsSession final : public oatpp::websocket::WebSocket::Listener, public Outbound {
    public:
        WsSession(
            domain::SteamId steam_id,
            const oatpp::websocket::WebSocket* socket,
            std::shared_ptr<EnvelopeCodec> codec,
            service::Services services);

        ~WsSession() override;

        domain::SteamId steamId() const { return steam_id_; }

        void send(const std::string& frame) override;

        domain::ConnectionId connectionId() const override { return connection_id_; }

        /*
         * Stops the writer and waits for it. Must run before the socket is destroyed, because
         * the writer holds a raw pointer to it - SessionFactory::onBeforeDestroy calls this.
         * Idempotent.
         */
        void shutdown();

        void onPing(const WebSocket& socket, const oatpp::String& message) override;
        void onPong(const WebSocket& socket, const oatpp::String& message) override;
        void onClose(const WebSocket& socket, v_uint16 code, const oatpp::String& message) override;
        void readMessage(const WebSocket& socket, v_uint8 opcode, p_char8 data, oatpp::v_io_size size) override;

    private:
        void writerLoop();
        void dispatch(const std::string& frame);
        void handleChatMessage(const oatpp::Object<dto::EnvelopeDto>& envelope);
        void sendNack(
            const oatpp::String& conversation_id,
            const oatpp::String& client_message_id,
            const std::string& code);

        const domain::SteamId steam_id_;
        const domain::ConnectionId connection_id_;
        const oatpp::websocket::WebSocket* socket_;
        std::shared_ptr<EnvelopeCodec> codec_;
        service::Services services_;

        /* A few hundred frames or a few MiB: a client further behind than that is disconnected, not waited for. */
        static constexpr std::size_t MAX_QUEUED_FRAMES = 512;
        static constexpr std::size_t MAX_QUEUED_BYTES = 4 * 1024 * 1024;
        BoundedFrameQueue queue_{MAX_QUEUED_FRAMES, MAX_QUEUED_BYTES};
        std::thread writer_;

        /* WS messages arrive fragmented; frames accumulate here until size == 0. */
        oatpp::data::stream::BufferOutputStream inbound_;
    };
} // namespace picasso::transport::ws
