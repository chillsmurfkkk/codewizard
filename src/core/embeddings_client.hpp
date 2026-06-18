#pragma once

#include <string>
#include <vector>

#include "core/http_client.hpp"

namespace codewizard {

class EmbeddingsClient {
public:
    EmbeddingsClient(
        std::string url,
        std::string api_key,
        std::string model,
        HttpClient http_client = HttpClient{}
    );

    [[nodiscard]] std::vector<double> embed(const std::string& text) const;

private:
    std::string url_;
    std::string api_key_;
    std::string model_;
    HttpClient http_client_;
};

} // namespace codewizard
