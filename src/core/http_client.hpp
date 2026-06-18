#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace codewizard {

class HttpClient {
public:
    explicit HttpClient(long timeout_ms = 60000);

    [[nodiscard]] std::string post_json(
        const std::string& url,
        const std::string& api_key,
        const nlohmann::json& body
    ) const;

private:
    long timeout_ms_;
};

} // namespace codewizard
