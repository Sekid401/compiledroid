/**
 * compiledroid v1.0.0
 * Termux-native Android build tool
 * No Gradle, no JVM, no Android Studio
 */

#include "common.h"
#include "manifest.h"
#include "scanner.h"
#include "compiler.h"
#include "packager.h"
#include "errorengine.h"
#include "logger.h"
#include "state.h"

#include <iostream>
#include <string>
#include <cstring>

void print_banner() {
    std::cout << C_CYAN << C_BOLD
              << "compiledroid v1.0.0\n"
              << C_RESET
              << "  Termux-native Android build tool\n\n";
}

void print_usage() {
    std::cout << "Usage:\n"
              << "  compiledroid <project_dir>           Build project\n"
              << "  compiledroid <project_dir> --resume  Resume from last failed file\n"
              << "  compiledroid --clean <project_dir>   Clean build artifacts\n"
              << "  compiledroid --version               Show version\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(); return 1; }

    std::string arg1(argv[1]);

    if (arg1 == "--version") {
        std::cout << "compiledroid v1.0.0\n";
        return 0;
    }

    bool resume  = false;
    bool clean   = false;
    std::string project_dir;

    if (arg1 == "--clean" && argc >= 3) {
        clean = true;
        project_dir = argv[2];
    } else {
        project_dir = arg1;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--resume") == 0) resume = true;
        }
    }

    print_banner();

    // ── Clean mode ──
    if (clean) {
        log_info("clean", "Cleaning build artifacts in " + project_dir);
        State::clean(project_dir);
        log_ok("clean", "Done.");
        return 0;
    }

    // ── Parse manifest.xml ──
    Manifest manifest;
    if (!manifest.load(project_dir + "/manifest.xml")) {
        log_err("manifest", "manifest.xml not found or invalid in " + project_dir);
        return 1;
    }
    log_ok("manifest", "Package: " + manifest.package + " v" + manifest.version_name);

    // ── Scan project ──
    Scanner scanner(project_dir);
    scanner.scan();
    log_info("scanner", "Found " + std::to_string(scanner.src_files.size()) + " source file(s)");
    log_info("scanner", "Found " + std::to_string(scanner.res_files.size()) + " resource file(s)");
    log_info("scanner", "Found " + std::to_string(scanner.lib_files.size()) + " lib file(s)");

    // ── Load/init state ──
    State state(project_dir);
    if (resume) {
        state.load();
        log_info("state", "Resuming from: " + state.resume_file());
    } else {
        state.reset();
    }

    // ── Compile ──
    Compiler compiler(project_dir, manifest, state);
    int compile_result = compiler.compile_all(scanner.src_files, resume);
    if (compile_result != 0) {
        log_err("build", "Build stopped. Fix the error above, then run with --resume");
        state.save();
        return compile_result;
    }

    // ── Package ──
    Packager packager(project_dir, manifest);
    int pack_result = packager.pack(scanner.res_files, scanner.lib_files);
    if (pack_result != 0) {
        log_err("package", "Packaging failed.");
        return pack_result;
    }

    log_ok("build", "Build complete: " + project_dir + "/" + manifest.app_name + ".apk");
    state.reset(); // clear state on success
    return 0;
}
