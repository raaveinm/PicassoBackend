//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace picasso::domain {
    /*
     * Distinct id types instead of bare uint64_t/string. The layering puts a lot of
     * ids next to each other in signatures - forwardSdp(from, conversation, to) is
     * three ids in a row - and a plain typedef would let any two of them swap
     * silently at the call site.
     */
    template <typename Tag, typename T>
    class Id {
    public:
        using value_type = T;

        Id() = default;
        explicit Id(T value) : value_(std::move(value)) {}

        const T& value() const noexcept { return value_; }

        friend bool operator==(const Id&, const Id&) = default;
        friend auto operator<=>(const Id&, const Id&) = default;

    private:
        T value_{};
    };

    struct SteamIdTag {};
    struct ConversationIdTag {};
    struct MessageIdTag {};

    /* Steam's 64-bit account id. */
    using SteamId = Id<SteamIdTag, std::uint64_t>;

    /* bigint identity in the schema, so int64 here rather than an opaque string. */
    using ConversationId = Id<ConversationIdTag, std::int64_t>;

    /* Server-assigned, globally monotonic. See messages.id in the schema. */
    using MessageId = Id<MessageIdTag, std::int64_t>;
} // namespace picasso::domain

template <typename Tag, typename T>
struct std::hash<picasso::domain::Id<Tag, T>> {
    std::size_t operator()(const picasso::domain::Id<Tag, T>& id) const noexcept {
        return std::hash<T>{}(id.value());
    }
};
