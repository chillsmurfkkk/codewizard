#pragma once

#include <string>
#include <vector>

#include "core/types.hpp"

namespace codewizard {

struct PromptBuilderOptions {
    bool include_similarity_scores = true;
};

class PromptBuilder {
public:
    explicit PromptBuilder(PromptBuilderOptions options = {});

    [[nodiscard]] std::string build_context(const std::vector<SearchResult>& results) const;
    [[nodiscard]] std::string build_user_prompt(
        const std::string& question,
        const std::vector<SearchResult>& results
    ) const;

private:
    [[nodiscard]] std::string format_result(
        const SearchResult& result,
        std::size_t display_index
    ) const;

    PromptBuilderOptions options_;
};

} // namespace codewizard
