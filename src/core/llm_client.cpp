#include "core/llm_client.hpp"

#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace codewizard {

LlmClient::LlmClient(
    std::string url,
    std::string api_key,
    std::string model,
    double temperature,
    HttpClient http_client
)
    : url_(std::move(url)),
      api_key_(std::move(api_key)),
      model_(std::move(model)),
      temperature_(temperature),
      http_client_(std::move(http_client))
{
}

std::string LlmClient::answer(
    const std::string& question,
    const std::string& context
) const
{
    if (model_.empty()) {
        throw std::runtime_error("LLM model is empty");
    }

    const std::string system_prompt =
        "You are a helpful code assistant. "
        "Answer the user's question using only the provided code context. "
        "If the answer is not present in the context, say that there is not enough information. "
        "Mention relevant files and line numbers when possible. "
        "Do not invent implementation details.";

    nlohmann::json body;
    body["model"] = model_;
    body["temperature"] = temperature_;
    body["messages"] = nlohmann::json::array({
        {
            {"role", "system"},
            {"content", system_prompt}
        },
        {
            {"role", "user"},
            {"content",
                "Code context:\n\n" +
                context +
                "\n\nQuestion:\n" +
                question +
                "\n\nAnswer:"
            }
        }
    });

    const std::string response = http_client_.post_json(url_, api_key_, body);
    const auto json_response = nlohmann::json::parse(response);

    if (!json_response.contains("choices") || !json_response["choices"].is_array() || json_response["choices"].empty()) {
        throw std::runtime_error("LLM response does not contain choices");
    }

    const auto& first_choice = json_response["choices"].at(0);
    if (!first_choice.contains("message") || !first_choice["message"].contains("content")) {
        throw std::runtime_error("LLM response does not contain message content");
    }

    return first_choice["message"]["content"].get<std::string>();
}

} // namespace codewizard
