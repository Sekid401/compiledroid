#pragma once
#include <string>
#include <vector>

struct Scanner {
    std::string project_dir;
    std::vector<std::string> src_files;
    std::vector<std::string> res_files;
    std::vector<std::string> lib_files;

    explicit Scanner(const std::string& dir) : project_dir(dir) {}
    void scan();

private:
    void walk(const std::string& dir, std::vector<std::string>& out,
              const std::vector<std::string>& exts);
};
