#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "core/types.hpp"

namespace codewizard {

class VectorStore {
public:
    VectorStore() = default;
    explicit VectorStore(ProjectIndex index);

    [[nodiscard]] const ProjectIndex& index() const;
    [[nodiscard]] const std::vector<EmbeddedChunk>& chunks() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;

    void set_metadata(IndexMetadata metadata);
    void set_chunks(std::vector<EmbeddedChunk> chunks);
    void add_chunk(EmbeddedChunk chunk);
    void clear();

    void save(const std::filesystem::path& index_path) const;
    static VectorStore load(const std::filesystem::path& index_path);

    [[nodiscard]] std::vector<SearchResult> search(
        const Embedding& query_embedding,
        std::size_t top_k
    ) const;

    [[nodiscard]] static double cosine_similarity(
        const Embedding& left,
        const Embedding& right
    );

private:
    ProjectIndex index_;
};

[[nodiscard]] std::filesystem::path default_index_path(
    const std::filesystem::path& project_root
);

} // namespace codewizard
