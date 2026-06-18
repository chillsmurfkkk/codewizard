#include "core/chunker.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace codewizard {
namespace {

std::vector<std::string> read_lines(const SourceFile& file)
{
    std::ifstream input(file.absolute_path);
    if (!input) {
        throw std::runtime_error("Failed to open source file: " + file.absolute_path.string());
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        lines.push_back(std::move(line));
    }

    return lines;
}

std::string join_lines(
    const std::vector<std::string>& lines,
    std::size_t start_index,
    std::size_t end_index
)
{
    std::ostringstream output;

    for (std::size_t index = start_index; index < end_index; ++index) {
        output << lines[index];

        if (index + 1 < end_index) {
            output << '\n';
        }
    }

    return output.str();
}

} // namespace

Chunker::Chunker(ChunkerOptions options)
    : options_(options)
{
    if (options_.max_lines_per_chunk == 0) {
        throw std::runtime_error("Chunk size must be greater than zero");
    }

    if (options_.overlap_lines >= options_.max_lines_per_chunk) {
        throw std::runtime_error("Chunk overlap must be smaller than chunk size");
    }
}

std::vector<CodeChunk> Chunker::chunk_files(const std::vector<SourceFile>& files) const
{
    std::vector<CodeChunk> chunks;

    for (const auto& file : files) {
        auto file_chunks = chunk_file(file, chunks.size());
        chunks.insert(
            chunks.end(),
            std::make_move_iterator(file_chunks.begin()),
            std::make_move_iterator(file_chunks.end())
        );
    }

    return chunks;
}

std::vector<CodeChunk> Chunker::chunk_file(const SourceFile& file, std::size_t first_chunk_id) const
{
    const auto lines = read_lines(file);
    if (lines.empty()) {
        return {};
    }

    std::vector<CodeChunk> chunks;
    const std::size_t step = options_.max_lines_per_chunk - options_.overlap_lines;

    for (std::size_t start_index = 0; start_index < lines.size(); start_index += step) {
        const std::size_t end_index = std::min(start_index + options_.max_lines_per_chunk, lines.size());

        chunks.push_back(CodeChunk{
            first_chunk_id + chunks.size(),
            SourceRange{
                file.relative_path,
                start_index + 1,
                end_index
            },
            join_lines(lines, start_index, end_index)
        });

        if (end_index == lines.size()) {
            break;
        }
    }

    return chunks;
}

} // namespace codewizard
