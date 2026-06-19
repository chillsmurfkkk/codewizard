#include "core/prompt_builder.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace codewizard {
namespace {

bool is_blank(const std::string& value)
{
    return value.find_first_not_of(" \t\r\n") == std::string::npos;
}

std::string code_fence_language(const std::string& file)
{
    if (file.ends_with(".c") || file.ends_with(".h")) {
        return "c";
    }

    if (
        file.ends_with(".cc") ||
        file.ends_with(".cpp") ||
        file.ends_with(".cxx") ||
        file.ends_with(".cppm") ||
        file.ends_with(".hh") ||
        file.ends_with(".hpp") ||
        file.ends_with(".hxx") ||
        file.ends_with(".ixx")
    ) {
        return "cpp";
    }

    if (file.ends_with(".cmake") || file == "CMakeLists.txt") {
        return "cmake";
    }

    if (file.ends_with(".md")) {
        return "markdown";
    }

    return "";
}

} // namespace

PromptBuilder::PromptBuilder(PromptBuilderOptions options)
    : options_(options)
{
}

std::string PromptBuilder::build_context(const std::vector<SearchResult>& results) const
{
    if (results.empty()) {
        throw std::runtime_error("Cannot build a prompt context without retrieved chunks");
    }

    std::ostringstream context;

    for (std::size_t index = 0; index < results.size(); ++index) {
        if (index > 0) {
            context << "\n\n";
        }

        context << format_result(results[index], index + 1);
    }

    return context.str();
}

std::string PromptBuilder::build_user_prompt(
    const std::string& question,
    const std::vector<SearchResult>& results
) const
{
    if (is_blank(question)) {
        throw std::runtime_error("Cannot build a prompt for an empty question");
    }

    std::ostringstream prompt;
    prompt
        << "Code context:\n\n"
        << build_context(results)
        << "\n\nQuestion:\n"
        << question
        << "\n\nAnswer:";

    return prompt.str();
}

std::string PromptBuilder::format_result(
    const SearchResult& result,
    std::size_t display_index
) const
{
    const auto& chunk = result.chunk;
    const auto language = code_fence_language(chunk.source.file);

    std::ostringstream output;
    output
        << "[" << display_index << "] File: " << chunk.source.file
        << ", lines " << chunk.source.start_line << "-" << chunk.source.end_line;

    if (options_.include_similarity_scores) {
        output << ", score=" << std::fixed << std::setprecision(3) << result.score;
    }

    output
        << "\n```" << language
        << "\n" << chunk.text
        << "\n```";

    return output.str();
}

} // namespace codewizard
