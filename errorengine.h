#pragma once
#include <string>

enum class ErrorKind {
    SYNTAX,       // missing semicolon, undeclared var, wrong type → auto-fix
    MISSING_HDR,  // #include not found → explain
    UNDEF_REF,    // undefined reference (linker) → explain
    FETCH,        // network/download error → user must fix
    UNKNOWN
};

struct ErrorResult {
    ErrorKind kind;
    std::string file;
    int line = -1;
    std::string message;      // human-readable explanation
    std::string auto_fix;     // if non-empty, fix was applied
    bool can_auto_fix = false;
};

struct ErrorEngine {
    // Parse raw compiler stderr, return categorized result
    static ErrorResult parse(const std::string& stderr_output, const std::string& src_file);

    // Attempt auto-fix on source file. Returns true if fix was applied.
    static bool apply_fix(ErrorResult& result);

    // Print detailed error to user
    static void explain(const ErrorResult& result);
};
