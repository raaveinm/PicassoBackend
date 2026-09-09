# One target per module. The split is not cosmetic: target_link_libraries below is
# what actually enforces the dependency direction from CLAUDE.md
# (transport -> service -> domain/ports <- storage). A layer that reaches sideways
# fails to link instead of merely looking wrong in review.

# Carries the include root and the language level. Everything links it, so headers
# are always spelled from src/ ("domain/Ids.hpp"), never relatively.
add_library(picasso_base INTERFACE)
target_include_directories(picasso_base INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(picasso_base INTERFACE cxx_std_20)

add_subdirectory(domain)
add_subdirectory(dto)
add_subdirectory(storage)
add_subdirectory(steam)
add_subdirectory(service)
add_subdirectory(transport)
add_subdirectory(app)

# The single thing the executable links. Aggregation only - no sources of its own.
add_library(picasso_core INTERFACE)
target_link_libraries(picasso_core INTERFACE picasso_app)
