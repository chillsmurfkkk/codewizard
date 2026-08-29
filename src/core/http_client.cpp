#include "core/http_client.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <random>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include <cpr/cpr.h>

namespace codewizard {
namespace {

HttpResponse perform_request(
    const std::string& url,
    const std::string& api_key,
    const std::string& body,
    long timeout_ms
)
{
    const cpr::Response response = cpr::Post(
        cpr::Url{url},
        cpr::Header{
            {"Authorization", "Bearer " + api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{body},
        cpr::Timeout{timeout_ms}
    );

    std::optional<std::string> retry_after;
    const auto retry_after_header = response.header.find("retry-after");
    if (retry_after_header != response.header.end()) {
        retry_after = retry_after_header->second;
    }

    return HttpResponse{
        response.status_code,
        response.text,
        response.error.code == cpr::ErrorCode::OK ? std::string{} : response.error.message,
        std::move(retry_after)
    };
}

bool is_retryable_status(long status_code)
{
    return status_code == 429;
}

std::optional<std::chrono::milliseconds> parse_retry_after(const std::optional<std::string>& value)
{
    if (!value) {
        return std::nullopt;
    }

    const auto first = value->find_first_not_of(" \t");
    const auto last = value->find_last_not_of(" \t");
    if (first == std::string::npos) {
        return std::nullopt;
    }

    const std::string_view trimmed{value->data() + first, last - first + 1};
    long long seconds = 0;
    const auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), seconds);
    if (result.ec != std::errc{} || result.ptr != trimmed.data() + trimmed.size() || seconds < 0) {
        return std::nullopt;
    }

    constexpr auto maximum_seconds = (std::chrono::milliseconds::max)().count() / 1000;
    return std::chrono::milliseconds{(std::min)(seconds, maximum_seconds) * 1000};
}

std::chrono::milliseconds backoff_delay(
    std::size_t failed_attempt,
    const HttpRetryOptions& options
)
{
    auto delay = options.initial_backoff;
    for (std::size_t attempt = 1; attempt < failed_attempt && delay < options.max_backoff; ++attempt) {
        delay = (std::min)(delay * 2, options.max_backoff);
    }

    thread_local std::mt19937 generator{std::random_device{}()};
    std::uniform_real_distribution<double> jitter{0.5, 1.5};
    return (std::min)(
        std::chrono::milliseconds{static_cast<long long>(delay.count() * jitter(generator))},
        options.max_backoff
    );
}

} // namespace

HttpClient::HttpClient(
    long timeout_ms,
    HttpRetryOptions retry_options,
    RequestFunction request,
    SleepFunction sleep
)
    : timeout_ms_(timeout_ms),
      retry_options_(retry_options),
      request_(request ? std::move(request) : RequestFunction{perform_request}),
      sleep_(sleep ? std::move(sleep) : SleepFunction{[](std::chrono::milliseconds delay) {
          std::this_thread::sleep_for(delay);
      }})
{
    if (retry_options_.max_attempts == 0) {
        throw std::invalid_argument("HTTP max attempts must be greater than zero");
    }

    if (retry_options_.initial_backoff < std::chrono::milliseconds::zero() ||
        retry_options_.max_backoff < retry_options_.initial_backoff) {
        throw std::invalid_argument("HTTP retry backoff is invalid");
    }
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

    const std::string serialized_body = body.dump();
    HttpResponse response;
    for (std::size_t attempt = 1;; ++attempt) {
        response = request_(url, api_key, serialized_body, timeout_ms_);
        if (response.error.empty() && response.status_code >= 200 && response.status_code < 300) {
            return response.body;
        }

        if (!is_retryable_status(response.status_code) || attempt >= retry_options_.max_attempts) {
            const std::string attempts = attempt > 1
                ? " after " + std::to_string(attempt) + " attempts"
                : std::string{};
            if (!response.error.empty()) {
                throw std::runtime_error("HTTP request failed" + attempts + ": " + response.error);
            }

            throw std::runtime_error(
                "HTTP error " + std::to_string(response.status_code) + attempts + ": " + response.body
            );
        }

        const auto retry_after = parse_retry_after(response.retry_after);
        const auto delay = retry_after ? *retry_after : backoff_delay(attempt, retry_options_);
        sleep_(delay);
    }
}

} // namespace codewizard
