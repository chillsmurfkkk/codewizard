#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "core/embeddings_client.hpp"
#include "core/types.hpp"
#include "core/vector_store.hpp"

namespace codewizard {

struct RetrievalOptions {
    std::size_t top_k = 3;
};

class Retriever {
public:
    Retriever(
        EmbeddingsClient embeddings_client,
        VectorStore vector_store,
        RetrievalOptions options = {}
    );

    [[nodiscard]] std::vector<SearchResult> retrieve(const std::string& question) const;

    [[nodiscard]] const VectorStore& vector_store() const;

private:
    EmbeddingsClient embeddings_client_;
    VectorStore vector_store_;
    RetrievalOptions options_;
};

[[nodiscard]] Retriever load_retriever_from_project(
    const std::filesystem::path& project_root,
    EmbeddingsClient embeddings_client,
    RetrievalOptions options = {}
);

} // namespace codewizard
