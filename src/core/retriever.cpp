#include "core/retriever.hpp"

#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace codewizard {
namespace {

bool is_blank(const std::string& value)
{
    return value.find_first_not_of(" \t\r\n") == std::string::npos;
}

} // namespace

Retriever::Retriever(
    EmbeddingsClient embeddings_client,
    VectorStore vector_store,
    RetrievalOptions options
)
    : embeddings_client_(std::move(embeddings_client)),
      vector_store_(std::move(vector_store)),
      options_(options)
{
    if (options_.top_k == 0) {
        throw std::runtime_error("Retriever top_k must be greater than zero");
    }
    if (options_.candidate_pool_size < options_.top_k) {
        options_.candidate_pool_size = options_.top_k;
    }
}

std::vector<SearchResult> Retriever::retrieve(const std::string& question) const
{
    if (is_blank(question)) {
        throw std::runtime_error("Question is empty");
    }

    if (vector_store_.empty()) {
        throw std::runtime_error("Vector store is empty. Index a project before asking questions.");
    }

    const auto question_embedding = embeddings_client_.embed(question);
    if (question_embedding.empty()) {
        throw std::runtime_error("Question embedding is empty");
    }

    const auto candidates = vector_store_.search(question_embedding, options_.candidate_pool_size);
    std::vector<SearchResult> results;
    results.reserve(options_.top_k);
    std::unordered_map<std::string, std::size_t> symbol_counts;

    for (const auto& candidate : candidates) {
        const bool is_atomic_symbol = candidate.chunk.metadata.kind == ChunkKind::function ||
            candidate.chunk.metadata.kind == ChunkKind::type_declaration;
        if (candidate.chunk.text.size() < options_.min_chunk_characters && !is_atomic_symbol) {
            continue;
        }

        const auto& symbol = candidate.chunk.metadata.qualified_name;
        if (!symbol.empty() && symbol_counts[symbol] >= options_.max_results_per_symbol) {
            continue;
        }

        results.push_back(candidate);
        if (!symbol.empty()) {
            ++symbol_counts[symbol];
        }
        if (results.size() == options_.top_k) {
            break;
        }
    }

    if (results.empty()) {
        throw std::runtime_error("No matching chunks found in the vector store");
    }

    return results;
}

const VectorStore& Retriever::vector_store() const
{
    return vector_store_;
}

Retriever load_retriever_from_project(
    const std::filesystem::path& project_root,
    EmbeddingsClient embeddings_client,
    RetrievalOptions options
)
{
    return Retriever{
        std::move(embeddings_client),
        VectorStore::load(default_index_path(project_root)),
        options
    };
}

} // namespace codewizard
