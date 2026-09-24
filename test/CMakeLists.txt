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

# Integration tests against a real Postgres named by PICASSO_TEST_DB_DSN; suites
# that need it are reported as skipped when it is unset. They TRUNCATE every table.
add_executable(picasso_storage_test
        storage/StorageTestMain.cpp
        storage/StorageTestDatabase.hpp
        storage/RepositoriesTest.cpp
        storage/ChatRepositoryTest.cpp
        storage/ConversationRepositoryTest.cpp
        storage/SessionRepositoryTest.cpp)

target_include_directories(picasso_storage_test
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# oatpp-postgresql directly, not through picasso_storage (which keeps it PRIVATE):
# the seed client writes rows no repository port can write yet.
target_link_libraries(picasso_storage_test
        PRIVATE picasso_storage oatpp::oatpp-postgresql Boost::unit_test_framework Threads::Threads)

add_test(NAME picasso_storage_test COMMAND picasso_storage_test)
