#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/app_config.hpp"
#include "core/embeddings_client.hpp"
#include "core/indexer.hpp"
#include "core/llm_client.hpp"
#include "core/prompt_builder.hpp"
#include "core/retriever.hpp"
#include "core/types.hpp"

namespace codewizard {

struct RAGPipelineOptions {
    RetrievalOptions retrieval;
    PromptBuilderOptions prompt_builder;
};

struct RAGAnswer {
    std::string answer;
    std::vector<SearchResult> sources;
    std::string context;
    std::filesystem::path index_path;
};

struct AnsweringProgress {
    std::string message;
};

using AnsweringProgressCallback = std::function<void(const AnsweringProgress&)>;

class RAGPipeline {
public:
    explicit RAGPipeline(
        AppConfig config,
        RAGPipelineOptions options = {}
    );

    [[nodiscard]] IndexingResult index_project(
        const std::filesystem::path& project_root,
        const IndexingProgressCallback& on_progress = {}
    ) const;

    [[nodiscard]] RAGAnswer answer_question(
        const std::filesystem::path& project_root,
        const std::string& question,
        const AnsweringProgressCallback& on_progress = {}
    ) const;

private:
    [[nodiscard]] EmbeddingsClient create_embeddings_client() const;
    [[nodiscard]] LlmClient create_llm_client() const;

    AppConfig config_;
    RAGPipelineOptions options_;
};

} // namespace codewizard
