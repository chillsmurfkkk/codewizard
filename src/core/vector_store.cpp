#include "core/vector_store.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace codewizard {
namespace {

constexpr const char* index_directory_name = ".index";
constexpr const char* index_file_name = "index.json";

nlohmann::json chunk_to_json(const EmbeddedChunk& chunk)
{
    return nlohmann::json{
        {"id", chunk.chunk.id},
        {"file", chunk.chunk.source.file},
        {"start_line", chunk.chunk.source.start_line},
        {"end_line", chunk.chunk.source.end_line},
        {"text", chunk.chunk.text},
        {"embedding", chunk.embedding}
    };
}

EmbeddedChunk chunk_from_json(const nlohmann::json& json)
{
    return EmbeddedChunk{
        CodeChunk{
            json.at("id").get<std::size_t>(),
            SourceRange{
                json.at("file").get<std::string>(),
                json.at("start_line").get<std::size_t>(),
                json.at("end_line").get<std::size_t>()
            },
            json.at("text").get<std::string>()
        },
        json.at("embedding").get<Embedding>()
    };
}

IndexMetadata metadata_from_index_json(const nlohmann::json& json)
{
    IndexMetadata metadata;
    metadata.project_path = json.at("project_path").get<std::string>();
    metadata.embedding_model = json.at("embedding_model").get<std::string>();
    metadata.file_count = json.value("file_count", std::size_t{0});
    metadata.chunk_count = json.value("chunk_count", std::size_t{0});
    return metadata;
}

} // namespace

VectorStore::VectorStore(ProjectIndex index)
    : index_(std::move(index))
{
}

const ProjectIndex& VectorStore::index() const
{
    return index_;
}

const std::vector<EmbeddedChunk>& VectorStore::chunks() const
{
    return index_.chunks;
}

bool VectorStore::empty() const
{
    return index_.chunks.empty();
}

std::size_t VectorStore::size() const
{
    return index_.chunks.size();
}

void VectorStore::set_metadata(IndexMetadata metadata)
{
    index_.metadata = std::move(metadata);
}

void VectorStore::set_chunks(std::vector<EmbeddedChunk> chunks)
{
    index_.chunks = std::move(chunks);
    index_.metadata.chunk_count = index_.chunks.size();
}

void VectorStore::add_chunk(EmbeddedChunk chunk)
{
    index_.chunks.push_back(std::move(chunk));
    index_.metadata.chunk_count = index_.chunks.size();
}

void VectorStore::clear()
{
    index_.chunks.clear();
    index_.metadata.chunk_count = 0;
}

void VectorStore::save(const std::filesystem::path& index_path) const
{
    const auto parent_path = index_path.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path);
    }

    nlohmann::json json;
    json["project_path"] = index_.metadata.project_path;
    json["embedding_model"] = index_.metadata.embedding_model;
    json["file_count"] = index_.metadata.file_count;
    json["chunk_count"] = index_.chunks.size();
    json["chunks"] = nlohmann::json::array();
    for (const auto& chunk : index_.chunks) {
        json["chunks"].push_back(chunk_to_json(chunk));
    }

    std::ofstream output(index_path);
    if (!output) {
        throw std::runtime_error("Failed to open index file for writing: " + index_path.string());
    }

    output << json.dump(2);
    output << '\n';
}

VectorStore VectorStore::load(const std::filesystem::path& index_path)
{
    std::ifstream input(index_path);
    if (!input) {
        throw std::runtime_error("Failed to open index file for reading: " + index_path.string());
    }

    const auto json = nlohmann::json::parse(input);

    if (!json.contains("chunks") || !json["chunks"].is_array()) {
        throw std::runtime_error("Index file does not contain a chunks array: " + index_path.string());
    }

    ProjectIndex index;
    index.metadata = metadata_from_index_json(json);

    for (const auto& chunk_json : json.at("chunks")) {
        index.chunks.push_back(chunk_from_json(chunk_json));
    }

    index.metadata.chunk_count = index.chunks.size();

    return VectorStore{std::move(index)};
}

std::vector<SearchResult> VectorStore::search(
    const Embedding& query_embedding,
    std::size_t top_k
) const
{
    if (query_embedding.empty() || top_k == 0) {
        return {};
    }

    std::vector<SearchResult> results;
    results.reserve(index_.chunks.size());

    for (const auto& chunk : index_.chunks) {
        if (chunk.embedding.empty() || chunk.embedding.size() != query_embedding.size()) {
            continue;
        }

        results.push_back(SearchResult{
            chunk.chunk,
            cosine_similarity(query_embedding, chunk.embedding)
        });
    }

    const auto result_count = std::min(top_k, results.size());
    std::partial_sort(
        results.begin(),
        results.begin() + static_cast<std::ptrdiff_t>(result_count),
        results.end(),
        [](const SearchResult& left, const SearchResult& right) {
            return left.score > right.score;
        }
    );

    results.resize(result_count);
    return results;
}

double VectorStore::cosine_similarity(
    const Embedding& left,
    const Embedding& right
)
{
    if (left.empty() || left.size() != right.size()) {
        return 0.0;
    }

    double dot_product = 0.0;
    double left_norm = 0.0;
    double right_norm = 0.0;

    for (std::size_t index = 0; index < left.size(); ++index) {
        dot_product += left[index] * right[index];
        left_norm += left[index] * left[index];
        right_norm += right[index] * right[index];
    }

    if (left_norm == 0.0 || right_norm == 0.0) {
        return 0.0;
    }

    return dot_product / (std::sqrt(left_norm) * std::sqrt(right_norm));
}

std::filesystem::path default_index_path(const std::filesystem::path& project_root)
{
    return project_root / index_directory_name / index_file_name;
}

} // namespace codewizard
