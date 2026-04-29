#include "manifest.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <regex>

// Extract attribute value from XML tag string
static std::string attr(const std::string& tag, const std::string& key) {
    // matches key="value" or key='value'
    std::regex re(key + R"([=]\s*["']([^"']*)["'])");
    std::smatch m;
    if (std::regex_search(tag, m, re)) return m[1].str();
    return "";
}

bool Manifest::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // ── <manifest> tag ──
    std::regex manifest_re(R"(<manifest([^>]*)>)");
    std::smatch mm;
    if (std::regex_search(content, mm, manifest_re)) {
        std::string tag = mm[1].str();
        package      = attr(tag, "package");
        version_name = attr(tag, "android:versionName");
        version_code = attr(tag, "android:versionCode");
    }
    if (package.empty()) return false;

    // ── <uses-sdk> tag ──
    std::regex sdk_re(R"(<uses-sdk([^>]*)/>)");
    std::smatch sm;
    if (std::regex_search(content, sm, sdk_re)) {
        std::string tag = sm[1].str();
        min_sdk    = attr(tag, "android:minSdkVersion");
        target_sdk = attr(tag, "android:targetSdkVersion");
    }

    // ── <application> android:label ──
    std::regex app_re(R"(<application([^>]*)>)");
    std::smatch am;
    if (std::regex_search(content, am, app_re)) {
        std::string tag = am[1].str();
        app_name = attr(tag, "android:label");
        if (app_name.empty()) app_name = package;
        // strip @string/ prefix if present
        if (app_name.rfind("@string/", 0) == 0)
            app_name = app_name.substr(8);
    }

    // ── <uses-permission> tags ──
    std::regex perm_re(R"(<uses-permission([^>]*)/>)");
    auto begin = std::sregex_iterator(content.begin(), content.end(), perm_re);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string perm = attr((*it)[1].str(), "android:name");
        if (!perm.empty()) permissions.push_back(perm);
    }

    // ── Main activity ──
    std::regex act_re(R"(<activity([^>]*)>)");
    std::smatch ac;
    if (std::regex_search(content, ac, act_re)) {
        activity_main = attr(ac[1].str(), "android:name");
    }

    // Defaults
    if (version_name.empty()) version_name = "1.0";
    if (version_code.empty()) version_code = "1";
    if (min_sdk.empty())      min_sdk      = "21";
    if (target_sdk.empty())   target_sdk   = "34";

    return true;
}
