#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace codewizard {

using Embedding = std::vector<double>;

struct SourceFile {
    std::filesystem::path absolute_path;
    std::string relative_path;
    std::uintmax_t size_bytes = 0;
};

struct SourceRange {
    std::string file;
    std::size_t start_line = 1;
    std::size_t end_line = 1;
};

struct CodeChunk {
    std::size_t id = 0;
    SourceRange source;
    std::string text;
};

struct EmbeddedChunk {
    CodeChunk chunk;
    Embedding embedding;
};

struct SearchResult {
    CodeChunk chunk;
    double score = 0.0;
};

struct IndexMetadata {
    std::string project_path;
    std::string embedding_model;
    std::size_t file_count = 0;
    std::size_t chunk_count = 0;
};

struct ProjectIndex {
    IndexMetadata metadata;
    std::vector<EmbeddedChunk> chunks;
};

} // namespace codewizard
