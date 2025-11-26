#pragma once

#include <memory>

#include <spdlog/logger.h>

struct Logger
{
public:
  static void Init();
  static spdlog::logger& Get();

private:
  static inline std::shared_ptr<spdlog::logger> logger_{ nullptr };
};

#define LOG_TRACE(...) Logger::Get().trace(__VA_ARGS__);
#define LOG_DEBUG(...) Logger::Get().debug(__VA_ARGS__);
#define LOG_INFO(...) Logger::Get().info(__VA_ARGS__);
#define LOG_WARN(...) Logger::Get().warn(__VA_ARGS__);
#define LOG_ERROR(...) Logger::Get().error(__VA_ARGS__);
#define LOG_CRITICAL(...) Logger::Get().critical(__VA_ARGS__);
