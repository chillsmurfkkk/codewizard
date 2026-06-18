#include "core/file_scanner.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>

namespace codewizard {
namespace {

std::string to_lower_ascii(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    return value;
}

std::string generic_relative_path(
    const std::filesystem::path& path,
    const std::filesystem::path& root
)
{
    std::error_code error;
    auto relative = std::filesystem::relative(path, root, error);
    if (error) {
        relative = path.filename();
    }

    return relative.generic_string();
}

} // namespace

FileScanner::FileScanner(FileScannerOptions options)
    : options_(std::move(options))
{
    for (auto& extension : options_.supported_extensions) {
        extension = to_lower_ascii(extension);
    }
}

std::vector<SourceFile> FileScanner::scan(const std::filesystem::path& project_root) const
{
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(project_root, error);
    if (error || !std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
        throw std::runtime_error("Project path is not a readable directory: " + project_root.string());
    }

    std::vector<SourceFile> files;

    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error
    );
    std::filesystem::recursive_directory_iterator end;

    while (iterator != end) {
        const auto& entry = *iterator;

        if (entry.is_directory(error)) {
            if (!error && should_skip_directory(entry.path())) {
                iterator.disable_recursion_pending();
            }
        } else if (!error && is_supported_file(entry)) {
            files.push_back(SourceFile{
                entry.path(),
                generic_relative_path(entry.path(), root),
                entry.file_size(error)
            });
        }

        iterator.increment(error);
        error.clear();
    }

    std::ranges::sort(files, [](const SourceFile& left, const SourceFile& right) {
        return left.relative_path < right.relative_path;
    });

    return files;
}

bool FileScanner::is_supported_file(const std::filesystem::directory_entry& entry) const
{
    std::error_code error;
    if (!entry.is_regular_file(error) || error) {
        return false;
    }

    const auto file_size = entry.file_size(error);
    if (error || file_size > options_.max_file_size_bytes) {
        return false;
    }

    const auto extension = to_lower_ascii(entry.path().extension().string());
    return std::ranges::find(options_.supported_extensions, extension) != options_.supported_extensions.end();
}

bool FileScanner::should_skip_directory(const std::filesystem::path& path) const
{
    const auto directory_name = to_lower_ascii(path.filename().string());

    if (directory_name.starts_with("cmake-build-")) {
        return true;
    }

    return std::ranges::any_of(options_.ignored_directories, [&](const std::string& ignored) {
        return directory_name == to_lower_ascii(ignored);
    });
}

} // namespace codewizard
