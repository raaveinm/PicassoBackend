add_library(picasso_logger STATIC
        ActivityLogger.hpp
        ActivityLogger.cpp
)

target_link_libraries(picasso_logger PUBLIC picasso_base)

