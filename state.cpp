#include "state.h"
#include <fstream>
#include <cstdlib>

std::string State::state_path() const {
    return project_dir + "/.compiledroid_state";
}

void State::load() {
    std::ifstream f(state_path());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("OK:", 0) == 0)
            compiled_ok.insert(line.substr(3));
        else if (line.rfind("FAILED:", 0) == 0)
            failed_file = line.substr(7);
    }
}

void State::save() {
    std::ofstream f(state_path());
    for (auto& s : compiled_ok) f << "OK:" << s << "\n";
    if (!failed_file.empty())   f << "FAILED:" << failed_file << "\n";
}

void State::reset() {
    compiled_ok.clear();
    failed_file.clear();
    std::remove(state_path().c_str());
}

void State::mark_ok(const std::string& file) {
    compiled_ok.insert(file);
    if (failed_file == file) failed_file.clear();
}

void State::mark_failed(const std::string& file) {
    failed_file = file;
}

bool State::is_done(const std::string& file) const {
    return compiled_ok.count(file) > 0;
}

void State::clean(const std::string& project_dir) {
    std::string state = project_dir + "/.compiledroid_state";
    std::string build = project_dir + "/build";
    std::remove(state.c_str());
    // rm -rf build/
    std::string cmd = "rm -rf " + build;
    system(cmd.c_str());
}
