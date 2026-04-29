#include "scanner.h"
#include "logger.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

static bool has_ext(const std::string& path, const std::vector<std::string>& exts) {
    for (auto& e : exts)
        if (path.size() > e.size() &&
            path.substr(path.size() - e.size()) == e) return true;
    return false;
}

void Scanner::walk(const std::string& dir, std::vector<std::string>& out,
                   const std::vector<std::string>& exts) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name(ent->d_name);
        if (name == "." || name == "..") continue;
        std::string full = dir + "/" + name;
        struct stat st{};
        stat(full.c_str(), &st);
        if (S_ISDIR(st.st_mode)) {
            walk(full, out, exts);
        } else if (S_ISREG(st.st_mode)) {
            if (has_ext(name, exts)) out.push_back(full);
        }
    }
    closedir(d);
    std::sort(out.begin(), out.end());
}

void Scanner::scan() {
    // Source files: .cpp .c .cc .java .kt
    walk(project_dir + "/src", src_files, {".cpp", ".c", ".cc", ".java", ".kt"});
    // Resources
    walk(project_dir + "/res", res_files, {".xml", ".png", ".jpg", ".webp", ".9.png"});
    // Libs: .so .a
    walk(project_dir + "/libs", lib_files, {".so", ".a"});
}
