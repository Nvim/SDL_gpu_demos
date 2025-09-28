#include "logger.h"
#include "spdlog/common.h"
#include "spdlog/sinks/stdout_color_sinks.h"

void
Logger::Init()
{
  logger_ = spdlog::stdout_color_mt("LOG");
  logger_->set_level(spdlog::level::trace);
  logger_->set_pattern("[%H:%M:%S:%e][%^%l%$]: %v");
}

spdlog::logger&
Logger::Get()
{
  return *logger_;
}
