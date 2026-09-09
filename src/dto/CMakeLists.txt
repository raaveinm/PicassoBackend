# The wire contract. Depends on oat++ for the DTO macros, and on domain for the
# types it converts to and from - but nothing depends on dto except transport/,
# which is why services take domain types in their signatures rather than DTOs.
add_library(picasso_dto STATIC
        MessageType.cpp
        MessageType.hpp
        Envelope.hpp
        Rest.hpp)

target_link_libraries(picasso_dto
        PUBLIC picasso_base picasso_domain oatpp::oatpp)
