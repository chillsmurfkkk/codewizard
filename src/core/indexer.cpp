#include "core/indexer.hpp"

#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace codewizard {
namespace {

std::filesystem::path canonical_project_root(const std::filesystem::path& project_root)
{
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(project_root, error);
    if (error) {
        throw std::runtime_error("Project path is not a readable directory: " + project_root.string());
    }

    return root;
}

} // namespace

Indexer::Indexer(
    EmbeddingsClient embeddings_client,
    std::string embedding_model,
    FileScanner scanner,
    Chunker chunker
)
    : embeddings_client_(std::move(embeddings_client)),
      embedding_model_(std::move(embedding_model)),
      scanner_(std::move(scanner)),
      chunker_(std::move(chunker))
{
    if (embedding_model_.empty()) {
        throw std::runtime_error("Embedding model is empty");
    }
}

IndexingResult Indexer::index_project(
    const std::filesystem::path& project_root,
    const IndexingProgressCallback& on_progress
) const
{
    const auto root = canonical_project_root(project_root);

    report_progress(on_progress, IndexingProgress{
        "Scanning project files...",
        0,
        0,
        0,
        0
    });

    const auto files = scanner_.scan(root);

    report_progress(on_progress, IndexingProgress{
        "Chunking source files...",
        files.size(),
        0,
        0,
        0
    });

    const auto chunks = chunker_.chunk_files(files);

    report_progress(on_progress, IndexingProgress{
        "Embedding chunks...",
        files.size(),
        chunks.size(),
        0,
        chunks.size()
    });

    std::vector<EmbeddedChunk> embedded_chunks;
    embedded_chunks.reserve(chunks.size());

    for (std::size_t index = 0; index < chunks.size(); ++index) {
        report_progress(on_progress, IndexingProgress{
            "Embedding chunk " + std::to_string(index + 1) + "/" + std::to_string(chunks.size()) + "...",
            files.size(),
            chunks.size(),
            index + 1,
            chunks.size()
        });

        embedded_chunks.push_back(EmbeddedChunk{
            chunks[index],
            embeddings_client_.embed(chunks[index].text)
        });
    }

    VectorStore store;
    store.set_metadata(IndexMetadata{
        root.generic_string(),
        embedding_model_,
        files.size(),
        embedded_chunks.size()
    });
    store.set_chunks(std::move(embedded_chunks));

    const auto index_path = default_index_path(root);

    report_progress(on_progress, IndexingProgress{
        "Saving index...",
        files.size(),
        chunks.size(),
        chunks.size(),
        chunks.size()
    });

    store.save(index_path);

    report_progress(on_progress, IndexingProgress{
        "Indexed " + std::to_string(files.size()) + " files and " +
            std::to_string(store.size()) + " chunks.",
        files.size(),
        store.size(),
        store.size(),
        store.size()
    });

    return IndexingResult{
        std::move(store),
        index_path,
        files.size(),
        chunks.size()
    };
}

void Indexer::report_progress(
    const IndexingProgressCallback& on_progress,
    IndexingProgress progress
) const
{
    if (on_progress) {
        on_progress(std::move(progress));
    }
}

} // namespace codewizard
