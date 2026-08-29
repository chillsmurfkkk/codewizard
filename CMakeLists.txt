cmake_minimum_required(VERSION 3.24)

include(FetchContent)

project(codewizard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(cpr CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(unofficial-tree-sitter CONFIG REQUIRED)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
find_program(TREE_SITTER_CLI NAMES tree-sitter REQUIRED)
FetchContent_Declare(
    tree_sitter_cpp
    URL https://github.com/tree-sitter/tree-sitter-cpp/archive/refs/tags/v0.23.4.tar.gz
    URL_HASH SHA512=baebacf06ea1527132c641b4e2a2e997c501a63708d7afdb5d9456de519dbd652f25aee03a7b4112ef9a683fa176aaaf96d272de286223773a5d6cdf01605a2e
)
FetchContent_MakeAvailable(tree_sitter_cpp)

enable_testing()

add_executable(codewizard
    src/app/window_frame.cpp
    src/core/app_config.cpp
    src/core/chunker.cpp
    src/core/embeddings_client.cpp
    src/core/file_scanner.cpp
    src/core/http_client.cpp
    src/core/indexer.cpp
    src/core/llm_client.cpp
    src/core/prompt_builder.cpp
    src/core/rag_pipeline.cpp
    src/core/retriever.cpp
    src/core/vector_store.cpp
    src/main.cpp
)

target_include_directories(codewizard
    PRIVATE
        src
        ${tree_sitter_cpp_SOURCE_DIR}/bindings/c
)

target_link_libraries(codewizard
    PRIVATE
        cpr::cpr
        glfw
        imgui::imgui
        nlohmann_json::nlohmann_json
        unofficial::tree-sitter::tree-sitter
        tree-sitter-cpp
        OpenGL::GL
)

add_executable(codewizard_tests
    tests/core_tests.cpp
    src/core/chunker.cpp
    src/core/file_scanner.cpp
    src/core/vector_store.cpp
)

target_include_directories(codewizard_tests
    PRIVATE
        src
        ${tree_sitter_cpp_SOURCE_DIR}/bindings/c
)

target_link_libraries(codewizard_tests
    PRIVATE
        nlohmann_json::nlohmann_json
        unofficial::tree-sitter::tree-sitter
        tree-sitter-cpp
)

add_test(NAME codewizard_core_tests COMMAND codewizard_tests)

add_executable(codewizard_http_tests
    tests/http_client_tests.cpp
    src/core/http_client.cpp
)

target_include_directories(codewizard_http_tests PRIVATE src)
target_link_libraries(codewizard_http_tests PRIVATE cpr::cpr nlohmann_json::nlohmann_json)

add_test(NAME codewizard_http_tests COMMAND codewizard_http_tests)

if(WIN32)
    target_link_libraries(codewizard
        PRIVATE
            ole32
            uuid
    )
endif()

if(MSVC)
    target_compile_options(codewizard PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(codewizard_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(codewizard_http_tests PRIVATE /W4 /permissive- /utf-8)
    target_link_options(codewizard PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
else()
    target_compile_options(codewizard PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(codewizard_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(codewizard_http_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
