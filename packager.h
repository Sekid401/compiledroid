#pragma once
#include "manifest.h"
#include <string>
#include <vector>

struct Packager {
    std::string project_dir;
    Manifest&   manifest;

    Packager(const std::string& dir, Manifest& m)
        : project_dir(dir), manifest(m) {}

    int pack(const std::vector<std::string>& res_files,
             const std::vector<std::string>& lib_files);

private:
    int run_aapt2();
    int dex_classes();
    int link_native();
    int sign_apk();
    std::string apk_path() const;
    std::string unsigned_apk_path() const;
};
