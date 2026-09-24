//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#include <stdexcept>

#include <boost/test/unit_test.hpp>
#include "storage/Repositories.hpp"

BOOST_AUTO_TEST_SUITE(make_repositories)

    BOOST_AUTO_TEST_CASE(empty_dsn_throws) {
        BOOST_CHECK_THROW(picasso::storage::makeRepositories(std::string()), std::invalid_argument);
    }

BOOST_AUTO_TEST_SUITE_END()
