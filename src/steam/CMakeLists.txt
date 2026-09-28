# Steam OpenID assertion validation - the one Steam-facing job this server has.
#
# an HTTP client. oat++ ships one (oatpp::web::client) - link oatpp::oatpp here when
# that lands rather than pulling in a second HTTP stack.
add_library(picasso_steam STATIC
        OpenIdVerifier.cpp
        OpenIdVerifier.hpp)

target_link_libraries(picasso_steam 
        PUBLIC picasso_base picasso_domain
        PRIVATE oatpp::oatpp oatpp::oatpp-openssl
)
