#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>

#include "core/chunker.hpp"
#include "core/embeddings_client.hpp"
#include "core/file_scanner.hpp"
#include "core/vector_store.hpp"

namespace codewizard {

struct IndexingProgress {
    std::string message;
    std::size_t files_found = 0;
    std::size_t chunks_created = 0;
    std::size_t current_chunk = 0;
    std::size_t total_chunks = 0;
};

struct IndexingResult {
    VectorStore store;
    std::filesystem::path index_path;
    std::size_t file_count = 0;
    std::size_t chunk_count = 0;
};

using IndexingProgressCallback = std::function<void(const IndexingProgress&)>;

class Indexer {
public:
    Indexer(
        EmbeddingsClient embeddings_client,
        std::string embedding_model,
        FileScanner scanner = FileScanner{},
        Chunker chunker = Chunker{}
    );

    [[nodiscard]] IndexingResult index_project(
        const std::filesystem::path& project_root,
        const IndexingProgressCallback& on_progress = {}
    ) const;

private:
    void report_progress(
        const IndexingProgressCallback& on_progress,
        IndexingProgress progress
    ) const;

    EmbeddingsClient embeddings_client_;
    std::string embedding_model_;
    FileScanner scanner_;
    Chunker chunker_;
};

} // namespace codewizard
