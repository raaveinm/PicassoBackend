find_package(Boost REQUIRED CONFIG COMPONENTS unit_test_framework)

add_executable(picasso_transport_http_test
        transport/http/HttpTestMain.cpp
        transport/http/HttpTestServer.hpp
        transport/http/HealthControllerTest.cpp)

target_include_directories(picasso_transport_http_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(picasso_transport_http_test
        PRIVATE picasso_transport_http Boost::unit_test_framework Threads::Threads)

add_test(NAME picasso_transport_http_test COMMAND picasso_transport_http_test)
