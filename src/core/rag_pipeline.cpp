#include "core/rag_pipeline.hpp"

#include <stdexcept>
#include <system_error>
#include <utility>

#include "core/embeddings_client.hpp"
#include "core/llm_client.hpp"
#include "core/vector_store.hpp"

namespace codewizard {
namespace {

std::filesystem::path canonical_project_root(const std::filesystem::path& project_root)
{
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(project_root, error);
    if (error) {
        throw std::runtime_error("Project path is not a readable directory: " + project_root.string());
    }

    return root;
}

} // namespace

RAGPipeline::RAGPipeline(
    AppConfig config,
    RAGPipelineOptions options
)
    : config_(std::move(config)),
      options_(options)
{
}

IndexingResult RAGPipeline::index_project(
    const std::filesystem::path& project_root,
    const IndexingProgressCallback& on_progress
) const
{
    const Indexer indexer{
        create_embeddings_client(),
        config_.embedding_model
    };

    return indexer.index_project(project_root, on_progress);
}

RAGAnswer RAGPipeline::answer_question(
    const std::filesystem::path& project_root,
    const std::string& question,
    const AnsweringProgressCallback& on_progress
) const
{
    if (on_progress) {
        on_progress(AnsweringProgress{"Loading project index..."});
    }

    const auto root = canonical_project_root(project_root);
    const auto index_path = default_index_path(root);
    auto retriever = load_retriever_from_project(
        root,
        create_embeddings_client(),
        options_.retrieval
    );

    if (on_progress) {
        on_progress(AnsweringProgress{"Embedding question and retrieving chunks..."});
    }

    auto sources = retriever.retrieve(question);

    if (on_progress) {
        on_progress(AnsweringProgress{
            "Building prompt from " + std::to_string(sources.size()) + " retrieved chunks..."
        });
    }

    const PromptBuilder prompt_builder{options_.prompt_builder};
    auto context = prompt_builder.build_context(sources);

    if (on_progress) {
        on_progress(AnsweringProgress{"Calling language model..."});
    }

    auto llm_client = create_llm_client();
    auto answer = llm_client.answer(question, context);

    if (on_progress) {
        on_progress(AnsweringProgress{"Answer ready."});
    }

    return RAGAnswer{
        std::move(answer),
        std::move(sources),
        std::move(context),
        index_path
    };
}

EmbeddingsClient RAGPipeline::create_embeddings_client() const
{
    return EmbeddingsClient{
        config_.embeddings_url(),
        config_.api_key,
        config_.embedding_model
    };
}

LlmClient RAGPipeline::create_llm_client() const
{
    return LlmClient{
        config_.chat_completions_url(),
        config_.api_key,
        config_.llm_model
    };
}

} // namespace codewizard
