//
// Created by Kirill "Raaveinm" on 9/24/26.
//

#define BOOST_TEST_MODULE picasso_transport_http

#include <boost/test/unit_test.hpp>
#include "oatpp/core/base/Environment.hpp"

namespace {
    struct OatppEnvironment {
        OatppEnvironment() { oatpp::base::Environment::init(); }
        ~OatppEnvironment() { oatpp::base::Environment::destroy(); }
    };
} // namespace

BOOST_TEST_GLOBAL_FIXTURE(OatppEnvironment);
