cmake_minimum_required(VERSION 3.24)

project(codewizard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(cpr CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

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
)

target_link_libraries(codewizard
    PRIVATE
        cpr::cpr
        glfw
        imgui::imgui
        nlohmann_json::nlohmann_json
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
)

target_link_libraries(codewizard_tests
    PRIVATE
        nlohmann_json::nlohmann_json
)

add_test(NAME codewizard_core_tests COMMAND codewizard_tests)

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
    target_link_options(codewizard PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
else()
    target_compile_options(codewizard PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(codewizard_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
