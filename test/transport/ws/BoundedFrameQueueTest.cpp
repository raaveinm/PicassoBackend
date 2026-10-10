//
// Created by Kirill "Raaveinm" on 10/9/26.
//

#include <chrono>
#include <future>
#include <string>
#include <thread>

#include <boost/test/unit_test.hpp>

#include "transport/ws/BoundedFrameQueue.hpp"

using picasso::transport::ws::BoundedFrameQueue;
using Kind = BoundedFrameQueue::Item::Kind;

// The policy that makes it safe to push a delivery while holding a conversation's lock: a push
// never waits, a slow client is cut off rather than waited for, and nothing is ever dropped
// silently - a client that falls behind is disconnected, then recovers through POST /sync.
BOOST_AUTO_TEST_SUITE(bounded_frame_queue)

    BOOST_AUTO_TEST_CASE(frames_come_out_in_the_order_they_went_in) {
        BoundedFrameQueue queue(10, 1024);
        queue.push("a");
        queue.push("b");
        queue.pushPong("ping-payload");
        queue.push("c");

        BOOST_TEST(queue.pop().data == "a");
        BOOST_TEST(queue.pop().data == "b");
        const auto pong = queue.pop();
        BOOST_CHECK(pong.kind == Kind::Pong);
        BOOST_TEST(pong.data == "ping-payload");
        BOOST_TEST(queue.pop().data == "c");
    }

    BOOST_AUTO_TEST_CASE(pushing_past_the_frame_limit_overflows_instead_of_dropping_one_frame) {
        BoundedFrameQueue queue(3, 1024);
        BOOST_CHECK(queue.push("1") == BoundedFrameQueue::PushResult::Accepted);
        BOOST_CHECK(queue.push("2") == BoundedFrameQueue::PushResult::Accepted);
        BOOST_CHECK(queue.push("3") == BoundedFrameQueue::PushResult::Accepted);

        BOOST_CHECK(queue.push("4") == BoundedFrameQueue::PushResult::Overflowed);

        // The writer is told to close the connection - it does NOT get frames 1..3 followed by a gap.
        BOOST_CHECK(queue.pop().kind == Kind::Overflow);
        BOOST_TEST(queue.overflowed());
        // and it stays overflowed: later pushes can't smuggle a frame past the disconnect
        BOOST_CHECK(queue.push("5") == BoundedFrameQueue::PushResult::Overflowed);
        BOOST_CHECK(queue.pop().kind == Kind::Overflow);
    }

    BOOST_AUTO_TEST_CASE(the_byte_limit_applies_too) {
        BoundedFrameQueue queue(100, 10);
        BOOST_CHECK(queue.push(std::string(6, 'x')) == BoundedFrameQueue::PushResult::Accepted);

        BOOST_CHECK(queue.push(std::string(6, 'y')) == BoundedFrameQueue::PushResult::Overflowed);
    }

    BOOST_AUTO_TEST_CASE(a_consumer_that_keeps_up_never_overflows) {
        BoundedFrameQueue queue(2, 1024);
        for (int i = 0; i < 50; ++i) {
            BOOST_REQUIRE(queue.push("m") == BoundedFrameQueue::PushResult::Accepted);
            BOOST_REQUIRE(queue.pop().kind == Kind::Frame);
        }
        BOOST_TEST(!queue.overflowed());
    }

    BOOST_AUTO_TEST_CASE(pop_blocks_until_something_arrives) {
        BoundedFrameQueue queue(10, 1024);
        auto waiting = std::async(std::launch::async, [&queue] { return queue.pop(); });

        BOOST_CHECK(waiting.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
        queue.push("late");

        BOOST_REQUIRE(waiting.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        BOOST_TEST(waiting.get().data == "late");
    }

    BOOST_AUTO_TEST_CASE(closing_wakes_a_blocked_writer_and_discards_later_frames) {
        BoundedFrameQueue queue(10, 1024);
        auto waiting = std::async(std::launch::async, [&queue] { return queue.pop(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        queue.close();

        BOOST_REQUIRE(waiting.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        BOOST_CHECK(waiting.get().kind == Kind::Closed);
        BOOST_CHECK(queue.push("x") == BoundedFrameQueue::PushResult::Closed);
        queue.close();                                                  // idempotent
    }

    BOOST_AUTO_TEST_CASE(many_pushers_one_writer_loses_nothing_within_the_limits) {
        constexpr int PUSHERS = 4;
        constexpr int EACH = 100;
        BoundedFrameQueue queue(PUSHERS * EACH, 1024 * 1024);

        std::vector<std::thread> pushers;
        for (int p = 0; p < PUSHERS; ++p) {
            pushers.emplace_back([&queue] {
                for (int i = 0; i < EACH; ++i) {
                    queue.push("x");
                }
            });
        }
        for (auto& pusher : pushers) {
            pusher.join();
        }

        int received = 0;
        while (received < PUSHERS * EACH) {
            BOOST_REQUIRE(queue.pop().kind == Kind::Frame);
            ++received;
        }
        BOOST_TEST(received == PUSHERS * EACH);
    }

BOOST_AUTO_TEST_SUITE_END()
