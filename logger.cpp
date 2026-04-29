#include "logger.h"
#include <iostream>

void log_info(const std::string& tag, const std::string& msg) {
    std::cout << C_CYAN << "[" << tag << "] " << C_RESET << msg << "\n";
}
void log_ok(const std::string& tag, const std::string& msg) {
    std::cout << C_GREEN << C_BOLD << "[" << tag << "] " << C_RESET << msg << "\n";
}
void log_warn(const std::string& tag, const std::string& msg) {
    std::cout << C_YELLOW << "[" << tag << "] " << C_RESET << msg << "\n";
}
void log_err(const std::string& tag, const std::string& msg) {
    std::cerr << C_RED << C_BOLD << "[" << tag << "] " << C_RESET << msg << "\n";
}
void log_fix(const std::string& tag, const std::string& msg) {
    std::cout << C_YELLOW << C_BOLD << "[" << tag << "] " << C_RESET << msg << "\n";
}
