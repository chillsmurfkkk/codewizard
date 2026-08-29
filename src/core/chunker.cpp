#include "core/chunker.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <tree_sitter/api.h>
#include <tree-sitter-cpp.h>

namespace codewizard {
namespace {

struct SourceText {
    std::string text;
    std::vector<std::size_t> line_starts{0};
};

struct ParsedFile {
    TSTree* tree = nullptr;
    TSParser* parser = nullptr;

    ParsedFile() = default;

    ParsedFile(ParsedFile&& other) noexcept
        : tree(other.tree), parser(other.parser)
    {
        other.tree = nullptr;
        other.parser = nullptr;
    }

    ParsedFile& operator=(ParsedFile&& other) noexcept
    {
        if (this != &other) {
            if (tree != nullptr) ts_tree_delete(tree);
            if (parser != nullptr) ts_parser_delete(parser);
            tree = other.tree;
            parser = other.parser;
            other.tree = nullptr;
            other.parser = nullptr;
        }
        return *this;
    }

    ~ParsedFile()
    {
        if (tree != nullptr) {
            ts_tree_delete(tree);
        }
        if (parser != nullptr) {
            ts_parser_delete(parser);
        }
    }

    ParsedFile(const ParsedFile&) = delete;
    ParsedFile& operator=(const ParsedFile&) = delete;
};

struct SyntaxContext {
    std::string qualified_name;
};

struct PendingChunk {
    std::size_t start_byte = 0;
    std::size_t end_byte = 0;
    ChunkKind kind = ChunkKind::declaration;
    std::string qualified_name;
    bool complete_symbol = true;
};

SourceText read_source(const SourceFile& file)
{
    std::ifstream input(file.absolute_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open source file: " + file.absolute_path.string());
    }

    SourceText source{
        std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}),
        {0}
    };

    for (std::size_t index = 0; index < source.text.size(); ++index) {
        if (source.text[index] == '\n') {
            source.line_starts.push_back(index + 1);
        }
    }

    return source;
}

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

std::string join_lines(
    const std::vector<std::string>& lines,
    std::size_t start_index,
    std::size_t end_index
)
{
    std::ostringstream output;
    for (std::size_t index = start_index; index < end_index; ++index) {
        output << lines[index];
        if (index + 1 < end_index) {
            output << '\n';
        }
    }
    return output.str();
}

bool contains_non_whitespace(const std::string& text)
{
    return std::any_of(text.begin(), text.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    });
}

std::size_t utf8_boundary_before(const std::string& text, std::size_t byte, std::size_t floor)
{
    while (byte > floor && byte < text.size() &&
        (static_cast<unsigned char>(text[byte]) & 0xC0) == 0x80) {
        --byte;
    }
    return byte == floor ? std::min(floor + 1, text.size()) : byte;
}

void append_bounded_range(
    std::vector<PendingChunk>& output,
    const std::string& source,
    std::size_t start,
    std::size_t end,
    ChunkKind kind,
    std::string qualified_name,
    bool complete_symbol,
    std::size_t min_bytes,
    std::size_t max_bytes
)
{
    const bool requires_split = end - start > max_bytes;
    while (end - start > max_bytes) {
        auto next = start + max_bytes;
        if (end - next < min_bytes) {
            next = end - min_bytes;
        }
        next = utf8_boundary_before(source, next, start);
        output.push_back(PendingChunk{start, next, kind, qualified_name, false});
        start = next;
    }
    if (end > start) {
        output.push_back(PendingChunk{
            start,
            end,
            kind,
            std::move(qualified_name),
            complete_symbol && !requires_split
        });
    }
}

std::size_t line_at_byte(const std::vector<std::size_t>& line_starts, std::size_t byte)
{
    const auto iterator = std::upper_bound(line_starts.begin(), line_starts.end(), byte);
    return static_cast<std::size_t>(std::distance(line_starts.begin(), iterator));
}

std::string node_text(const TSNode node, const std::string& source)
{
    const std::size_t start = ts_node_start_byte(node);
    const std::size_t end = ts_node_end_byte(node);
    if (start >= end || start >= source.size()) {
        return {};
    }
    return source.substr(start, std::min(end, source.size()) - start);
}

bool is_identifier_type(const char* type)
{
    return std::string_view(type) == "identifier" ||
        std::string_view(type) == "field_identifier" ||
        std::string_view(type) == "type_identifier" ||
        std::string_view(type) == "namespace_identifier";
}

std::string first_identifier(const TSNode node, const std::string& source)
{
    if (is_identifier_type(ts_node_type(node))) {
        return node_text(node, source);
    }

    const auto child_count = ts_node_named_child_count(node);
    for (uint32_t index = 0; index < child_count; ++index) {
        const auto name = first_identifier(ts_node_named_child(node, index), source);
        if (!name.empty()) {
            return name;
        }
    }

    return {};
}

ChunkKind node_kind(const TSNode node)
{
    const std::string_view type = ts_node_type(node);
    if (type == "namespace_definition") return ChunkKind::namespace_scope;
    if (type == "class_specifier" || type == "struct_specifier" || type == "union_specifier" ||
        type == "enum_specifier") return ChunkKind::type_declaration;
    if (type == "function_definition") return ChunkKind::function;
    return ChunkKind::declaration;
}

std::string append_name(const std::string& parent, const std::string& name)
{
    if (name.empty()) return parent;
    if (parent.empty()) return name;
    return parent + "::" + name;
}

std::string node_name(const TSNode node, const std::string& source)
{
    const auto name_node = ts_node_child_by_field_name(node, "name", 4);
    if (!ts_node_is_null(name_node)) {
        const auto name = first_identifier(name_node, source);
        if (!name.empty()) return name;
        return node_text(name_node, source);
    }

    if (std::string_view(ts_node_type(node)) == "function_definition") {
        const auto declarator = ts_node_child_by_field_name(node, "declarator", 10);
        if (!ts_node_is_null(declarator)) {
            return first_identifier(declarator, source);
        }
    }

    return first_identifier(node, source);
}

bool is_container(const TSNode node)
{
    const std::string_view type = ts_node_type(node);
    return type == "namespace_definition" || type == "class_specifier" ||
        type == "struct_specifier" || type == "union_specifier";
}

TSNode node_body(const TSNode node)
{
    return ts_node_child_by_field_name(node, "body", 4);
}

void collect_syntax_breakpoints(const TSNode node, std::vector<std::size_t>& breakpoints)
{
    const auto type = std::string_view(ts_node_type(node));
    if (type.ends_with("_statement") || type == "declaration" || type == "field_declaration") {
        breakpoints.push_back(static_cast<std::size_t>(ts_node_end_byte(node)));
    }

    const auto child_count = ts_node_named_child_count(node);
    for (uint32_t index = 0; index < child_count; ++index) {
        collect_syntax_breakpoints(ts_node_named_child(node, index), breakpoints);
    }
}

void split_large_node(
    const TSNode node,
    const std::string& source,
    const SyntaxContext& context,
    std::size_t min_bytes,
    std::size_t max_bytes,
    std::vector<PendingChunk>& output
)
{
    const std::size_t start = ts_node_start_byte(node);
    const std::size_t end = ts_node_end_byte(node);
    if (end <= start) return;

    std::vector<std::size_t> breakpoints;
    collect_syntax_breakpoints(node, breakpoints);
    breakpoints.push_back(end);
    std::sort(breakpoints.begin(), breakpoints.end());
    breakpoints.erase(std::unique(breakpoints.begin(), breakpoints.end()), breakpoints.end());

    std::size_t cursor = start;
    while (cursor < end) {
        if (end - cursor <= max_bytes) {
            output.push_back(PendingChunk{
                cursor,
                end,
                node_kind(node),
                append_name(context.qualified_name, node_name(node, source)),
                false
            });
            break;
        }

        const auto limit = std::min(cursor + max_bytes, end);
        auto boundary = std::upper_bound(breakpoints.begin(), breakpoints.end(), limit);
        if (boundary != breakpoints.begin() && *(boundary - 1) > cursor) {
            --boundary;
        } else {
            boundary = breakpoints.end();
        }

        auto next = boundary == breakpoints.end()
            ? utf8_boundary_before(source, limit, cursor)
            : *boundary;
        if (next < end && end - next < min_bytes) {
            const auto latest = end - min_bytes;
            auto earlier = std::upper_bound(breakpoints.begin(), breakpoints.end(), latest);
            if (earlier != breakpoints.begin() && *(earlier - 1) > cursor) {
                next = *(earlier - 1);
            } else {
                next = utf8_boundary_before(source, latest, cursor);
            }
        }
        if (next <= cursor) break;

        output.push_back(PendingChunk{
            cursor,
            next,
            node_kind(node),
            append_name(context.qualified_name, node_name(node, source)),
            false
        });
        cursor = next;
    }
}

void collect_node(
    const TSNode node,
    const std::string& source,
    const SyntaxContext& context,
    std::size_t min_bytes,
    std::size_t max_bytes,
    std::vector<PendingChunk>& output
)
{
    const std::size_t start = ts_node_start_byte(node);
    const std::size_t end = ts_node_end_byte(node);
    if (end <= start) return;

    const auto kind = node_kind(node);
    const auto name = append_name(context.qualified_name, node_name(node, source));

    if (is_container(node)) {
        const auto body = node_body(node);
        if (!ts_node_is_null(body) && ts_node_named_child_count(body) > 0) {
            const std::size_t body_start = ts_node_start_byte(body);
            const std::size_t body_end = ts_node_end_byte(body);
            if (body_start > start) {
                output.push_back(PendingChunk{start, body_start, ChunkKind::declaration, name, false});
            }

            SyntaxContext child_context{name};
            const auto child_count = ts_node_named_child_count(body);
            for (uint32_t index = 0; index < child_count; ++index) {
                collect_node(ts_node_named_child(body, index), source, child_context, min_bytes, max_bytes, output);
            }

            if (body_end < end) {
                output.push_back(PendingChunk{body_end, end, ChunkKind::declaration, name, false});
            }
            return;
        }
    }

    if (end - start <= max_bytes) {
        output.push_back(PendingChunk{start, end, kind, name, true});
    } else {
        split_large_node(node, source, context, min_bytes, max_bytes, output);
    }
}

ParsedFile parse_cpp(const std::string& source)
{
    ParsedFile parsed;
    parsed.parser = ts_parser_new();
    if (parsed.parser == nullptr || !ts_parser_set_language(parsed.parser, tree_sitter_cpp())) {
        return parsed;
    }
    parsed.tree = ts_parser_parse_string(
        parsed.parser,
        nullptr,
        source.data(),
        static_cast<uint32_t>(source.size())
    );
    return parsed;
}

bool is_cpp_file(const SourceFile& file)
{
    std::string extension = file.absolute_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension == ".c" || extension == ".cc" || extension == ".cpp" ||
        extension == ".cppm" || extension == ".cxx" || extension == ".h" ||
        extension == ".hh" || extension == ".hpp" || extension == ".hxx" ||
        extension == ".ixx";
}

std::string language_for_file(const SourceFile& file)
{
    std::string extension = file.absolute_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension == ".c" ? "c" : "cpp";
}

std::vector<CodeChunk> fallback_chunks(
    const SourceFile& file,
    const SourceText& source,
    std::size_t first_chunk_id,
    const ChunkerOptions& options
)
{
    const auto lines = split_lines(source.text);
    if (lines.empty()) return {};

    std::vector<CodeChunk> chunks;
    const std::size_t step = options.max_lines_per_chunk - options.overlap_lines;
    for (std::size_t start = 0; start < lines.size(); start += step) {
        const auto end = std::min(start + options.max_lines_per_chunk, lines.size());
        const auto text = join_lines(lines, start, end);
        if (!contains_non_whitespace(text)) {
            if (end == lines.size()) break;
            continue;
        }

        chunks.push_back(CodeChunk{
            first_chunk_id + chunks.size(),
            SourceRange{file.relative_path, start + 1, end},
            text,
            ChunkMetadata{"unknown", ChunkKind::fallback, ChunkMode::fallback, "", std::nullopt, 0, 1, true}
        });
        if (end == lines.size()) break;
    }
    return chunks;
}

std::vector<CodeChunk> syntax_chunks(
    const SourceFile& file,
    const SourceText& source,
    std::size_t first_chunk_id,
    const ChunkerOptions& options
)
{
    const auto parsed = parse_cpp(source.text);
    if (parsed.tree == nullptr) return {};

    const auto root = ts_tree_root_node(parsed.tree);
    if (ts_node_child_count(root) == 0 || ts_node_has_error(root)) return {};

    std::vector<PendingChunk> pending;
    const auto child_count = ts_node_named_child_count(root);
    for (uint32_t index = 0; index < child_count; ++index) {
        collect_node(
            ts_node_named_child(root, index),
            source.text,
            {},
            options.min_bytes_per_syntax_chunk,
            options.max_bytes_per_syntax_chunk,
            pending
        );
    }

    std::sort(pending.begin(), pending.end(), [](const PendingChunk& left, const PendingChunk& right) {
        if (left.start_byte != right.start_byte) return left.start_byte < right.start_byte;
        return left.end_byte < right.end_byte;
    });

    std::vector<PendingChunk> covered;
    std::size_t cursor = 0;
    for (const auto& item : pending) {
        if (item.start_byte > cursor) {
            append_bounded_range(
                covered,
                source.text,
                cursor,
                item.start_byte,
                ChunkKind::file_header,
                {},
                true,
                options.min_bytes_per_syntax_chunk,
                options.max_bytes_per_syntax_chunk
            );
        }
        if (item.end_byte > cursor) {
            covered.push_back(item);
            cursor = item.end_byte;
        }
    }
    if (cursor < source.text.size()) {
        append_bounded_range(
            covered,
            source.text,
            cursor,
            source.text.size(),
            ChunkKind::file_header,
            {},
            true,
            options.min_bytes_per_syntax_chunk,
            options.max_bytes_per_syntax_chunk
        );
    }

    // Headers, access specifiers, closing braces, and tiny declarations are
    // useful as context, but poor standalone embedding units. Attach them to
    // a neighboring semantic unit while staying below the hard size limit.
    for (std::size_t index = 0; index < covered.size();) {
        const auto size = covered[index].end_byte - covered[index].start_byte;
        const bool is_atomic_symbol = covered[index].complete_symbol &&
            (covered[index].kind == ChunkKind::function ||
                covered[index].kind == ChunkKind::type_declaration);
        if (size >= options.min_bytes_per_syntax_chunk || is_atomic_symbol || covered.size() == 1) {
            ++index;
            continue;
        }

        if (index + 1 < covered.size() &&
            covered[index + 1].end_byte - covered[index].start_byte <= options.max_bytes_per_syntax_chunk) {
            covered[index + 1].start_byte = covered[index].start_byte;
            covered.erase(covered.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        if (index > 0 &&
            covered[index].end_byte - covered[index - 1].start_byte <= options.max_bytes_per_syntax_chunk) {
            covered[index - 1].end_byte = covered[index].end_byte;
            covered.erase(covered.begin() + static_cast<std::ptrdiff_t>(index));
            --index;
            continue;
        }

        ++index;
    }

    std::vector<CodeChunk> chunks;
    for (const auto& item : covered) {
        if (item.end_byte <= item.start_byte) continue;
        const auto text = source.text.substr(item.start_byte, item.end_byte - item.start_byte);
        if (!contains_non_whitespace(text)) continue;

        const auto start_line = line_at_byte(source.line_starts, item.start_byte);
        const auto end_line = line_at_byte(source.line_starts, item.end_byte - 1);
        chunks.push_back(CodeChunk{
            first_chunk_id + chunks.size(),
            SourceRange{file.relative_path, start_line, end_line},
            text,
            ChunkMetadata{language_for_file(file), item.kind, ChunkMode::syntax, item.qualified_name, std::nullopt, 0, 1, item.complete_symbol}
        });
    }
    return chunks;
}

} // namespace

Chunker::Chunker(ChunkerOptions options)
    : options_(options)
{
    if (options_.max_lines_per_chunk == 0) {
        throw std::runtime_error("Chunk size must be greater than zero");
    }
    if (options_.overlap_lines >= options_.max_lines_per_chunk) {
        throw std::runtime_error("Chunk overlap must be smaller than chunk size");
    }
    if (options_.max_bytes_per_syntax_chunk == 0) {
        throw std::runtime_error("Syntax chunk size must be greater than zero");
    }
    if (options_.min_bytes_per_syntax_chunk > options_.max_bytes_per_syntax_chunk) {
        throw std::runtime_error("Minimum syntax chunk size must not exceed maximum size");
    }
}

std::vector<CodeChunk> Chunker::chunk_files(const std::vector<SourceFile>& files) const
{
    std::vector<CodeChunk> chunks;
    for (const auto& file : files) {
        auto file_chunks = chunk_file(file, chunks.size());
        chunks.insert(
            chunks.end(),
            std::make_move_iterator(file_chunks.begin()),
            std::make_move_iterator(file_chunks.end())
        );
    }
    return chunks;
}

std::vector<CodeChunk> Chunker::chunk_file(const SourceFile& file, std::size_t first_chunk_id) const
{
    const auto source = read_source(file);
    if (source.text.empty()) return {};

    if (options_.enable_syntax_chunking && is_cpp_file(file)) {
        auto chunks = syntax_chunks(file, source, first_chunk_id, options_);
        if (!chunks.empty()) return chunks;
    }

    return fallback_chunks(file, source, first_chunk_id, options_);
}

} // namespace codewizard
