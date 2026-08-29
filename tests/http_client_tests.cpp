#include "core/http_client.hpp"

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using codewizard::HttpClient;
using codewizard::HttpResponse;
using codewizard::HttpRetryOptions;

class TestFailure : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw TestFailure(message);
    }
}

void test_retries_rate_limit_until_success()
{
    std::vector<HttpResponse> responses{
        {429, "rate limited", {}, "0"},
        {429, "rate limited", {}, "0"},
        {200, R"({"ok":true})", {}, std::nullopt}
    };
    std::size_t request_count = 0;
    std::vector<std::chrono::milliseconds> delays;

    HttpClient client{
        60000,
        HttpRetryOptions{},
        [&](const auto&, const auto&, const auto&, long) {
            return responses.at(request_count++);
        },
        [&](std::chrono::milliseconds delay) {
            delays.push_back(delay);
        }
    };

    const auto result = client.post_json("https://example.test", "key", {{"input", "code"}});
    require(result == R"({"ok":true})", "The successful retry response should be returned");
    require(request_count == 3, "A 429 response should be retried");
    require(delays.size() == 2, "Each failed retryable request should wait");
    require(delays[0] == std::chrono::milliseconds{0}, "Retry-After should control the delay");
}

void test_does_not_retry_permanent_client_error()
{
    std::size_t request_count = 0;
    HttpClient client{
        60000,
        HttpRetryOptions{},
        [&](const auto&, const auto&, const auto&, long) {
            ++request_count;
            return HttpResponse{401, "invalid key", {}, std::nullopt};
        },
        [](std::chrono::milliseconds) {}
    };

    try {
        (void)client.post_json("https://example.test", "key", nlohmann::json::object());
        throw TestFailure("A permanent HTTP error should be thrown");
    } catch (const TestFailure&) {
        throw;
    } catch (const std::runtime_error& error) {
        require(std::string{error.what()}.find("HTTP error 401") != std::string::npos,
            "The HTTP status should be preserved");
    }

    require(request_count == 1, "A permanent client error should not be retried");
}

void test_reports_exhausted_rate_limit_retries()
{
    std::size_t request_count = 0;
    HttpClient client{
        60000,
        HttpRetryOptions{3, std::chrono::milliseconds{0}, std::chrono::milliseconds{0}},
        [&](const auto&, const auto&, const auto&, long) {
            ++request_count;
            return HttpResponse{429, "still limited", {}, std::nullopt};
        },
        [](std::chrono::milliseconds) {}
    };

    try {
        (void)client.post_json("https://example.test", "key", nlohmann::json::object());
        throw TestFailure("An exhausted rate limit should be thrown");
    } catch (const TestFailure&) {
        throw;
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        require(message.find("HTTP error 429") != std::string::npos, "The final status should be preserved");
        require(message.find("after 3 attempts") != std::string::npos, "The attempt count should be reported");
    }

    require(request_count == 3, "Retry attempts should be bounded");
}

void test_does_not_retry_server_or_transport_errors()
{
    for (const auto& response : std::vector<HttpResponse>{
        {503, "unavailable", {}, std::nullopt},
        {0, {}, "connection failed", std::nullopt}
    }) {
        std::size_t request_count = 0;
        HttpClient client{
            60000,
            HttpRetryOptions{},
            [&](const auto&, const auto&, const auto&, long) {
                ++request_count;
                return response;
            },
            [](std::chrono::milliseconds) {}
        };

        try {
            (void)client.post_json("https://example.test", "key", nlohmann::json::object());
            throw TestFailure("A non-rate-limit failure should be thrown");
        } catch (const TestFailure&) {
            throw;
        } catch (const std::runtime_error&) {
        }

        require(request_count == 1, "Only 429 responses should be retried");
    }
}

} // namespace

int main()
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"Retries rate limit until success", test_retries_rate_limit_until_success},
        {"Does not retry permanent client error", test_does_not_retry_permanent_client_error},
        {"Reports exhausted rate limit retries", test_reports_exhausted_rate_limit_retries},
        {"Does not retry server or transport errors", test_does_not_retry_server_or_transport_errors}
    };

    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    return failures == 0 ? 0 : 1;
}
