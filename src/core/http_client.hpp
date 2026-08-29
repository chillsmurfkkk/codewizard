#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace codewizard {

struct HttpResponse {
    long status_code = 0;
    std::string body;
    std::string error;
    std::optional<std::string> retry_after;
};

struct HttpRetryOptions {
    std::size_t max_attempts = 8;
    std::chrono::milliseconds initial_backoff{500};
    std::chrono::milliseconds max_backoff{30000};
};

class HttpClient {
public:
    using RequestFunction = std::function<HttpResponse(
        const std::string&,
        const std::string&,
        const std::string&,
        long
    )>;
    using SleepFunction = std::function<void(std::chrono::milliseconds)>;

    explicit HttpClient(
        long timeout_ms = 60000,
        HttpRetryOptions retry_options = {},
        RequestFunction request = {},
        SleepFunction sleep = {}
    );

    [[nodiscard]] std::string post_json(
        const std::string& url,
        const std::string& api_key,
        const nlohmann::json& body
    ) const;

private:
    long timeout_ms_;
    HttpRetryOptions retry_options_;
    RequestFunction request_;
    SleepFunction sleep_;
};

} // namespace codewizard
