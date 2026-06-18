#pragma once

#include <filesystem>
#include <string>

namespace codewizard {

struct AppConfig {
    std::string api_key;
    std::string base_url = "https://openrouter.ai/api/v1";
    std::string embedding_model = "qwen/qwen3-embedding-8b";
    std::string llm_model = "google/gemini-3.1-flash-lite";

    [[nodiscard]] std::string embeddings_url() const;
    [[nodiscard]] std::string chat_completions_url() const;
};

[[nodiscard]] AppConfig load_app_config(
    const std::filesystem::path& config_path = "config.json"
);

} // namespace codewizard
