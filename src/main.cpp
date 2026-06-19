#include <cstdio>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <shobjidl.h>
#endif
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "app/window_frame.hpp"
#include "core/app_config.hpp"
#include "core/rag_pipeline.hpp"
#include "core/types.hpp"

namespace {

void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

void load_ui_font(ImGuiIO& io)
{
    ImFontConfig font_config;
    font_config.OversampleH = 3;
    font_config.OversampleV = 2;
    font_config.PixelSnapH = false;

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\segoeui.ttf",
        22.0F,
        &font_config,
        io.Fonts->GetGlyphRangesCyrillic()
    );

    if (font == nullptr) {
        font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\arial.ttf",
            22.0F,
            &font_config,
            io.Fonts->GetGlyphRangesCyrillic()
        );
    }

    if (font == nullptr) {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = 1.28F;
    }
}

void apply_ui_style(bool dark_mode)
{
    if (dark_mode) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F;
    style.FrameRounding = 5.0F;
    style.GrabRounding = 5.0F;
    style.ChildRounding = 6.0F;
    style.WindowBorderSize = 0.0F;
    style.WindowPadding = ImVec2(14.0F, 12.0F);
    style.FramePadding = ImVec2(10.0F, 7.0F);
    style.ItemSpacing = ImVec2(10.0F, 9.0F);
    style.ScrollbarSize = 18.0F;

    if (dark_mode) {
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.075F, 0.082F, 0.092F, 1.0F);
        colors[ImGuiCol_ChildBg] = ImVec4(0.105F, 0.115F, 0.128F, 1.0F);
        colors[ImGuiCol_FrameBg] = ImVec4(0.145F, 0.157F, 0.175F, 1.0F);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.185F, 0.205F, 0.230F, 1.0F);
        colors[ImGuiCol_Button] = ImVec4(0.180F, 0.205F, 0.240F, 1.0F);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.235F, 0.270F, 0.315F, 1.0F);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.285F, 0.330F, 0.390F, 1.0F);
        colors[ImGuiCol_Header] = ImVec4(0.180F, 0.205F, 0.240F, 1.0F);
        colors[ImGuiCol_Separator] = ImVec4(0.230F, 0.250F, 0.280F, 1.0F);
    }
}

std::string chunk_preview(const std::string& text)
{
    constexpr std::size_t max_preview_length = 240;

    std::string preview = text.substr(0, max_preview_length);
    for (char& character : preview) {
        if (character == '\n' || character == '\r' || character == '\t') {
            character = ' ';
        }
    }

    if (text.size() > max_preview_length) {
        preview += "...";
    }

    return preview;
}

#ifdef _WIN32

std::string wide_to_utf8(const wchar_t* value)
{
    if (value == nullptr) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }

    std::string output(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), size, nullptr, nullptr);
    return output;
}

std::optional<std::string> choose_folder(GLFWwindow* owner)
{
    const HRESULT init_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool should_uninitialize = SUCCEEDED(init_result);

    IFileOpenDialog* dialog = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)
    );

    if (FAILED(result)) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    }

    std::optional<std::string> selected_path;
    result = dialog->Show(glfwGetWin32Window(owner));
    if (SUCCEEDED(result)) {
        IShellItem* item = nullptr;
        result = dialog->GetResult(&item);
        if (SUCCEEDED(result)) {
            PWSTR path = nullptr;
            result = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (SUCCEEDED(result)) {
                selected_path = wide_to_utf8(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();

    if (should_uninitialize) {
        CoUninitialize();
    }

    return selected_path;
}

#else

std::optional<std::string> choose_folder(GLFWwindow*)
{
    return std::nullopt;
}

#endif

void copy_to_input_buffer(const std::string& value, char* buffer, std::size_t buffer_size)
{
    if (buffer_size == 0) {
        return;
    }

    std::snprintf(buffer, buffer_size, "%s", value.c_str());
    buffer[buffer_size - 1] = '\0';
}

} // namespace

int main()
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "codewizard", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwSetWindowSizeLimits(
        window,
        codewizard::app::WindowFrame::min_width,
        codewizard::app::WindowFrame::min_height,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE
    );

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    bool dark_mode = true;
    load_ui_font(io);
    apply_ui_style(dark_mode);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    char project_path[1024] = "";
    char question[2048] = "";
    std::string status = "Ready to index a project.";
    std::string answer = "Index a project, then ask a question about that codebase.";
    std::vector<codewizard::SearchResult> sources;
    std::string prompt_context;

    {
        codewizard::app::WindowFrame window_frame{window};

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            window_frame.update();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F), ImGuiCond_Always);
            ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

            ImGui::Begin(
                "codewizard",
                nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove
            );

            if (window_frame.draw_title_bar(dark_mode)) {
                dark_mode = !dark_mode;
                apply_ui_style(dark_mode);
            }

            ImGui::TextUnformatted("Project path");
            ImGui::SetNextItemWidth(-118.0F);
            ImGui::InputText("##project_path", project_path, IM_ARRAYSIZE(project_path));
            ImGui::SameLine();
            if (ImGui::Button("Browse", ImVec2(100.0F, 0.0F))) {
                if (const auto selected_path = choose_folder(window)) {
                    copy_to_input_buffer(*selected_path, project_path, IM_ARRAYSIZE(project_path));
                }
            }

            if (ImGui::Button("Index Project")) {
                try {
                    answer = "Indexing project...";
                    sources.clear();
                    prompt_context.clear();

                    const auto config = codewizard::load_app_config();
                    const codewizard::RAGPipeline pipeline{config};
                    const auto result = pipeline.index_project(
                        project_path,
                        [&status](const codewizard::IndexingProgress& progress) {
                            status = progress.message;
                        }
                    );

                    status = "Indexed " + std::to_string(result.chunk_count) +
                        " chunks from " + std::to_string(result.file_count) +
                        " files. Saved " + result.index_path.string();
                    answer = "Project index is ready. Ask a question about the codebase.";
                } catch (const std::exception& exception) {
                    status = std::string{"Indexing failed: "} + exception.what();
                    answer = "Indexing failed. Check the status message.";
                }
            }

            ImGui::SameLine();
            ImGui::TextWrapped("Status: %s", status.c_str());

            ImGui::Separator();
            ImGui::TextUnformatted("Question");
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::InputText("##question", question, IM_ARRAYSIZE(question));

            if (ImGui::Button("Ask")) {
                try {
                    answer = "Thinking...";
                    sources.clear();
                    prompt_context.clear();
                    status = "Retrieving relevant chunks...";

                    const auto config = codewizard::load_app_config();
                    const codewizard::RAGPipeline pipeline{config};
                    auto result = pipeline.answer_question(project_path, question);

                    answer = std::move(result.answer);
                    sources = std::move(result.sources);
                    prompt_context = std::move(result.context);
                    status = "Answered with " + std::to_string(sources.size()) + " retrieved chunks.";
                } catch (const std::exception& exception) {
                    status = std::string{"Answer failed: "} + exception.what();
                    answer = "Answer generation failed. Check the status message.";
                }
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Answer");
            ImGui::BeginChild("answer", ImVec2(0.0F, 190.0F), true);
            ImGui::TextWrapped("%s", answer.c_str());
            ImGui::EndChild();

            ImGui::TextUnformatted("Sources");
            ImGui::BeginChild("sources", ImVec2(0.0F, 100.0F), true);
            if (sources.empty()) {
                ImGui::TextDisabled("No sources yet.");
            } else {
                for (std::size_t index = 0; index < sources.size(); ++index) {
                    const auto& source = sources[index].chunk.source;
                    ImGui::Text(
                        "%zu. %s, lines %zu-%zu, score=%.3f",
                        index + 1,
                        source.file.c_str(),
                        source.start_line,
                        source.end_line,
                        sources[index].score
                    );
                }
            }
            ImGui::EndChild();

            ImGui::TextUnformatted("Retrieved chunks debug");
            const float debug_height = std::max(180.0F, ImGui::GetContentRegionAvail().y - 44.0F);
            ImGui::BeginChild("debug_chunks", ImVec2(0.0F, debug_height), true);
            if (sources.empty()) {
                ImGui::TextDisabled("No retrieved chunks yet.");
            } else {
                for (std::size_t index = 0; index < sources.size(); ++index) {
                    const auto& result = sources[index];
                    const auto& source = result.chunk.source;
                    ImGui::Text(
                        "%zu. %s:%zu-%zu score=%.3f",
                        index + 1,
                        source.file.c_str(),
                        source.start_line,
                        source.end_line,
                        result.score
                    );
                    ImGui::TextWrapped("%s", chunk_preview(result.chunk.text).c_str());
                    ImGui::Separator();
                }

                if (!prompt_context.empty() && ImGui::CollapsingHeader("Prompt context")) {
                    ImGui::TextWrapped("%s", prompt_context.c_str());
                }
            }
            ImGui::EndChild();

            ImGui::End();

            ImGui::Render();

            int display_w = 0;
            int display_h = 0;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.08F, 0.09F, 0.10F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
