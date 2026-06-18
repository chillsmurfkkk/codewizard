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

add_executable(codewizard
    src/core/app_config.cpp
    src/core/chunker.cpp
    src/core/embeddings_client.cpp
    src/core/file_scanner.cpp
    src/core/http_client.cpp
    src/core/llm_client.cpp
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

if(MSVC)
    target_compile_options(codewizard PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(codewizard PRIVATE -Wall -Wextra -Wpedantic)
endif()
