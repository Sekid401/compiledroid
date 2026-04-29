#include "packager.h"
#include "logger.h"
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <vector>

static bool cmd_exists(const std::string& cmd) {
    std::string check = "command -v " + cmd + " >/dev/null 2>&1";
    return system(check.c_str()) == 0;
}

static void mkdir_p(const std::string& p) {
    system(("mkdir -p " + p).c_str());
}

static bool dir_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Returns list of *.flat files in dir, empty if none
static std::vector<std::string> collect_flats(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (!d) return out;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name(ent->d_name);
        if (name.size() > 5 && name.substr(name.size() - 5) == ".flat")
            out.push_back(dir + "/" + name);
    }
    closedir(d);
    return out;
}

std::string Packager::apk_path() const {
    return project_dir + "/" + manifest.app_name + ".apk";
}

std::string Packager::unsigned_apk_path() const {
    return project_dir + "/build/" + manifest.app_name + "-unsigned.apk";
}

int Packager::run_aapt2() {
    if (!cmd_exists("aapt2")) {
        log_warn("packager", "aapt2 not found — skipping resource compilation");
        log_warn("packager", "Install: pkg install aapt2");
        return 0; // non-fatal for native-only apps
    }

    std::string res_dir     = project_dir + "/res";
    std::string compiled_res = project_dir + "/build/res_compiled";
    std::string manifest_xml = project_dir + "/manifest.xml";
    mkdir_p(compiled_res);

    // Compile resources — skip if no res/ dir
    if (dir_exists(res_dir)) {
        log_info("aapt2", "Compiling resources...");
        std::string compile_cmd = "aapt2 compile --dir " + res_dir
            + " -o " + compiled_res + " 2>&1";
        if (system(compile_cmd.c_str()) != 0) {
            log_err("aapt2", "Resource compilation failed.");
            return 1;
        }
    } else {
        log_info("aapt2", "No res/ directory — skipping resource compilation");
    }

    // Collect .flat files
    std::vector<std::string> flats = collect_flats(compiled_res);

    // If no flats, skip aapt2 link entirely — dex_classes will create the APK shell
    if (flats.empty()) {
        log_info("aapt2", "No compiled resources — skipping APK link (dex step will create APK)");
        return 0;
    }

    // Resolve android.jar
    std::string android_jar;
    const char* android_home = getenv("ANDROID_HOME");
    const char* pfx = getenv("PREFIX");
    if (android_home) {
        std::string flat_jar = std::string(android_home) + "/android.jar";
        std::string sdk_jar  = std::string(android_home) + "/platforms/android-"
                             + manifest.target_sdk + "/android.jar";
        struct stat st{};
        if (stat(flat_jar.c_str(), &st) == 0) android_jar = flat_jar;
        else if (stat(sdk_jar.c_str(), &st) == 0) android_jar = sdk_jar;
    }
    if (android_jar.empty() && pfx) {
        android_jar = std::string(pfx) + "/share/compiledroid/sdk/android.jar";
    }
    struct stat st{};
    if (android_jar.empty() || stat(android_jar.c_str(), &st) != 0) {
        log_err("aapt2", "android.jar not found. Download it:");
        log_err("aapt2", "  curl -L -o $PREFIX/share/compiledroid/sdk/android.jar "
                         "https://github.com/Sable/android-platforms/raw/master/android-34/android.jar");
        return 1;
    }
    log_info("aapt2", "Using: " + android_jar);

    // Link — build flat list explicitly (no glob)
    log_info("aapt2", "Linking APK...");
    mkdir_p(project_dir + "/build/gen");
    std::string link_cmd = "aapt2 link"
        " -o " + unsigned_apk_path() +
        " -I " + android_jar +
        " --manifest " + manifest_xml;
    for (auto& f : flats) link_cmd += " " + f;
    link_cmd += " --java " + project_dir + "/build/gen"
                " 2>&1";
    if (system(link_cmd.c_str()) != 0) {
        log_err("aapt2", "APK link failed.");
        return 1;
    }
    return 0;
}

int Packager::dex_classes() {
    std::string classes_dir = project_dir + "/build/classes";
    std::string dex_out     = project_dir + "/build/dex";
    std::string dex_file    = dex_out + "/classes.dex";

    // Check if there are any .class files to dex
    FILE* chk = popen(("find " + classes_dir + " -name '*.class' 2>/dev/null | head -1").c_str(), "r");
    if (!chk) return 0;
    char buf[512] = {};
    fgets(buf, sizeof(buf), chk);
    pclose(chk);
    if (buf[0] == '\0') {
        log_info("dex", "No .class files — skipping DEX conversion");
        return 0;
    }

    if (!cmd_exists("d8")) {
        log_err("dex", "d8 not found. Install: pkg install d8");
        return 1;
    }

    mkdir_p(dex_out);
    log_info("dex", "Converting .class files to DEX...");

    // Resolve android.jar for d8 desugaring
    std::string android_jar;
    const char* android_home = getenv("ANDROID_HOME");
    const char* pfx = getenv("PREFIX");
    if (android_home) {
        std::string a = std::string(android_home) + "/android.jar";
        std::string b = std::string(android_home) + "/platforms/android-"
                      + manifest.target_sdk + "/android.jar";
        struct stat st{};
        if (stat(a.c_str(), &st) == 0) android_jar = a;
        else if (stat(b.c_str(), &st) == 0) android_jar = b;
    }
    if (android_jar.empty() && pfx) {
        android_jar = std::string(pfx) + "/share/compiledroid/sdk/android.jar";
    }

    // Collect all .class files
    std::string class_list;
    FILE* p = popen(("find " + classes_dir + " -name '*.class' 2>/dev/null").c_str(), "r");
    if (!p) return 1;
    char line[1024];
    while (fgets(line, sizeof(line), p)) {
        std::string s(line);
        if (!s.empty() && s.back() == '\n') s.pop_back();
        class_list += " " + s;
    }
    pclose(p);

    std::string d8_cmd = "d8 --output " + dex_out;
    if (!android_jar.empty()) d8_cmd += " --lib " + android_jar;
    d8_cmd += class_list + " 2>&1";
    if (system(d8_cmd.c_str()) != 0) {
        log_err("dex", "DEX conversion failed.");
        return 1;
    }

    // Inject classes.dex into the unsigned APK
    std::string apk = unsigned_apk_path();
    struct stat st2{};
    if (stat(apk.c_str(), &st2) != 0) {
        // No APK from aapt2 (no resources) — create one with just the manifest via aapt2 link
        log_info("dex", "No unsigned APK — creating manifest-only APK via aapt2...");
        std::string android_jar_for_link = android_jar;
        if (android_jar_for_link.empty() && pfx)
            android_jar_for_link = std::string(pfx) + "/share/compiledroid/sdk/android.jar";
        // Ensure output dir exists
        mkdir_p(project_dir + "/build");
        mkdir_p(project_dir + "/build/gen");
        std::string link_cmd = "aapt2 link"
            " -o " + apk +
            " -I " + android_jar_for_link +
            " --manifest " + project_dir + "/manifest.xml"
            " --java " + project_dir + "/build/gen"
            " 2>&1";
        if (system(link_cmd.c_str()) != 0) {
            // aapt2 failed — fall back to raw zip (won't install but at least won't crash here)
            log_warn("dex", "aapt2 manifest-only link failed — creating raw APK shell");
            std::string zip_cmd = "zip -j " + apk + " " + dex_out + "/classes.dex 2>&1";
            if (system(zip_cmd.c_str()) != 0) {
                log_err("dex", "Failed to create APK shell.");
                return 1;
            }
            log_ok("dex", "classes.dex ready (raw APK — may not install)");
            return 0;
        }
    }

    log_info("dex", "Injecting classes.dex into APK...");
    // Use absolute path - zip is run from dex_out so relative paths break
    std::string abs_apk = apk;
    if (apk[0] != '/') abs_apk = project_dir + "/../" + apk; // already relative to project
    std::string zip_cmd = "zip -j " + apk + " " + dex_out + "/classes.dex 2>&1";
    if (system(zip_cmd.c_str()) != 0) {
        log_err("dex", "Failed to inject classes.dex.");
        return 1;
    }

    log_ok("dex", "classes.dex ready");
    return 0;
}

int Packager::link_native() {
    // Collect .o files
    std::string obj_dir = project_dir + "/build/obj";
    std::string lib_out = project_dir + "/build/lib/lib" + manifest.app_name + ".so";
    mkdir_p(project_dir + "/build/lib");

    std::string objs_cmd = "ls " + obj_dir + "/*.o 2>/dev/null";
    FILE* p = popen(objs_cmd.c_str(), "r");
    if (!p) return 0;
    std::string objs;
    char buf[512];
    while (fgets(buf, sizeof(buf), p)) {
        std::string s(buf);
        if (!s.empty() && s.back() == '\n') s.pop_back();
        objs += " " + s;
    }
    pclose(p);

    if (objs.empty()) {
        log_info("linker", "No .o files — skipping native link");
        return 0;
    }

    std::string lib_flags = "-L" + project_dir + "/libs";
    std::string link_cmd = "clang++ -shared -fPIC -o " + lib_out
        + objs + " " + lib_flags + " -landroid -llog 2>&1";

    log_info("linker", "Linking native library...");
    if (system(link_cmd.c_str()) != 0) {
        log_err("linker", "Native link failed.");
        return 1;
    }
    log_ok("linker", "lib" + manifest.app_name + ".so");
    return 0;
}

int Packager::sign_apk() {
    if (!cmd_exists("apksigner")) {
        log_warn("packager", "apksigner not found — APK will be unsigned");
        log_warn("packager", "Install: pkg install apksigner");
        // Copy unsigned as final
        system(("cp " + unsigned_apk_path() + " " + apk_path()).c_str());
        return 0;
    }

    // Generate a debug keystore if not present
    std::string keystore = project_dir + "/build/debug.keystore";
    struct stat st{};
    if (stat(keystore.c_str(), &st) != 0) {
        log_info("sign", "Generating debug keystore...");
        std::string keytool_cmd =
            "keytool -genkey -v -keystore " + keystore +
            " -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000"
            " -storepass android -keypass android"
            " -dname \"CN=Android Debug,O=Android,C=US\" 2>&1";
        system(keytool_cmd.c_str());
    }

    log_info("sign", "Signing APK...");
    std::string sign_cmd = "apksigner sign"
        " --ks " + keystore +
        " --ks-pass pass:android"
        " --key-pass pass:android"
        " --out " + apk_path() +
        " " + unsigned_apk_path() + " 2>&1";

    if (system(sign_cmd.c_str()) != 0) {
        log_err("sign", "APK signing failed.");
        return 1;
    }
    log_ok("sign", "Signed: " + apk_path());
    return 0;
}

int Packager::pack(const std::vector<std::string>& res_files,
                   const std::vector<std::string>& lib_files) {
    mkdir_p(project_dir + "/build");

    int rc;

    // 1. Link native .o → .so
    rc = link_native();
    if (rc != 0) return rc;

    // 2. Package resources + manifest with aapt2
    rc = run_aapt2();
    if (rc != 0) return rc;

    // 3. Convert .class → classes.dex and inject into APK
    rc = dex_classes();
    if (rc != 0) return rc;

    // 4. Sign
    rc = sign_apk();
    return rc;
}
