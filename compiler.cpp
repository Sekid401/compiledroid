#include "compiler.h"
#include "errorengine.h"
#include "logger.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>

// ── Helpers ─────────────────────────────────────────────────────

static void mkdir_p(const std::string& path) {
    system(("mkdir -p " + path).c_str());
}

static bool cmd_exists(const std::string& cmd) {
    return system(("command -v " + cmd + " >/dev/null 2>&1").c_str()) == 0;
}

static bool file_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}

static std::string ext(const std::string& f) {
    auto p = f.rfind('.');
    return p != std::string::npos ? f.substr(p) : "";
}

// ── ECJ jar path ────────────────────────────────────────────────
// Stored in $PREFIX/share/compiledroid/ecj.jar on Termux,
// or ~/.compiledroid/ecj.jar elsewhere

std::string Compiler::ecj_jar_path() {
    const char* prefix = getenv("PREFIX"); // Termux sets this
    if (prefix) return std::string(prefix) + "/share/compiledroid/ecj.jar";
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/.compiledroid/ecj.jar";
    return "/tmp/ecj.jar";
}

bool Compiler::ensure_ecj() {
    std::string jar = ecj_jar_path();
    std::string dex = jar.substr(0, jar.rfind('/')) + "/ecj.dex";

    // If DEX already exists, we're good
    if (file_exists(dex)) return true;

    std::string dir = jar.substr(0, jar.rfind('/'));
    mkdir_p(dir);

    // Download jar if needed
    if (!file_exists(jar)) {
        log_info("ecj", "Downloading ECJ compiler jar...");
        std::string url = "https://repo1.maven.org/maven2/org/eclipse/jdt/ecj/3.37.0/ecj-3.37.0.jar";
        std::string cmd = "curl -fsSL -o " + jar + " \"" + url + "\"";
        if (system(cmd.c_str()) != 0) {
            // Fallback
            url = "https://search.maven.org/remotecontent?filepath=org/eclipse/jdt/ecj/3.37.0/ecj-3.37.0.jar";
            cmd = "curl -fsSL -o " + jar + " \"" + url + "\"";
            if (system(cmd.c_str()) != 0) {
                log_err("ecj", "Failed to download ECJ jar.");
                return false;
            }
        }
        log_ok("ecj", "ECJ jar downloaded.");
    }

    // Convert jar → DEX so dalvikvm can run it
    log_info("ecj", "Converting ECJ jar to DEX (d8)...");
    std::string d8_cmd = "d8 --output " + dir + " " + jar + " 2>&1";
    if (system(d8_cmd.c_str()) != 0) {
        log_err("ecj", "d8 conversion failed. Is d8 installed? (pkg install d8)");
        return false;
    }

    // d8 outputs classes.dex — rename to ecj.dex
    std::string classes_dex = dir + "/classes.dex";
    if (file_exists(classes_dex)) {
        std::rename(classes_dex.c_str(), dex.c_str());
        log_ok("ecj", "ECJ DEX ready: " + dex);
    } else {
        log_err("ecj", "d8 ran but classes.dex not found.");
        return false;
    }

    return true;
}

// ── JVM Detection ───────────────────────────────────────────────

JVMInfo Compiler::detect_jvm() {
    JVMInfo info;

    // 1. Classic JVM — java/javac on PATH
    if (cmd_exists("java") && cmd_exists("javac")) {
        info.kind     = JVMKind::CLASSIC;
        info.java_bin = "java";
        info.javac_bin = "javac";
        info.has_kotlin = cmd_exists("kotlinc");
        if (info.has_kotlin) info.kotlinc_bin = "kotlinc";
        log_ok("jvm", "Classic JVM detected (javac)");
        return info;
    }

    // 2. ART — dalvikvm available (Android)
    if (cmd_exists("dalvikvm")) {
        info.kind     = JVMKind::ART;
        info.java_bin = "dalvikvm";
        info.javac_bin = ""; // uses ECJ jar via dalvikvm
        // kotlinc standalone (doesn't need JDK, just JRE — ART counts)
        if (cmd_exists("kotlinc")) {
            info.has_kotlin = true;
            info.kotlinc_bin = "kotlinc";
        }
        log_ok("jvm", "ART (dalvikvm) detected — will use ECJ for Java compilation");
        return info;
    }

    // 3. Windows JVM
#ifdef _WIN32
    std::vector<std::string> win_paths = {
        "C:\\Program Files\\Java\\jdk-17\\bin\\java.exe",
        "C:\\Program Files\\Eclipse Adoptium\\jdk-17\\bin\\java.exe",
        "C:\\Program Files\\Microsoft\\jdk-17\\bin\\java.exe",
    };
    for (auto& p : win_paths) {
        if (file_exists(p)) {
            info.kind      = JVMKind::WINDOWS;
            info.java_bin  = "\"" + p + "\"";
            std::string javac = p.substr(0, p.rfind('\\')) + "\\javac.exe";
            if (file_exists(javac)) info.javac_bin = "\"" + javac + "\"";
            info.has_kotlin = cmd_exists("kotlinc");
            if (info.has_kotlin) info.kotlinc_bin = "kotlinc";
            log_ok("jvm", "Windows JVM detected: " + p);
            return info;
        }
    }
#endif

    // 4. macOS JVM
#ifdef __APPLE__
    FILE* p = popen("/usr/libexec/java_home 2>/dev/null", "r");
    if (p) {
        char buf[512] = {};
        fgets(buf, sizeof(buf), p);
        pclose(p);
        std::string java_home(buf);
        if (!java_home.empty() && java_home.back() == '\n')
            java_home.pop_back();
        if (!java_home.empty()) {
            info.kind      = JVMKind::MACOS;
            info.java_bin  = java_home + "/bin/java";
            info.javac_bin = java_home + "/bin/javac";
            info.has_kotlin = cmd_exists("kotlinc");
            if (info.has_kotlin) info.kotlinc_bin = "kotlinc";
            log_ok("jvm", "macOS JVM detected: " + java_home);
            return info;
        }
    }
#endif

    // No JVM found
    info.kind = JVMKind::NONE;
    log_warn("jvm", "No JVM found — Java/Kotlin compilation unavailable");
    log_warn("jvm", "Android: dalvikvm should be available");
    log_warn("jvm", "Linux:   pkg install openjdk-17");
    log_warn("jvm", "Windows: install JDK 17 from adoptium.net");
    log_warn("jvm", "macOS:   brew install openjdk@17");
    return info;
}

// ── Java compilation ────────────────────────────────────────────

int Compiler::compile_java(const std::string& file,
                           const std::string& classes,
                           const std::string& err_file) {
    std::string cmd;

    switch (jvm.kind) {
        case JVMKind::CLASSIC:
        case JVMKind::MACOS: {
            std::string android_jar;
            const char* pfx = getenv("PREFIX");
            if (pfx) android_jar = std::string(pfx) + "/share/compiledroid/sdk/android.jar";
            std::string cp = classes;
            if (!android_jar.empty() && file_exists(android_jar))
                cp = android_jar + ":" + classes;
            cmd = jvm.javac_bin + " -d " + classes + " -cp " + cp
                + " " + file + " 2>" + err_file;
            break;
        }

        case JVMKind::WINDOWS:
            if (!jvm.javac_bin.empty())
                cmd = jvm.javac_bin + " -d " + classes + " -cp " + classes
                    + " " + file + " 2>" + err_file;
            else {
                // Fall back to ECJ on Windows if no javac
                if (!ensure_ecj()) return 1;
                cmd = jvm.java_bin + " -jar " + ecj_jar_path()
                    + " -d " + classes + " " + file + " 2>" + err_file;
            }
            break;

        case JVMKind::ART:
            // ART: dalvikvm runs ECJ dex
            if (!ensure_ecj()) return 1;
            {
                std::string dex = ecj_jar_path().substr(0, ecj_jar_path().rfind('/')) + "/ecj.dex";
                cmd = "dalvikvm -cp " + dex
                    + " org.eclipse.jdt.internal.compiler.batch.Main"
                    + " -d " + classes + " -source 8 -target 8"
                    + " " + file + " 2>" + err_file;
            }
            break;

        default:
            log_err("compile", "No JVM available to compile Java.");
            return 1;
    }

    return system(cmd.c_str());
}

// ── Kotlin compilation ──────────────────────────────────────────

int Compiler::compile_kotlin(const std::string& file,
                             const std::string& classes,
                             const std::string& err_file) {
    if (!jvm.has_kotlin) {
        log_err("compile", "kotlinc not found.");
        log_warn("compile", "Install: pkg install kotlin (or download from kotlinlang.org)");
        return 1;
    }

    // kotlinc works the same across all JVM variants since it's a standalone tool
    std::string cmd = jvm.kotlinc_bin + " " + file
        + " -include-runtime -d " + classes + " 2>" + err_file;
    return system(cmd.c_str());
}

// ── obj path ────────────────────────────────────────────────────

std::string Compiler::obj_path(const std::string& src) const {
    std::string rel = src;
    std::string prefix = project_dir + "/src/";
    if (rel.rfind(prefix, 0) == 0) rel = rel.substr(prefix.size());
    for (char& c : rel) if (c == '/') c = '_';
    auto dot = rel.rfind('.');
    if (dot != std::string::npos) rel = rel.substr(0, dot);
    return project_dir + "/build/obj/" + rel + ".o";
}

// ── compile_one ─────────────────────────────────────────────────

int Compiler::compile_one(const std::string& file) {
    std::string e = ext(file);
    std::string err_file = project_dir + "/build/.last_error.txt";
    std::string cmd;

    // ── C/C++ via clang ──
    if (e == ".cpp" || e == ".cc" || e == ".c") {
        std::string compiler_bin = (e == ".c") ? "clang" : "clang++";
        std::string obj = obj_path(file);
        mkdir_p(obj.substr(0, obj.rfind('/')));

        std::string flags = "-std=c++17 -O2 -fPIC";
        flags += " -I" + project_dir + "/src";
        flags += " -I" + project_dir + "/libs/include";
        flags += " -DANDROID -DANDROID_SDK=" + manifest.target_sdk;

        cmd = compiler_bin + " " + flags + " -c " + file
            + " -o " + obj + " 2>" + err_file;

    // ── Java ──
    } else if (e == ".java") {
        if (jvm.kind == JVMKind::NONE) {
            log_err("compile", "No JVM found — cannot compile Java.");
            return 1;
        }
        std::string classes = project_dir + "/build/classes";
        mkdir_p(classes);
        log_info("compile", file + " [java via " +
            (jvm.kind == JVMKind::ART ? "ART/ECJ" :
             jvm.kind == JVMKind::CLASSIC ? "javac" :
             jvm.kind == JVMKind::WINDOWS ? "Windows JVM" : "macOS JVM") + "]");

        int rc = compile_java(file, classes, err_file);
        if (rc != 0) goto handle_error;
        state.mark_ok(file);
        log_ok("compile", "OK: " + file);
        return 0;

    // ── Kotlin ──
    } else if (e == ".kt") {
        if (jvm.kind == JVMKind::NONE) {
            log_err("compile", "No JVM found — cannot compile Kotlin.");
            return 1;
        }
        std::string classes = project_dir + "/build/classes";
        mkdir_p(classes);
        log_info("compile", file + " [kotlin]");

        int rc = compile_kotlin(file, classes, err_file);
        if (rc != 0) goto handle_error;
        state.mark_ok(file);
        log_ok("compile", "OK: " + file);
        return 0;

    } else {
        log_warn("compiler", "Skipping unknown type: " + file);
        return 0;
    }

    // Run C/C++ compile command
    log_info("compile", file);
    {
        int rc = system(cmd.c_str());
        if (rc != 0) goto handle_error;
        log_ok("compile", "OK: " + file);
        state.mark_ok(file);
        return 0;
    }

handle_error:
    {
        std::ifstream ef(err_file);
        std::string stderr_out((std::istreambuf_iterator<char>(ef)),
                                std::istreambuf_iterator<char>());
        std::cerr << stderr_out;

        ErrorResult result = ErrorEngine::parse(stderr_out, file);
        ErrorEngine::explain(result);

        if (result.can_auto_fix) {
            bool fixed = ErrorEngine::apply_fix(result);
            if (fixed) {
                log_fix("autofix", result.auto_fix);
                log_info("compile", "Retrying after auto-fix...");
                int rc2 = system(cmd.c_str());
                if (rc2 == 0) {
                    log_ok("compile", "Fixed and compiled: " + file);
                    state.mark_ok(file);
                    return 0;
                }
                std::ifstream ef2(err_file);
                std::string se2((std::istreambuf_iterator<char>(ef2)),
                                 std::istreambuf_iterator<char>());
                std::cerr << se2;
                ErrorResult r2 = ErrorEngine::parse(se2, file);
                ErrorEngine::explain(r2);
            }
        }

        state.mark_failed(file);
        return 1;
    }
}

// ── compile_all ─────────────────────────────────────────────────

int Compiler::compile_all(const std::vector<std::string>& files, bool resume) {
    mkdir_p(project_dir + "/build/obj");

    // Log detected JVM once
    switch (jvm.kind) {
        case JVMKind::CLASSIC: log_info("jvm", "Using Classic JVM (javac)"); break;
        case JVMKind::ART:     log_info("jvm", "Using ART (dalvikvm + ECJ)"); break;
        case JVMKind::WINDOWS: log_info("jvm", "Using Windows JVM");          break;
        case JVMKind::MACOS:   log_info("jvm", "Using macOS JVM");            break;
        case JVMKind::NONE:    log_warn("jvm", "No JVM — Java/Kotlin skipped"); break;
    }

    for (auto& file : files) {
        if (resume && state.is_done(file)) {
            log_info("resume", "Skip (already OK): " + file);
            continue;
        }

        int rc = compile_one(file);
        if (rc != 0) return rc;
    }

    return 0;
}

