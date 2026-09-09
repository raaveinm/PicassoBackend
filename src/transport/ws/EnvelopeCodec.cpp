//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#include "transport/ws/EnvelopeCodec.hpp"

#include <utility>

namespace picasso::transport::ws {
    EnvelopeCodec::EnvelopeCodec(std::shared_ptr<oatpp::data::mapping::ObjectMapper> mapper)
        : mapper_(std::move(mapper)) {}

    oatpp::Object<dto::EnvelopeDto> EnvelopeCodec::decode(const std::string& frame) const {
        try {
            return mapper_->readFromString<oatpp::Object<dto::EnvelopeDto>>(oatpp::String(frame));
        } catch (const std::exception&) {
            /*
             * Malformed input from a client is expected traffic, not an error worth
             * unwinding a connection over. The session decides what to do with a null.
             */
            return nullptr;
        }
    }

    std::string EnvelopeCodec::encode(const oatpp::Object<dto::EnvelopeDto>& envelope) const {
        const auto encoded = mapper_->writeToString(envelope);
        /* oatpp::String wraps std::string, so operator* yields it directly. */
        return encoded ? *encoded : std::string{};
    }

    oatpp::Object<dto::EnvelopeDto> EnvelopeCodec::envelopeOf(const dto::MessageType type) {
        auto envelope = dto::EnvelopeDto::createShared();
        envelope->type = toWireString(type).c_str();
        return envelope;
    }
} // namespace picasso::transport::ws
