//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#include <boost/test/unit_test.hpp>

#include "transport/http/HttpTestServer.hpp"

using picasso::test::HttpTestServer;

BOOST_FIXTURE_TEST_SUITE(health_controller, HttpTestServer)

    BOOST_AUTO_TEST_CASE(ping_answers_pong) {
        const auto response = get("/ping");

        BOOST_TEST(response->getStatusCode() == 200);
        BOOST_TEST(*response->readBodyToString() == "pong");
        BOOST_TEST(*response->getHeader("X-Service-Status") == "Healthy");
    }

    BOOST_AUTO_TEST_CASE(unknown_route_is_404) {
        BOOST_TEST(get("/definitely-not-a-route")->getStatusCode() == 404);
    }

    // Path traversal guard in HealthController::isSafeAssetName.
    BOOST_AUTO_TEST_CASE(static_asset_rejects_traversal) {
        BOOST_TEST(get("/static/css/..%2F..%2Fetc%2Fpasswd.css")->getStatusCode() == 404);
        BOOST_TEST(get("/static/js/not-a-js-file.txt")->getStatusCode() == 404);
    }

    //TODO(add AuthController tests once services can be filled with fakes)

BOOST_AUTO_TEST_SUITE_END()
