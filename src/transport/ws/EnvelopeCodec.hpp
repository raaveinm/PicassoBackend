//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <memory>
#include <string>

#include "oatpp/core/data/mapping/ObjectMapper.hpp"

#include "dto/Envelope.hpp"
#include "dto/MessageType.hpp"

namespace picasso::transport::ws {
    /*
     * The only place WS frames become DTOs. Keeping it separate from the session
     * means malformed input is rejected in one spot, and the session never has to
     * decide what a half-parsed envelope means.
     */
    class EnvelopeCodec {
    public:
        explicit EnvelopeCodec(std::shared_ptr<oatpp::data::mapping::ObjectMapper> mapper);

        /* Returns nullptr for anything that is not a well-formed envelope. */
        oatpp::Object<dto::EnvelopeDto> decode(const std::string& frame) const;

        std::string encode(const oatpp::Object<dto::EnvelopeDto>& envelope) const;

        /* Convenience for the outbound side: builds an envelope with `type` set. */
        static oatpp::Object<dto::EnvelopeDto> envelopeOf(dto::MessageType type);

    private:
        std::shared_ptr<oatpp::data::mapping::ObjectMapper> mapper_;
    };
} // namespace picasso::transport::ws
