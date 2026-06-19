#include "core/indexer.hpp"

#include <chrono>
#include <future>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace codewizard {
namespace {

constexpr std::size_t max_concurrent_embedding_requests = 5;

struct InFlightEmbedding {
    std::size_t chunk_index = 0;
    std::future<Embedding> embedding;
};

std::filesystem::path canonical_project_root(const std::filesystem::path& project_root)
{
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(project_root, error);
    if (error) {
        throw std::runtime_error("Project path is not a readable directory: " + project_root.string());
    }

    return root;
}

std::optional<std::size_t> find_ready_embedding_request(
    std::vector<InFlightEmbedding>& in_flight
)
{
    for (std::size_t index = 0; index < in_flight.size(); ++index) {
        if (in_flight[index].embedding.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
            return index;
        }
    }

    return std::nullopt;
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
    embedded_chunks.resize(chunks.size());

    std::vector<InFlightEmbedding> in_flight;
    in_flight.reserve(max_concurrent_embedding_requests);

    std::size_t next_chunk = 0;
    std::size_t completed_chunks = 0;

    while (completed_chunks < chunks.size()) {
        while (next_chunk < chunks.size() &&
            in_flight.size() < max_concurrent_embedding_requests) {
            in_flight.push_back(InFlightEmbedding{
                next_chunk,
                std::async(
                    std::launch::async,
                    [this, chunk_text = chunks[next_chunk].text] {
                        return embeddings_client_.embed(chunk_text);
                    }
                )
            });
            ++next_chunk;
        }

        auto ready_index = find_ready_embedding_request(in_flight);
        if (!ready_index) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            continue;
        }

        auto ready = std::move(in_flight[*ready_index]);
        in_flight.erase(in_flight.begin() + static_cast<std::ptrdiff_t>(*ready_index));
        embedded_chunks[ready.chunk_index] = EmbeddedChunk{
            chunks[ready.chunk_index],
            ready.embedding.get()
        };
        ++completed_chunks;

        report_progress(on_progress, IndexingProgress{
            "Embedded chunk " + std::to_string(completed_chunks) + "/" + std::to_string(chunks.size()) +
                " with up to " + std::to_string(max_concurrent_embedding_requests) + " concurrent requests...",
            files.size(),
            chunks.size(),
            completed_chunks,
            chunks.size()
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
