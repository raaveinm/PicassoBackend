//
// Created by Kirill "Raaveinm" on 9/9/26.
//

#pragma once

#include <cstdint>
#include <string>

namespace picasso::app {

    struct Config {

        std::string bindAddress{"0.0.0.0"};
        std::uint16_t port{8000};

        std::string databaseDsn;

        std::string publicUrl{"http://127.0.0.1:8000"};

        std::string logLevel{"info"};
    };

    Config loadConfigFromEnvironment();
} // namespace picasso::app
