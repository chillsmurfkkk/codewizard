#include "core/retriever.hpp"

#include <stdexcept>
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

    auto results = vector_store_.search(question_embedding, options_.top_k);
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
