#pragma once

#include <string>

#include "core/http_client.hpp"

namespace codewizard {

class LlmClient {
public:
    LlmClient(
        std::string url,
        std::string api_key,
        std::string model,
        double temperature = 0.2,
        HttpClient http_client = HttpClient{}
    );

    [[nodiscard]] std::string answer(
        const std::string& question,
        const std::string& context
    ) const;

private:
    std::string url_;
    std::string api_key_;
    std::string model_;
    double temperature_;
    HttpClient http_client_;
};

} // namespace codewizard
