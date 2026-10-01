//
// Created by raaveinm on 10/2/26.
//

#include "logger/CsvLogger.hpp"

#include <utility>

namespace picasso::logger {

    CsvLogger::CsvLogger(std::shared_ptr<ActivityLogger> activityLogger)
        : activityLogger_(std::move(activityLogger)) {}

    std::string CsvLogger::levelName(const v_uint32 priority) {
        switch (priority) {
            case PRIORITY_V: return "VERBOSE";
            case PRIORITY_D: return "DEBUG";
            case PRIORITY_I: return "INFO";
            case PRIORITY_W: return "WARN";
            case PRIORITY_E: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    void CsvLogger::log(const v_uint32 priority, const std::string& tag, const std::string& message) {
        console_.log(priority, tag, message);

        if (activityLogger_ != nullptr) {
            activityLogger_->log(levelName(priority), tag, message);
        }
    }

    bool CsvLogger::isLogPriorityEnabled(const v_uint32 priority) {
        return console_.isLogPriorityEnabled(priority);
    }
} // namespace picasso::logger
