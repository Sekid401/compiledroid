#pragma once
#include "manifest.h"
#include "state.h"
#include <string>
#include <vector>

enum class JVMKind {
    CLASSIC,  // java/javac on PATH (Linux/Termux openjdk)
    ART,      // dalvikvm on Android — uses ECJ jar for compiling
    WINDOWS,  // java.exe on Windows
    MACOS,    // /usr/libexec/java_home on macOS
    NONE
};

struct JVMInfo {
    JVMKind     kind = JVMKind::NONE;
    std::string java_bin;    // path to java or dalvikvm
    std::string javac_bin;   // path to javac, or empty if using ECJ
    std::string kotlinc_bin; // path to kotlinc if available
    bool        has_kotlin = false;
};

struct Compiler {
    std::string  project_dir;
    Manifest&    manifest;
    State&       state;
    JVMInfo      jvm;

    Compiler(const std::string& dir, Manifest& m, State& s)
        : project_dir(dir), manifest(m), state(s) {
        jvm = detect_jvm();
    }

    int compile_all(const std::vector<std::string>& files, bool resume);

    static JVMInfo      detect_jvm();
    static std::string  ecj_jar_path();
    static bool         ensure_ecj();   // fetch ECJ jar if missing (ART path)

private:
    int compile_one(const std::string& file);
    int compile_java(const std::string& file,
                     const std::string& classes,
                     const std::string& err_file);
    int compile_kotlin(const std::string& file,
                       const std::string& classes,
                       const std::string& err_file);
    std::string obj_path(const std::string& src) const;
};
