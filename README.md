# codewizard

`codewizard` is a local educational RAG application for searching and explaining source code. The first milestone is a C++20 Dear ImGui shell built with CMake and vcpkg.

## Prerequisites

- Visual Studio 2022 or Build Tools with the C++ desktop workload.
- CMake 3.24+.
- Official vcpkg checkout at `C:\Users\user\tools\vcpkg`.

## Build

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
```

Run the app:

```powershell
.\build\windows-msvc\Debug\codewizard.exe
```

## Dependencies

Dependencies are managed with vcpkg manifest mode in `vcpkg.json`:

- Dear ImGui with GLFW and OpenGL3 backends
- cpr
- nlohmann/json

Runtime-local files such as `.index/index.json` and `config.json` are intentionally ignored by git.

## API config

The API clients are OpenAI-compatible and read settings from a local ignored `config.json`.
Use `config.example.json` as a template:

```powershell
Copy-Item .\config.example.json .\config.json
```

Then edit `config.json`:

```json
{
  "api_key": "your-api-key",
  "base_url": "https://openrouter.ai/api/v1",
  "embedding_model": "qwen/qwen3-embedding-8b",
  "llm_model": "google/gemini-3.1-flash-lite"
}
```
