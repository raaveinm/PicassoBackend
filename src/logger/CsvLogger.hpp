//
// Created by raaveinm on 10/2/26.
//

#pragma once

#include <memory>
#include <string>

#include "oatpp/core/base/Environment.hpp"

#include "logger/ActivityLogger.hpp"

namespace picasso::logger {

    class CsvLogger final : public oatpp::base::Logger {
    public:
        explicit CsvLogger(std::shared_ptr<ActivityLogger> activityLogger);

        void log(v_uint32 priority, const std::string& tag, const std::string& message) override;
        bool isLogPriorityEnabled(v_uint32 priority) override;

    private:
        static std::string levelName(v_uint32 priority);

        std::shared_ptr<ActivityLogger> activityLogger_;
        oatpp::base::DefaultLogger console_;
    };

} // namespace picasso::logger
