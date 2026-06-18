#include "core/http_client.hpp"

#include <stdexcept>
#include <utility>

#include <cpr/cpr.h>

namespace codewizard {

HttpClient::HttpClient(long timeout_ms)
    : timeout_ms_(timeout_ms)
{
}

std::string HttpClient::post_json(
    const std::string& url,
    const std::string& api_key,
    const nlohmann::json& body
) const
{
    if (url.empty()) {
        throw std::runtime_error("HTTP URL is empty");
    }

    if (api_key.empty()) {
        throw std::runtime_error("API key is empty");
    }

    const cpr::Response response = cpr::Post(
        cpr::Url{url},
        cpr::Header{
            {"Authorization", "Bearer " + api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{body.dump()},
        cpr::Timeout{timeout_ms_}
    );

    if (response.error.code != cpr::ErrorCode::OK) {
        throw std::runtime_error("HTTP request failed: " + response.error.message);
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        throw std::runtime_error(
            "HTTP error " + std::to_string(response.status_code) + ": " + response.text
        );
    }

    return response.text;
}

} // namespace codewizard
