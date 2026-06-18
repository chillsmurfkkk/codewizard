#pragma once

#include <cstddef>
#include <vector>

#include "core/types.hpp"

namespace codewizard {

struct ChunkerOptions {
    std::size_t max_lines_per_chunk = 100;
    std::size_t overlap_lines = 20;
};

class Chunker {
public:
    explicit Chunker(ChunkerOptions options = {});

    [[nodiscard]] std::vector<CodeChunk> chunk_files(const std::vector<SourceFile>& files) const;
    [[nodiscard]] std::vector<CodeChunk> chunk_file(const SourceFile& file, std::size_t first_chunk_id) const;

private:
    ChunkerOptions options_;
};

} // namespace codewizard
