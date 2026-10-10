//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "domain/Conversation.hpp"
#include "domain/Ids.hpp"
#include "domain/Message.hpp"
#include "domain/ports/ChatRepository.hpp"
#include "domain/ports/ContactRepository.hpp"
#include "domain/ports/ConversationRepository.hpp"
#include "domain/ports/EventSink.hpp"

namespace picasso::service {
    constexpr std::string CHAT_SERVICE_TAG{"CHAT_SERVICE"};

    enum class RejectCode {
        Invalid,
        TooLong,
        NotMember,
        NotAllowed,
        RateLimited,
        Internal,
    };

    inline std::string_view toWireString(const RejectCode code) {
        switch (code) {
            case RejectCode::Invalid: return "invalid";
            case RejectCode::TooLong: return "too_long";
            case RejectCode::NotMember: return "not_member";
            case RejectCode::NotAllowed: return "not_allowed";
            case RejectCode::RateLimited: return "rate_limited";
            case RejectCode::Internal: return "internal";
        }
        return "internal";
    }

    /**
     * Chat: sending, catching up, scrolling back, deleting. The server is SSOT, so every method
     * here either writes the truth or reads it - there is no merge.
     */
    class ChatService {
    public:
        using Clock = std::function<std::int64_t()>;

        ChatService(
            std::shared_ptr<domain::ChatRepository> chat,
            std::shared_ptr<domain::ConversationRepository> conversations,
            std::shared_ptr<domain::ContactRepository> contacts,
            std::shared_ptr<domain::EventSink> events = std::make_shared<domain::NullEventSink>(),
            Clock clock = {});

        ///////////////////////////////////////////////
        /// Sending
        ///////////////////////////////////////////////

        enum class SubmitStatus {
            Stored,                         // new: stored, delivered to the others
            Duplicate,                      // a retry of one already stored: acked again, NOT delivered again
            Rejected,
        };

        struct SubmitResult {
            SubmitStatus status{SubmitStatus::Rejected};
            std::optional<domain::Message> message;     // set unless Rejected
            RejectCode code{RejectCode::Internal};      // meaningful only when Rejected
        };

        [[nodiscard]] SubmitResult submit(
            domain::SteamId sender,
            const domain::ConversationId& conversation_id,
            const std::string& client_message_id,
            const std::string& body,
            domain::ConnectionId origin);

        ///////////////////////////////////////////////
        /// Catching up
        ///////////////////////////////////////////////

        struct SyncCursor {
            domain::ConversationId conversation_id;
            domain::MessageId after;                    // the highest message id the client holds for it
        };

        struct SyncedConversation {
            domain::Conversation conversation;
            bool writable{true};                        // false for a frozen dm
            bool reset{};                               // true: replace the cached range; false: append
            bool has_more_before{};                     // only meaningful when reset
            std::vector<domain::Message> messages;      // ascending
        };

        struct SyncResult {
            std::int64_t server_time{};
            std::int64_t deleted_cursor{};
            std::vector<SyncedConversation> conversations;
            std::vector<domain::ChatRepository::DeletedMessage> deleted;
        };

        /*
         * Walks ALL of the caller's conversations, not just the ones in `cursors`: that is how a
         * client learns about new conversations, and (by their absence) about removed ones.
         * Per conversation: no cursor, or more than `limit` messages after it -> `reset` with
         * the newest `limit`; otherwise `delta` with everything after the cursor. Keeping a
         * conversation's cached messages one contiguous range ending at the newest is what lets
         * a cursor be a plain maximum.
         */
        [[nodiscard]] SyncResult sync(
            domain::SteamId me,
            const std::vector<SyncCursor>& cursors,
            std::optional<std::int64_t> deleted_since,
            int limit);

        struct HistoryPage {
            bool member{};                              // false: no such conversation, or not the caller's
            std::vector<domain::Message> messages;      // ascending
            bool has_more_before{};
        };

        /* One scroll-up page: the newest `limit` messages older than `before`. */
        [[nodiscard]] HistoryPage history(
            domain::SteamId me,
            const domain::ConversationId& conversation_id,
            domain::MessageId before,
            int limit);

        ///////////////////////////////////////////////
        /// Deleting
        ///////////////////////////////////////////////

        enum class RemoveOutcome {
            Removed,                        // deleted now (or earlier - idempotent); every member's devices were told
            NotFound,                       // no such message, or it is not the caller's
        };

        /* Silent: no placeholder, no notice beyond the frame that makes it disappear. Sender only. */
        [[nodiscard]] RemoveOutcome remove(
            domain::SteamId me,
            const domain::ConversationId& conversation_id,
            domain::MessageId message_id);

    private:
        /* One mutex per stripe of conversation ids; collisions only cost a little concurrency. */
        std::mutex& lockFor(const domain::ConversationId& conversation_id);

        /* Token bucket per user: CHAT_RATE_BURST at once, CHAT_RATE_PER_MINUTE sustained. */
        bool takeToken(domain::SteamId sender, std::int64_t now_epoch_ms);

        std::shared_ptr<domain::ChatRepository> chat_;
        std::shared_ptr<domain::ConversationRepository> conversations_;
        std::shared_ptr<domain::ContactRepository> contacts_;
        std::shared_ptr<domain::EventSink> events_;
        Clock clock_;

        static constexpr std::size_t LOCK_STRIPES = 64;
        std::array<std::mutex, LOCK_STRIPES> locks_;

        struct Bucket {
            double tokens{};
            std::int64_t refilled_at_epoch_ms{};
        };
        std::mutex rate_mutex_;
        std::unordered_map<domain::SteamId, Bucket> buckets_;
    };
} // namespace picasso::service
