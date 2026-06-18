# codewizard Project Context

## Project Idea

`codewizard` is a local educational RAG application for searching and explaining source code. The user selects a project folder, the app indexes supported files, and then answers questions using relevant code chunks found in that project.

The goal is to demonstrate a complete RAG pipeline in C++ with a minimal Dear ImGui interface:

1. Scan files in a selected folder.
2. Split files into text chunks.
3. Create embeddings through an API.
4. Store chunks and embeddings locally.
5. Search the most relevant chunks for a question.
6. Build an LLM prompt from retrieved context.
7. Show the generated answer and sources in the GUI.

## MVP Scope

Required features:

- Enter or select a project path.
- Recursively scan source files.
- Support these file extensions:
  - `.cpp`
  - `.hpp`
  - `.h`
  - `.c`
  - `.md`
  - `.txt`
- Skip common generated or tool folders:
  - `.git`
  - `build`
  - `.vs`
  - `.idea`
  - `cmake-build-debug`
  - `bin`
  - `obj`
- Split files into chunks.
- Create embeddings through an API.
- Save chunks and embeddings to `.index/index.json`.
- Find the top 3 relevant chunks with cosine similarity.
- Generate an answer through an LLM API.
- Show the answer and source references.
- Show debug information for retrieved chunks:
  - file path
  - line range
  - similarity score
  - short text preview

Explicitly out of scope for the MVP:

- Full AST analysis.
- C++ syntax parsing.
- SQLite or another database.
- PDF support.
- Authentication.
- Complex chat history.
- Multiple advanced modes such as review, refactor, or bug fixing.

## Technology Choices

Initial stack:

- C++20
- CMake
- vcpkg manifest mode
- Dear ImGui for GUI
- GLFW + OpenGL backend for the first GUI implementation
- `cpr` or `libcurl` for HTTP requests
- `nlohmann/json` for JSON
- `std::filesystem` for directory traversal

API keys must not be hardcoded. The app should read configuration from either:

- environment variables, such as `API_KEY`
- a local `config.json` file ignored by git

Planned config shape:

```json
{
  "api_key": "your-api-key",
  "embedding_model": "embedding-model-name",
  "llm_model": "llm-model-name",
  "base_url": "https://api-router-url.com"
}
```

## Architecture

Target module structure:

```text
codewizard
|
+-- app
|   +-- GUI / Dear ImGui application shell
|
+-- core
|   +-- FileScanner
|   +-- Chunker
|   +-- ApiClient
|   +-- VectorStore
|   +-- RAGPipeline
|
+-- storage
    +-- .index/index.json at runtime
```

### FileScanner

Responsibilities:

- Walk a project directory recursively.
- Filter supported file extensions.
- Skip ignored directories.
- Return candidate files with normalized paths.

### Chunker

Responsibilities:

- Read file text line by line.
- Split content into simple overlapping chunks.
- Keep source metadata for each chunk.

Initial chunking strategy:

- 100 lines per chunk by default.
- 20 lines overlap by default.

Example:

```text
Chunk 1: lines 1-100
Chunk 2: lines 81-180
Chunk 3: lines 161-260
```

### ApiClient

Responsibilities:

- Send text to the embeddings endpoint.
- Parse embedding vectors.
- Send prompts to the LLM endpoint.
- Parse generated answers.

This module should hide provider-specific HTTP and JSON details from the rest of the app.

### VectorStore

Responsibilities:

- Store chunks and embeddings in memory.
- Save/load index data from `.index/index.json`.
- Compute cosine similarity.
- Return top-k matching chunks.

Initial JSON shape:

```json
{
  "project_path": "C:/Projects/MyApp",
  "embedding_model": "embedding-model-name",
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

### RAGPipeline

Responsibilities:

- Coordinate indexing:
  1. scan files
  2. chunk files
  3. embed chunks
  4. save index
- Coordinate answering:
  1. embed question
  2. retrieve top 3 chunks
  3. build prompt
  4. call LLM
  5. return answer and sources

Prompt template:

````text
You are a helpful code assistant.
Answer the user's question using only the provided code context.
If the answer is not present in the context, say that there is not enough information.
Mention relevant files and line numbers if possible.

Context:

[1] File: src/api_client.cpp, lines 20-85
```cpp
...
```

Question:
Where are HTTP requests implemented?

Answer:
````

## Minimal GUI

The first screen should be the actual usable tool, not a landing page.

Required controls:

- Project path input.
- `Index Project` button.
- Indexing status text.
- Question input.
- `Ask` button.
- Large answer area.
- Sources list.
- Retrieved chunks debug area.

Approximate layout:

```text
+------------------------------------------------------+
| codewizard                                           |
+------------------------------------------------------+
| Project path: [ C:/Projects/MyApp              ]     |
| [ Index Project ]                                    |
| Status: Indexed 42 chunks from 8 files               |
+------------------------------------------------------+
| Question:                                            |
| [ Where are HTTP requests implemented?         ]     |
| [ Ask ]                                              |
+------------------------------------------------------+
| Answer:                                              |
| HTTP requests are implemented in src/api_client.cpp...|
+------------------------------------------------------+
| Sources:                                             |
| 1. src/api_client.cpp, lines 20-85                   |
| 2. include/api_client.hpp, lines 5-30                |
+------------------------------------------------------+
| Retrieved chunks debug:                              |
| src/api_client.cpp:20-85 score=0.82                  |
+------------------------------------------------------+
```

## Implementation Plan

### Phase 1: Repository and Build Foundation

- Create CMake project skeleton.
- Add `vcpkg.json` manifest.
- Add CMake presets for a local build.
- Add a minimal executable target.
- Wire Dear ImGui + GLFW + OpenGL.
- Verify that an empty ImGui window runs.

### Phase 2: Core Data Types

- Define `SourceFile`, `Chunk`, `SearchResult`, and config structures.
- Add small utility functions for paths and strings.
- Add unit-testable core modules without GUI dependencies.

### Phase 3: File Scanning and Chunking

- Implement `FileScanner`.
- Implement extension filtering and ignored directories.
- Implement `Chunker` with line metadata.
- Add focused tests for scanning and chunking.

### Phase 4: Local Vector Store

- Implement chunk storage.
- Implement JSON serialization/deserialization.
- Implement cosine similarity.
- Implement top-k search.
- Add tests for persistence and retrieval.

### Phase 5: API Client

- Implement config loading from environment/config file.
- Implement embeddings API call.
- Implement LLM API call.
- Keep provider details isolated behind `ApiClient`.
- Add clear errors for missing keys, bad HTTP responses, and malformed JSON.

### Phase 6: RAG Pipeline

- Implement indexing workflow.
- Implement question answering workflow.
- Build prompt from retrieved chunks.
- Return answer, sources, scores, and previews.

### Phase 7: GUI Integration

- Build the minimal ImGui interface.
- Connect `Index Project` to the indexing workflow.
- Connect `Ask` to the answer workflow.
- Display status, answer, sources, and debug chunks.
- Avoid freezing the GUI for long operations where practical.

### Phase 8: Polish and Demo Readiness

- Add README with build and run instructions.
- Add sample config documentation.
- Add error states in the GUI.
- Test on a small local code project.
- Confirm that `.index/index.json` is created and reused.

## First Milestone

The first useful milestone is:

> A CMake/vcpkg C++20 app that opens a Dear ImGui window and can scan/chunk a selected folder locally without calling any API yet.

This milestone proves the repo structure, build chain, GUI shell, and local RAG preparation logic before adding API complexity.
