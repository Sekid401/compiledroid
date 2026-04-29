#pragma once
#include <string>
#include <set>

struct State {
    std::string project_dir;
    std::set<std::string> compiled_ok; // files successfully compiled
    std::string failed_file;           // file that failed last

    explicit State(const std::string& dir) : project_dir(dir) {}

    void load();
    void save();
    void reset();
    void mark_ok(const std::string& file);
    void mark_failed(const std::string& file);
    bool is_done(const std::string& file) const;
    std::string resume_file() const { return failed_file; }

    static void clean(const std::string& project_dir);

private:
    std::string state_path() const;
};
