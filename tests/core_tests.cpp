#include "core/chunker.hpp"
#include "core/file_scanner.hpp"
#include "core/vector_store.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

using codewizard::CodeChunk;
using codewizard::EmbeddedChunk;
using codewizard::Embedding;
using codewizard::FileScanner;
using codewizard::FileScannerOptions;
using codewizard::IndexMetadata;
using codewizard::SourceFile;
using codewizard::SourceRange;
using codewizard::VectorStore;

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

void require_equal(std::size_t actual, std::size_t expected, const std::string& label)
{
    if (actual != expected) {
        std::ostringstream message;
        message << label << ": expected " << expected << ", got " << actual;
        throw TestFailure(message.str());
    }
}

void require_equal(const std::string& actual, const std::string& expected, const std::string& label)
{
    if (actual != expected) {
        std::ostringstream message;
        message << label << ": expected \"" << expected << "\", got \"" << actual << "\"";
        throw TestFailure(message.str());
    }
}

void require_near(double actual, double expected, double tolerance, const std::string& label)
{
    if (std::fabs(actual - expected) > tolerance) {
        std::ostringstream message;
        message << label << ": expected " << expected << ", got " << actual;
        throw TestFailure(message.str());
    }
}

class TempDir {
public:
    TempDir()
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() / ("codewizard_tests_" + std::to_string(now));
        fs::create_directories(path_);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    ~TempDir()
    {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    [[nodiscard]] const fs::path& path() const
    {
        return path_;
    }

private:
    fs::path path_;
};

void write_file(const fs::path& path, std::string_view contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to create test file: " + path.string());
    }

    output << contents;
}

std::string numbered_lines(std::size_t count)
{
    std::ostringstream output;
    for (std::size_t line = 1; line <= count; ++line) {
        output << "line " << line;
        if (line < count) {
            output << '\n';
        }
    }

    return output.str();
}

std::set<std::string> relative_paths(const std::vector<SourceFile>& files)
{
    std::set<std::string> paths;
    for (const auto& file : files) {
        paths.insert(file.relative_path);
    }

    return paths;
}

EmbeddedChunk embedded_chunk(std::size_t id, std::string file, Embedding embedding)
{
    return EmbeddedChunk{
        CodeChunk{
            id,
            SourceRange{std::move(file), id + 1, id + 1},
            "chunk " + std::to_string(id)
        },
        std::move(embedding)
    };
}

void test_file_scanner_filters_supported_files()
{
    TempDir temp;

    write_file(temp.path() / "src" / "main.CPP", "int main() {}\n");
    write_file(temp.path() / "include" / "app.hpp", "#pragma once\n");
    write_file(temp.path() / "docs" / "notes.md", "notes\n");
    write_file(temp.path() / "CMakeLists.txt", "cmake\n");
    write_file(temp.path() / "Dockerfile", "from scratch\n");
    write_file(temp.path() / "scripts" / "build.py", "print('skip')\n");
    write_file(temp.path() / "src" / "huge.cpp", std::string(128, 'x'));
    write_file(temp.path() / "build" / "generated.cpp", "skip\n");
    write_file(temp.path() / ".git" / "ignored.cpp", "skip\n");
    write_file(temp.path() / "cmake-build-local" / "cache.cpp", "skip\n");

    FileScannerOptions options;
    options.max_file_size_bytes = 64;

    const auto files = FileScanner{options}.scan(temp.path());
    const auto paths = relative_paths(files);

    const std::set<std::string> expected{
        "CMakeLists.txt",
        "Dockerfile",
        "docs/notes.md",
        "include/app.hpp",
        "src/main.CPP"
    };

    require(paths == expected, "FileScanner should include supported files and skip ignored/large files");
    require_equal(files.size(), expected.size(), "FileScanner result count");
}

void test_file_scanner_honors_project_gitignore()
{
    TempDir temp;
    write_file(temp.path() / ".gitignore",
        "ignored-dir/\n"
        "*.generated.cpp\n"
        "!ignored-dir/keep.cpp\n"
        "src/private/*.cpp\n");
    write_file(temp.path() / "src" / "main.cpp", "int main() {}\n");
    write_file(temp.path() / "src" / "private" / "secret.cpp", "skip\n");
    write_file(temp.path() / "generated.generated.cpp", "skip\n");
    write_file(temp.path() / "ignored-dir" / "drop.cpp", "skip\n");
    write_file(temp.path() / "ignored-dir" / "keep.cpp", "keep\n");

    const auto files = FileScanner{}.scan(temp.path());
    const auto paths = relative_paths(files);
    const std::set<std::string> expected{"ignored-dir/keep.cpp", "src/main.cpp"};
    require(paths == expected, "FileScanner should honor the project's .gitignore rules");
}

void test_chunker_uses_expected_overlap()
{
    TempDir temp;
    const auto file_path = temp.path() / "src" / "sample.cpp";
    write_file(file_path, numbered_lines(250));

    const SourceFile file{file_path, "src/sample.cpp", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{{100, 20}}.chunk_file(file, 7);

    require_equal(chunks.size(), 3, "Chunk count");

    require_equal(chunks[0].id, 7, "First chunk id");
    require_equal(chunks[0].source.start_line, 1, "First chunk start");
    require_equal(chunks[0].source.end_line, 100, "First chunk end");
    require(chunks[0].text.starts_with("line 1\nline 2"), "First chunk should start at line 1");
    require(chunks[0].text.ends_with("line 100"), "First chunk should end at line 100");

    require_equal(chunks[1].id, 8, "Second chunk id");
    require_equal(chunks[1].source.start_line, 81, "Second chunk start");
    require_equal(chunks[1].source.end_line, 180, "Second chunk end");
    require(chunks[1].text.starts_with("line 81\nline 82"), "Second chunk should start after a 20-line overlap");
    require(chunks[1].text.ends_with("line 180"), "Second chunk should end at line 180");

    require_equal(chunks[2].id, 9, "Third chunk id");
    require_equal(chunks[2].source.start_line, 161, "Third chunk start");
    require_equal(chunks[2].source.end_line, 250, "Third chunk end");
    require(chunks[2].text.starts_with("line 161\nline 162"), "Third chunk should preserve the overlap");
    require(chunks[2].text.ends_with("line 250"), "Third chunk should include the final line");
}

void test_chunker_preserves_cpp_syntax_units()
{
    TempDir temp;
    const auto file_path = temp.path() / "src" / "sample.cpp";
    write_file(file_path,
        "namespace demo {\n"
        "class Box {\n"
        "public:\n"
        "    int get() const { return value_; }\n"
        "private:\n"
        "    int value_ = 42;\n"
        "};\n"
        "}\n"
        "int free_function(int value) { return value + 1; }\n");

    const SourceFile file{file_path, "src/sample.cpp", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{{100, 20, 600, 12000, true}}.chunk_file(file, 0);

    require(!chunks.empty(), "Syntax chunker should produce chunks");
    require(
        std::any_of(chunks.begin(), chunks.end(), [](const CodeChunk& chunk) {
            return chunk.metadata.mode == codewizard::ChunkMode::syntax &&
                chunk.metadata.kind == codewizard::ChunkKind::function &&
                chunk.metadata.qualified_name.find("demo::Box::get") != std::string::npos;
        }),
        "Syntax chunker should identify a qualified class method"
    );
    require(
        std::any_of(chunks.begin(), chunks.end(), [](const CodeChunk& chunk) {
            return chunk.metadata.kind == codewizard::ChunkKind::function &&
                chunk.metadata.qualified_name.find("free_function") != std::string::npos;
        }),
        "Syntax chunker should identify a free function"
    );
    require(
        std::all_of(chunks.begin(), chunks.end(), [](const CodeChunk& chunk) {
            return chunk.metadata.language == "cpp" && chunk.metadata.mode == codewizard::ChunkMode::syntax;
        }),
        "Valid C++ syntax chunks should carry syntax metadata"
    );
    require(
        std::none_of(chunks.begin(), chunks.end(), [](const CodeChunk& chunk) {
            return chunk.metadata.kind == codewizard::ChunkKind::type_declaration &&
                chunk.metadata.qualified_name.find("demo::Box") != std::string::npos &&
                (chunk.text.find('{') == std::string::npos || chunk.text.find('}') == std::string::npos);
        }),
        "Container wrappers should not become standalone chunks"
    );
}

void test_chunker_merges_tiny_split_symbol_tail()
{
    TempDir temp;
    const auto file_path = temp.path() / "large.cpp";
    const std::string contents =
        "int calculate() {\n"
        "    int value = 0;\n"
        "    value += 1111111111;\n"
        "    value += 2222222222;\n"
        "    value += 3333333333;\n"
        "    return value;\n"
        "}";
    write_file(file_path, contents);

    const SourceFile file{file_path, "large.cpp", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{{100, 20, 30, contents.size() - 1, true}}.chunk_file(file, 0);

    for (const auto& chunk : chunks) {
        if (!chunk.metadata.complete_symbol && chunk.text.size() < 30) {
            std::ostringstream message;
            message << "Tiny split-symbol chunk has " << chunk.text.size()
                << " bytes: \"" << chunk.text << '"';
            throw TestFailure(message.str());
        }
        require(chunk.text.size() <= contents.size() - 1, "Split chunks should respect the maximum size");
    }
}

void test_chunker_enforces_maximum_without_syntax_breakpoints()
{
    TempDir temp;
    const auto file_path = temp.path() / "literal.cpp";
    const std::string contents = "const char* text = \"" + std::string(200, 'x') + "\";\n";
    write_file(file_path, contents);

    const SourceFile file{file_path, "literal.cpp", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{{100, 20, 30, 80, true}}.chunk_file(file, 0);

    require(chunks.size() >= 3, "A node without internal syntax boundaries should be split");
    require(
        std::all_of(chunks.begin(), chunks.end(), [](const CodeChunk& chunk) {
            return !chunk.text.empty() && chunk.text.size() <= 80;
        }),
        "Every syntax chunk should respect the maximum size"
    );
}

void test_chunker_enforces_maximum_for_syntax_gaps()
{
    TempDir temp;
    const auto file_path = temp.path() / "gap.cpp";
    const std::string contents = "namespace demo {" + std::string(200, ' ') + "int value;\n}\n";
    write_file(file_path, contents);

    const SourceFile file{file_path, "gap.cpp", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{{100, 20, 30, 80, true}}.chunk_file(file, 0);

    require(
        std::all_of(chunks.begin(), chunks.end(), [](const CodeChunk& chunk) {
            return !chunk.text.empty() && chunk.text.size() <= 80;
        }),
        "Syntax gaps should respect the maximum chunk size"
    );
}

void test_chunker_falls_back_for_non_cpp_files()
{
    TempDir temp;
    const auto file_path = temp.path() / "notes.md";
    write_file(file_path, numbered_lines(130));

    const SourceFile file{file_path, "notes.md", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{{100, 20, 600, 12000, true}}.chunk_file(file, 0);

    require_equal(chunks.size(), 2, "Fallback chunk count");
    require(chunks[0].metadata.mode == codewizard::ChunkMode::fallback, "Fallback mode should be recorded");
    require(chunks[0].metadata.kind == codewizard::ChunkKind::fallback, "Fallback kind should be recorded");
    require_equal(chunks[1].source.start_line, 81, "Fallback overlap start");
}

void test_chunker_skips_whitespace_only_chunks()
{
    TempDir temp;
    const auto file_path = temp.path() / "whitespace.cpp";
    write_file(file_path, "   \n\n\t\n");

    const SourceFile file{file_path, "whitespace.cpp", fs::file_size(file_path)};
    const auto chunks = codewizard::Chunker{}.chunk_file(file, 0);

    require(chunks.empty(), "Whitespace-only files should not create embedding chunks");
}

void test_vector_store_saves_and_loads_index()
{
    TempDir temp;

    VectorStore store;
    store.set_metadata(IndexMetadata{
        "C:/Projects/Demo",
        "test-embedding-model",
        2,
        0
    });
    store.add_chunk(embedded_chunk(0, "src/main.cpp", {1.0, 0.0, 0.5}));
    store.add_chunk(embedded_chunk(1, "src/app.cpp", {0.0, 1.0, 0.5}));

    const auto index_path = temp.path() / ".index" / "index.json";
    store.save(index_path);

    const auto loaded = VectorStore::load(index_path);

    require_equal(loaded.size(), 2, "Loaded chunk count");
    require_equal(loaded.index().metadata.project_path, "C:/Projects/Demo", "Loaded project path");
    require_equal(loaded.index().metadata.embedding_model, "test-embedding-model", "Loaded embedding model");
    require_equal(loaded.index().metadata.file_count, 2, "Loaded file count");
    require_equal(loaded.index().metadata.chunk_count, 2, "Loaded metadata chunk count");

    const auto& chunks = loaded.chunks();
    require_equal(chunks[0].chunk.id, 0, "First loaded chunk id");
    require_equal(chunks[0].chunk.source.file, "src/main.cpp", "First loaded chunk file");
    require_equal(chunks[0].chunk.text, "chunk 0", "First loaded chunk text");
    require(chunks[0].embedding == Embedding({1.0, 0.0, 0.5}), "First loaded embedding should match");

    require_equal(chunks[1].chunk.id, 1, "Second loaded chunk id");
    require_equal(chunks[1].chunk.source.file, "src/app.cpp", "Second loaded chunk file");
    require(chunks[1].embedding == Embedding({0.0, 1.0, 0.5}), "Second loaded embedding should match");
}

void test_vector_store_cosine_similarity_and_top_k()
{
    VectorStore store;
    store.add_chunk(embedded_chunk(0, "perfect.cpp", {1.0, 0.0}));
    store.add_chunk(embedded_chunk(1, "orthogonal.cpp", {0.0, 1.0}));
    store.add_chunk(embedded_chunk(2, "close.cpp", {0.8, 0.6}));
    store.add_chunk(embedded_chunk(3, "diagonal.cpp", {1.0, 1.0}));
    store.add_chunk(embedded_chunk(4, "wrong-dimension.cpp", {1.0, 0.0, 0.0}));
    store.add_chunk(embedded_chunk(5, "empty.cpp", {}));

    require_near(VectorStore::cosine_similarity({1.0, 0.0}, {1.0, 0.0}), 1.0, 1e-12, "Identical cosine");
    require_near(VectorStore::cosine_similarity({1.0, 0.0}, {0.0, 1.0}), 0.0, 1e-12, "Orthogonal cosine");
    require_near(VectorStore::cosine_similarity({0.0, 0.0}, {1.0, 0.0}), 0.0, 1e-12, "Zero-vector cosine");
    require_near(VectorStore::cosine_similarity({1.0}, {1.0, 0.0}), 0.0, 1e-12, "Mismatched cosine");

    const auto results = store.search({1.0, 0.0}, 3);

    require_equal(results.size(), 3, "Top-k result count");
    require_equal(results[0].chunk.source.file, "perfect.cpp", "Best search result");
    require_equal(results[1].chunk.source.file, "close.cpp", "Second search result");
    require_equal(results[2].chunk.source.file, "diagonal.cpp", "Third search result");
    require_near(results[0].score, 1.0, 1e-12, "Best search score");
    require_near(results[1].score, 0.8, 1e-12, "Second search score");
    require_near(results[2].score, 1.0 / std::sqrt(2.0), 1e-12, "Third search score");

    require(store.search({1.0, 0.0}, 0).empty(), "top_k=0 should return no results");
    require(store.search({}, 3).empty(), "Empty query embedding should return no results");
}

} // namespace

int main()
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"FileScanner filters supported files", test_file_scanner_filters_supported_files},
        {"FileScanner honors project gitignore", test_file_scanner_honors_project_gitignore},
        {"Chunker uses expected overlap", test_chunker_uses_expected_overlap},
        {"Chunker preserves C++ syntax units", test_chunker_preserves_cpp_syntax_units},
        {"Chunker merges tiny split symbol tail", test_chunker_merges_tiny_split_symbol_tail},
        {"Chunker enforces maximum without syntax breakpoints", test_chunker_enforces_maximum_without_syntax_breakpoints},
        {"Chunker enforces maximum for syntax gaps", test_chunker_enforces_maximum_for_syntax_gaps},
        {"Chunker falls back for non-C++ files", test_chunker_falls_back_for_non_cpp_files},
        {"Chunker skips whitespace-only chunks", test_chunker_skips_whitespace_only_chunks},
        {"VectorStore saves and loads index", test_vector_store_saves_and_loads_index},
        {"VectorStore cosine similarity and top-k", test_vector_store_cosine_similarity_and_top_k}
    };

    int failures = 0;

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    if (failures > 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << tests.size() << " test(s) passed\n";
    return 0;
}
