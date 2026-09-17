//
// Created by raaveinm on 9/17/26.
//

#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

namespace picasso::logger {

    /**
     *
     * Writes server activity rows to `logs/log_file<MM-DD-YYYY>.csv`. A new file
     * starts automatically when the local calendar day changes, and files older
     * than retentionDays (by the date encoded in their own name, not mtime) are
     * removed on each rotation.
     *
     */

    class ActivityLogger {
    public:
        explicit ActivityLogger(std::filesystem::path directory = "logs", int retentionDays = 7);

        void log(const std::string& level, const std::string& component, const std::string& message);

    private:
        static std::chrono::year_month_day todayLocal();
        static std::string dateStamp(const std::chrono::year_month_day& day);
        static std::string timestamp();
        static std::string csvEscape(const std::string& field);
        static std::optional<std::chrono::year_month_day> parseDateStamp(const std::string& stamp);

        void rotateIfNeeded();
        void purgeOldFiles(const std::chrono::year_month_day& today) const;

        std::filesystem::path directory_;
        int retentionDays_;
        std::mutex mutex_;
        std::ofstream file_;
        std::string currentStamp_;
    };

} // namespace picasso::logger
