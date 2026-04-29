#include "errorengine.h"
#include "logger.h"
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>

// ── Pattern table ───────────────────────────────────────────────

struct Pattern {
    std::regex  re;
    ErrorKind   kind;
    std::string explanation;
};

static std::vector<Pattern> build_patterns() {
    return {
        // Syntax: missing semicolon
        { std::regex(R"(error: expected ';')"),
          ErrorKind::SYNTAX,
          "Missing semicolon. compiledroid will attempt to auto-fix." },

        // Syntax: undeclared identifier
        { std::regex(R"(error: use of undeclared identifier '([^']+)')"),
          ErrorKind::SYNTAX,
          "Undeclared identifier. Check spelling or missing declaration." },

        // Syntax: expected expression
        { std::regex(R"(error: expected expression)"),
          ErrorKind::SYNTAX,
          "Unexpected token or missing expression." },

        // Syntax: type mismatch
        { std::regex(R"(error: (cannot convert|no viable conversion|incompatible type))"),
          ErrorKind::SYNTAX,
          "Type mismatch. Check variable types." },

        // Missing header
        { std::regex(R"(fatal error: '([^']+)' file not found)"),
          ErrorKind::MISSING_HDR,
          "Header file not found. Make sure the include path is correct or the library is in libs/." },

        // Undefined reference (linker)
        { std::regex(R"(undefined reference to `([^`]+)')"),
          ErrorKind::UNDEF_REF,
          "Linker error: function or symbol is declared but not defined. Check if a .cpp/.c file is missing or a lib is not linked." },

        // Fetch/network errors
        { std::regex(R"((could not resolve|failed to fetch|network error|connection refused|download failed))"),
          ErrorKind::FETCH,
          "Network/fetch error. This cannot be auto-fixed — check your internet connection or dependency source." },
    };
}

// ── Extract file + line from clang error line ───────────────────

static void extract_location(const std::string& line, std::string& file, int& lineno) {
    // Format: /path/to/file.cpp:42:5: error: ...
    std::regex loc_re(R"(^([^:]+):(\d+):\d+: error:)");
    std::smatch m;
    if (std::regex_search(line, m, loc_re)) {
        file   = m[1].str();
        lineno = std::stoi(m[2].str());
    }
}

// ── Parse ───────────────────────────────────────────────────────

ErrorResult ErrorEngine::parse(const std::string& stderr_output, const std::string& src_file) {
    ErrorResult result;
    result.kind = ErrorKind::UNKNOWN;
    result.file = src_file;

    auto patterns = build_patterns();
    std::istringstream ss(stderr_output);
    std::string line;

    while (std::getline(ss, line)) {
        // Try to extract location
        if (result.line < 0)
            extract_location(line, result.file, result.line);

        for (auto& p : patterns) {
            if (std::regex_search(line, p.re)) {
                result.kind    = p.kind;
                result.message = p.explanation;
                result.can_auto_fix = (p.kind == ErrorKind::SYNTAX);
                return result;
            }
        }
    }

    result.message = "Unknown error. Check the output above.";
    return result;
}

// ── Auto-fix ────────────────────────────────────────────────────

bool ErrorEngine::apply_fix(ErrorResult& result) {
    if (!result.can_auto_fix || result.file.empty() || result.line < 0)
        return false;

    // Read file
    std::ifstream fin(result.file);
    if (!fin.is_open()) return false;
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(fin, l)) lines.push_back(l);
    fin.close();

    if (result.line > (int)lines.size()) return false;

    std::string& target = lines[result.line - 1];
    bool fixed = false;

    // Fix 1: missing semicolon — add ; at end of line
    if (result.message.find("semicolon") != std::string::npos) {
        // Strip trailing whitespace
        size_t end = target.find_last_not_of(" \t\r\n");
        if (end != std::string::npos && target[end] != ';' &&
            target[end] != '{' && target[end] != '}' && target[end] != ',') {
            target = target.substr(0, end + 1) + ";";
            result.auto_fix = "Added missing ';' at line " + std::to_string(result.line);
            fixed = true;
        }
    }

    if (!fixed) return false;

    // Write back
    std::ofstream fout(result.file);
    if (!fout.is_open()) return false;
    for (size_t i = 0; i < lines.size(); i++) {
        fout << lines[i];
        if (i + 1 < lines.size()) fout << "\n";
    }
    return true;
}

// ── Explain ─────────────────────────────────────────────────────

void ErrorEngine::explain(const ErrorResult& result) {
    std::string loc = result.file;
    if (result.line >= 0) loc += ":" + std::to_string(result.line);

    switch (result.kind) {
        case ErrorKind::SYNTAX:
            log_warn("error", "Syntax error in " + loc);
            break;
        case ErrorKind::MISSING_HDR:
            log_err("error", "Missing header in " + loc);
            break;
        case ErrorKind::UNDEF_REF:
            log_err("error", "Undefined reference in " + loc);
            break;
        case ErrorKind::FETCH:
            log_err("error", "Fetch/network error");
            break;
        default:
            log_err("error", "Error in " + loc);
    }

    std::cout << C_YELLOW << "  → " << C_RESET << result.message << "\n";

    if (result.kind == ErrorKind::FETCH) {
        std::cout << C_RED << C_BOLD
                  << "  ✗ You must fix this manually. Run with --resume after fixing.\n"
                  << C_RESET;
    } else if (!result.auto_fix.empty()) {
        std::cout << C_GREEN << "  ✓ Auto-fix applied: " << result.auto_fix << C_RESET << "\n";
    } else if (result.can_auto_fix) {
        std::cout << C_YELLOW << "  ✗ Auto-fix attempted but could not be applied automatically.\n"
                  << "    Fix manually at " << loc << " then run with --resume.\n" << C_RESET;
    } else {
        std::cout << C_YELLOW << "  Fix manually at " << loc
                  << " then run with --resume.\n" << C_RESET;
    }
}
