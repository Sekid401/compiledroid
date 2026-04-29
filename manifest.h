#pragma once
#include <string>
#include <vector>

struct Manifest {
    std::string package;
    std::string app_name;
    std::string version_name;
    std::string version_code;
    std::string min_sdk;
    std::string target_sdk;
    std::vector<std::string> permissions;
    std::string activity_main;

    bool load(const std::string& path);
};
