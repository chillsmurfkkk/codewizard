# codewizard Project Context

Last updated: 2026-06-19

## What This Project Is

`codewizard` is a local educational RAG application for searching and explaining source code.
The user selects a project folder, the app indexes supported files, stores embeddings locally,
then answers questions using the most relevant code chunks from that project.

The current implementation is a C++20 desktop app with a Dear ImGui interface, CMake/vcpkg
build setup, OpenAI-compatible embeddings and chat-completions clients, local JSON index
persistence, retrieval, prompt construction, and answer generation.

## Current Status

The project is past the original first milestone. The current codebase has an end-to-end RAG
loop:

1. Select or type a project path in the GUI.
2. Click `Index Project`.
3. Scan supported files recursively.
4. Split files into overlapping chunks.
5. Send chunk text to an embeddings endpoint.
6. Save chunks and embeddings to `<project>/.index/index.json`.
7. Ask a question.
8. Embed the question.
9. Retrieve the top matching chunks by cosine similarity.
10. Build prompt context from the retrieved chunks.
11. Send a system message plus user message to the chat-completions endpoint.
12. Show the answer, sources, retrieved chunk debug info, and collapsible prompt context.

## Where To Edit The System Prompt

The system prompt is currently hardcoded in:

```text
src/core/llm_client.cpp
```

Specifically, edit the `system_prompt` string inside:

```cpp
std::string LlmClient::answer(
    const std::string& question,
    const std::string& context
) const
```

Current system prompt:

```text
You are a software engineer explaining an application to a user.
Use only the provided code context to answer the user's question.
Explain what the application does, how the relevant parts work, and where the behavior is implemented.
Keep the answer practical, concise, and easy to understand for someone reading the codebase.
Mention relevant files and line numbers when they help the user navigate the project.
If the provided context is not enough to answer confidently, say that there is not enough information.
Do not invent implementation details.
Do not suggest code edits, patches, or refactors unless the user explicitly asks for them.
Do not use Markdown formatting; write plain text paragraphs and simple lists only when needed.
```

Important distinction:

- `src/core/llm_client.cpp` owns the actual `system` message sent to the LLM.
- `src/core/prompt_builder.cpp` formats the retrieved code context and can build a user prompt,
  but `RAGPipeline` currently uses only `PromptBuilder::build_context(...)`; the final user
  message is assembled in `LlmClient::answer(...)`.

If prompt editing becomes a regular workflow, a good next refactor is to move the system prompt
out of `LlmClient` and into either:

- `AppConfig`, loaded from `config.json`, or
- `PromptBuilder`, so all prompt-related text lives in one module.

## Build And Runtime

### Stack

- C++20
- CMake 3.24+
- vcpkg manifest mode
- Dear ImGui
- GLFW + OpenGL
- `cpr` for HTTP requests
- `nlohmann/json` for JSON
- `std::filesystem` for file traversal and paths

### Build Commands

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

Run:

```powershell
.\build\windows-msvc\Debug\codewizard.exe
```

### Config

The app reads local API settings from ignored `config.json`.
Use `config.example.json` as the template:

```json
{
  "api_key": "put-your-api-key-here",
  "base_url": "https://openrouter.ai/api/v1",
  "embedding_model": "qwen/qwen3-embedding-8b",
  "llm_model": "google/gemini-3.1-flash-lite"
}
```

`AppConfig` defaults live in:

```text
src/core/app_config.hpp
```

`load_app_config(...)` currently reads from `config.json` if present. It does not currently read
environment variables.

Endpoint construction:

- embeddings: `<base_url>/embeddings`
- chat completions: `<base_url>/chat/completions`

## Current Source Layout

```text
src/
  app/
    window_frame.hpp/.cpp       Borderless window movement, resizing, title bar controls

  core/
    app_config.hpp/.cpp         Runtime config and endpoint URL construction
    chunker.hpp/.cpp            Line-based overlapping chunking
    embeddings_client.hpp/.cpp  OpenAI-compatible embeddings API client
    file_scanner.hpp/.cpp       Recursive source file scanning
    http_client.hpp/.cpp        Shared JSON POST wrapper using cpr
    indexer.hpp/.cpp            Scan, chunk, embed, and save index
    llm_client.hpp/.cpp         OpenAI-compatible chat-completions client and system prompt
    prompt_builder.hpp/.cpp     Retrieved context formatting
    rag_pipeline.hpp/.cpp       High-level indexing and answering orchestration
    retriever.hpp/.cpp          Question embedding and top-k retrieval
    types.hpp                   Shared data structures
    vector_store.hpp/.cpp       JSON persistence and cosine similarity search

  main.cpp                      Dear ImGui app, async tasks, folder picker, UI rendering
```

## Core Flow

### Indexing

Entry point:

```text
RAGPipeline::index_project(...)
```

Main implementation:

```text
src/core/indexer.cpp
```

Indexing steps:

1. Canonicalize project root.
2. Scan files with `FileScanner`.
3. Chunk files with `Chunker`.
4. Embed chunks with `EmbeddingsClient`.
5. Store metadata and embedded chunks in `VectorStore`.
6. Save JSON to `<project>/.index/index.json`.

The indexer currently embeds chunks with up to 5 concurrent async requests.

### Answering

Entry point:

```text
RAGPipeline::answer_question(...)
```

Answering steps:

1. Load `<project>/.index/index.json`.
2. Embed the question.
3. Search top `k` chunks, default `top_k = 3`.
4. Build prompt context from retrieved chunks.
5. Call the LLM client.
6. Return answer, sources, prompt context, and index path.

## File Scanning

Implemented in:

```text
src/core/file_scanner.hpp
src/core/file_scanner.cpp
```

Supported extensions:

- `.c`
- `.cc`
- `.cmake`
- `.cpp`
- `.cppm`
- `.cxx`
- `.h`
- `.hh`
- `.hpp`
- `.hxx`
- `.ixx`
- `.md`
- `.txt`

Supported exact filenames:

- `CMakeLists.txt`
- `Dockerfile`
- `Makefile`

Ignored directories:

- `.git`
- `.vs`
- `.idea`
- `.vscode`
- `build`
- `out`
- `bin`
- `obj`
- `cmake-build-debug`
- `cmake-build-release`
- any directory starting with `cmake-build-`

Maximum indexed file size:

```text
1 MiB
```

## Chunking

Implemented in:

```text
src/core/chunker.hpp
src/core/chunker.cpp
```

Current defaults:

- `max_lines_per_chunk = 100`
- `overlap_lines = 20`

Example:

```text
Chunk 1: lines 1-100
Chunk 2: lines 81-180
Chunk 3: lines 161-260
```

Each chunk stores:

- chunk id
- relative file path
- start line
- end line
- chunk text

## Vector Store

Implemented in:

```text
src/core/vector_store.hpp
src/core/vector_store.cpp
```

Runtime index path:

```text
<project>/.index/index.json
```

Current JSON shape:

```json
{
  "project_path": "C:/Projects/MyApp",
  "embedding_model": "qwen/qwen3-embedding-8b",
  "file_count": 8,
  "chunk_count": 42,
  "chunks": [
    {
      "id": 0,
      "file": "src/main.cpp",
      "start_line": 1,
      "end_line": 100,
      "text": "...",
      "embedding": [0.12, -0.45, 0.78]
    }
  ]
}
```

Search uses cosine similarity and `std::partial_sort` to return the highest-scoring results.

## Prompting

Prompt-related code is currently split between two modules.

### System Prompt

Actual system prompt sent to the model:

```text
src/core/llm_client.cpp
```

The chat request body uses:

```json
{
  "model": "...",
  "temperature": 0.2,
  "messages": [
    {
      "role": "system",
      "content": "..."
    },
    {
      "role": "user",
      "content": "Code context:\n\n...\n\nQuestion:\n...\n\nAnswer:"
    }
  ]
}
```

### Retrieved Context Formatting

Implemented in:

```text
src/core/prompt_builder.cpp
```

Context format:

````text
[1] File: src/api_client.cpp, lines 20-85, score=0.823
```cpp
...
```

[2] File: include/api_client.hpp, lines 5-30, score=0.771
```cpp
...
```
````

`PromptBuilder` chooses code fence language for C, C++, CMake, and Markdown files.

## GUI

Implemented mainly in:

```text
src/main.cpp
src/app/window_frame.hpp
src/app/window_frame.cpp
```

Current UI:

- Borderless resizable GLFW window.
- Custom title bar with app title, theme toggle, minimize, and close buttons.
- Dark/light theme toggle.
- Cyrillic-capable Windows font loading.
- Project path input.
- `Browse` folder picker on Windows.
- `Index Project` button.
- Status line.
- Question input.
- `Ask` button.
- Answer panel.
- Sources panel with file, line range, and score.
- Retrieved chunks debug panel with previews.
- Collapsible `Prompt context` section after retrieval.

Indexing and answering run with `std::async`, so the GUI can keep rendering while work is in
progress. Buttons are disabled while a task is running.

## Errors And Edge Cases Currently Handled

- Invalid or unreadable project path.
- Empty API key.
- Empty embeddings model.
- Empty LLM model.
- Empty question.
- Empty vector store.
- Missing or unreadable index file.
- Bad HTTP transport result.
- Non-2xx HTTP responses.
- Malformed embeddings response.
- Malformed LLM response.
- Embedding dimension mismatches are skipped during search.

## Known Gaps / Good Next Steps

High-value next improvements:

- Move the system prompt into configuration or `PromptBuilder`.
- Add tests for `FileScanner`, `Chunker`, `VectorStore`, `PromptBuilder`, and config loading.
- Add environment variable support for API keys.
- Add a visible config validation screen or startup status.
- Prevent asking before an index exists, or show a clearer UI hint.
- Persist UI settings such as theme and last project path.
- Add cancellation for long indexing jobs.
- Add rate-limit/backoff handling for embedding requests.
- Add incremental indexing so unchanged files do not need to be embedded again.
- Consider storing the LLM model and prompt version in answer/debug metadata.
- Consider checking index embedding model compatibility before answering.

## Original MVP Scope

The original MVP scope was:

- Enter or select a project path.
- Recursively scan source files.
- Split files into chunks.
- Create embeddings through an API.
- Save chunks and embeddings locally.
- Find the top 3 relevant chunks with cosine similarity.
- Generate an answer through an LLM API.
- Show the answer and source references.
- Show debug information for retrieved chunks.

This is mostly implemented in the current codebase.

Explicitly still out of scope unless intentionally expanded:

- Full AST analysis.
- C++ syntax parsing.
- SQLite or another database.
- PDF support.
- Authentication.
- Complex chat history.
- Multiple advanced modes such as review, refactor, or bug fixing.
