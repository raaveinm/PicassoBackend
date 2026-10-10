//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace picasso::transport::ws {

    class BoundedFrameQueue {
    public:
        BoundedFrameQueue(const std::size_t max_frames, const std::size_t max_bytes)
            : max_frames_(max_frames), max_bytes_(max_bytes) {
        }

        enum class PushResult {
            Accepted,
            Overflowed,                 // this push (or an earlier one) overflowed: the connection is to be closed
            Closed,                     // the queue was closed: the frame was discarded
        };

        // A text frame for the client
        PushResult push(std::string frame) {
            return enqueue(Entry{.pong = false, .data = std::move(frame)});
        }

        // The reply to a ping, queued like any frame so that only the writer touches the socket
        PushResult pushPong(std::string payload) {
            return enqueue(Entry{.pong = true, .data = std::move(payload)});
        }

        struct Item {
            enum class Kind { Frame, Pong, Overflow, Closed };
            Kind kind{Kind::Closed};
            std::string data;
        };

        // Blocks until there is something for the writer to do
        Item pop() {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return closed_ || overflowed_ || !entries_.empty(); });
            if (closed_) {
                return Item{Item::Kind::Closed, {}};
            }
            if (overflowed_) {
                return Item{.kind = Item::Kind::Overflow, .data = {}};
            }
            auto [pong, data] = std::move(entries_.front());
            entries_.pop_front();
            bytes_ -= data.size();
            return Item{.kind = pong ? Item::Kind::Pong : Item::Kind::Frame, .data = std::move(data)};
        }

        // Wakes the writer and makes every later push a no-op. Idempotent
        void close() {
            {
                const std::lock_guard lock(mutex_);
                closed_ = true;
                entries_.clear();
                bytes_ = 0;
            }
            ready_.notify_all();
        }

        bool overflowed() {
            const std::lock_guard lock(mutex_);
            return overflowed_;
        }

    private:
        struct Entry {
            bool pong{};
            std::string data;
        };

        PushResult enqueue(Entry entry) {
            std::unique_lock lock(mutex_);
            if (closed_) {
                return PushResult::Closed;
            }
            if (overflowed_) {
                return PushResult::Overflowed;
            }
            if (entries_.size() + 1 > max_frames_ || bytes_ + entry.data.size() > max_bytes_) {
                overflowed_ = true;
                entries_.clear();
                bytes_ = 0;
                lock.unlock();
                ready_.notify_one();
                return PushResult::Overflowed;
            }
            bytes_ += entry.data.size();
            entries_.push_back(std::move(entry));
            lock.unlock();
            ready_.notify_one();
            return PushResult::Accepted;
        }

        const std::size_t max_frames_;
        const std::size_t max_bytes_;

        std::mutex mutex_;
        std::condition_variable ready_;
        std::deque<Entry> entries_;
        std::size_t bytes_{};
        bool overflowed_{};
        bool closed_{};
    };
} // namespace picasso::transport::ws
