# codewizard

`codewizard` is a local desktop RAG app for asking questions about source code.

It scans a project folder, creates a local index, and uses an LLM to answer questions with source references.

## What you need

- Windows.
- Visual Studio 2022 or Build Tools with the C++ desktop workload.
- CMake 3.24+.
- vcpkg.
- API key for an OpenAI-compatible provider.

## Setup

Copy the example config:

```powershell
Copy-Item .\config.example.json .\config.json
```

Edit `config.json`:

```json
{
  "api_key": "your-api-key",
  "base_url": "https://openrouter.ai/api/v1",
  "embedding_model": "qwen/qwen3-embedding-8b",
  "llm_model": "google/gemini-3.1-flash-lite"
}
```

## Build

Set `VCPKG_ROOT` to your local vcpkg folder:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
```

Then configure and build with the Windows preset:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
```

The preset uses Visual Studio 2022 on Windows. The vcpkg path is taken from `VCPKG_ROOT`, so it does not need to be the same on every machine.

## Run

```powershell
.\build\windows-msvc\Release\codewizard.exe
```

For development builds, use `windows-msvc-debug` and run the executable from `build\windows-msvc\Debug`.

## How to use

1. Select a project folder.
2. Click `Index Project`.
3. Ask a question about the code.
4. Read the answer and check the source references.

The index is saved inside the selected project:

```text
<project>/.index/index.json
```

## Notes

- `config.json` is local and ignored by git.
- `.index/index.json` is local and ignored by git.
- Architecture details are documented separately in `docs/architecture.md`.
