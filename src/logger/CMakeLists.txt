add_library(picasso_logger STATIC
        ActivityLogger.hpp
        ActivityLogger.cpp
        CsvLogger.hpp
        CsvLogger.cpp
)

target_link_libraries(picasso_logger PUBLIC picasso_base oatpp::oatpp)
