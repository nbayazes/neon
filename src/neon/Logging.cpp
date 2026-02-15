#include "pch.h"
#include "Logging.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

using string = std::string;
namespace filesystem = std::filesystem;

void ConfigureLogging(const string& logName) {
    string logPath = logName;

    // use the executable directory for log output
    TCHAR szFileName[MAX_PATH];
    if (SUCCEEDED(GetModuleFileName(nullptr, szFileName, MAX_PATH))) {
        filesystem::path path(szFileName);
        logPath = path.parent_path().append(logName).string();
    }

    filesystem::remove(logPath); // Erase old log file

    auto logger = std::make_shared<spdlog::logger>(
        "loggers",
        spdlog::sinks_init_list{
            std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath)
        }
    );

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);

    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting#pattern-flags
    spdlog::set_pattern("[%M:%S.%e] [%^%l%$] [TID:%t] [%s:%#] %v");
}
