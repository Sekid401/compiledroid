#pragma once
#include "common.h"
#include <string>

void log_info(const std::string& tag, const std::string& msg);
void log_ok  (const std::string& tag, const std::string& msg);
void log_warn(const std::string& tag, const std::string& msg);
void log_err (const std::string& tag, const std::string& msg);
void log_fix (const std::string& tag, const std::string& msg);
