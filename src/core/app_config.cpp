#include "core/app_config.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace codewizard {
namespace {

std::string trim_trailing_slashes(std::string value)
{
    while (!value.empty() && (value.back() == '/' || value.back() == '\\')) {
        value.pop_back();
    }

    return value;
}

void apply_string_if_present(const nlohmann::json& json, const char* key, std::string& target)
{
    if (json.contains(key)) {
        if (!json[key].is_string()) {
            throw std::runtime_error(std::string("Config value must be a string: ") + key);
        }

        target = json[key].get<std::string>();
    }
}

} // namespace

std::string AppConfig::embeddings_url() const
{
    return trim_trailing_slashes(base_url) + "/embeddings";
}

std::string AppConfig::chat_completions_url() const
{
    return trim_trailing_slashes(base_url) + "/chat/completions";
}

AppConfig load_app_config(const std::filesystem::path& config_path)
{
    AppConfig config;

    if (std::filesystem::exists(config_path)) {
        std::ifstream input(config_path);
        if (!input) {
            throw std::runtime_error("Failed to open config file: " + config_path.string());
        }

        const auto json = nlohmann::json::parse(input);

        apply_string_if_present(json, "api_key", config.api_key);
        apply_string_if_present(json, "base_url", config.base_url);
        apply_string_if_present(json, "embedding_model", config.embedding_model);
        apply_string_if_present(json, "llm_model", config.llm_model);
    }

    return config;
}

} // namespace codewizard
