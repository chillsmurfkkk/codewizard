#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/types.hpp"

namespace codewizard {

struct FileScannerOptions {
    std::vector<std::string> supported_extensions = {
        ".c",
        ".cc",
        ".cmake",
        ".cpp",
        ".cppm",
        ".cxx",
        ".h",
        ".hh",
        ".hpp",
        ".hxx",
        ".ixx",
        ".md",
        ".txt"
    };

    std::vector<std::string> supported_filenames = {
        "CMakeLists.txt",
        "Dockerfile",
        "Makefile"
    };

    std::vector<std::string> ignored_directories = {
        ".git",
        ".vs",
        ".idea",
        ".vscode",
        "build",
        "out",
        "bin",
        "obj",
        "cmake-build-debug",
        "cmake-build-release"
    };

    std::uintmax_t max_file_size_bytes = 1024 * 1024;
};

class FileScanner {
public:
    explicit FileScanner(FileScannerOptions options = {});

    [[nodiscard]] std::vector<SourceFile> scan(const std::filesystem::path& project_root) const;

private:
    [[nodiscard]] bool is_supported_file(const std::filesystem::directory_entry& entry) const;
    [[nodiscard]] bool should_skip_directory(const std::filesystem::path& path) const;

    FileScannerOptions options_;
};

} // namespace codewizard
