#include "core/embeddings_client.hpp"

#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace codewizard {

EmbeddingsClient::EmbeddingsClient(
    std::string url,
    std::string api_key,
    std::string model,
    HttpClient http_client
)
    : url_(std::move(url)),
      api_key_(std::move(api_key)),
      model_(std::move(model)),
      http_client_(std::move(http_client))
{
}

std::vector<double> EmbeddingsClient::embed(const std::string& text) const
{
    if (model_.empty()) {
        throw std::runtime_error("Embeddings model is empty");
    }

    nlohmann::json body;
    body["model"] = model_;
    body["input"] = text;

    const std::string response = http_client_.post_json(url_, api_key_, body);
    const auto json_response = nlohmann::json::parse(response);

    if (!json_response.contains("data") || !json_response["data"].is_array() || json_response["data"].empty()) {
        throw std::runtime_error("Embeddings response does not contain data");
    }

    const auto& first_item = json_response["data"].at(0);
    if (!first_item.contains("embedding") || !first_item["embedding"].is_array()) {
        throw std::runtime_error("Embeddings response does not contain an embedding array");
    }

    const auto& embedding_json = first_item["embedding"];

    std::vector<double> embedding;
    embedding.reserve(embedding_json.size());

    for (const auto& value : embedding_json) {
        if (!value.is_number()) {
            throw std::runtime_error("Embedding contains a non-number value");
        }

        embedding.push_back(value.get<double>());
    }

    return embedding;
}

} // namespace codewizard
