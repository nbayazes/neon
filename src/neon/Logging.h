#pragma once

// have to use header only for wchar support
//#include <fmt/core.h>
//#include <fmt/xchar.h>
//#define SPDLOG_FMT_EXTERNAL
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include <spdlog/spdlog.h>

void ConfigureLogging(const std::string& logName);
