#include <cstdio>
#include <string>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "core/chunker.hpp"
#include "core/file_scanner.hpp"
#include "core/types.hpp"

namespace {

void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "codewizard", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    char project_path[1024] = "";
    char question[1024] = "";
    std::string status = "Ready to index a project.";
    std::string answer = "The RAG pipeline is not wired yet. This window proves the CMake, vcpkg, GLFW, OpenGL, and Dear ImGui foundation is ready.";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        ImGui::Begin("codewizard", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::TextUnformatted("Project path");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##project_path", project_path, IM_ARRAYSIZE(project_path));

        if (ImGui::Button("Index Project")) {
            try {
                const codewizard::FileScanner scanner;
                const auto files = scanner.scan(project_path);
                const codewizard::Chunker chunker;
                const auto chunks = chunker.chunk_files(files);
                status = "Found " + std::to_string(files.size()) +
                    " files, created " + std::to_string(chunks.size()) +
                    " chunks. Next step: VectorStore.";
            } catch (const std::exception& exception) {
                status = std::string{"Scan failed: "} + exception.what();
            }
        }

        ImGui::SameLine();
        ImGui::Text("Status: %s", status.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted("Question");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##question", question, IM_ARRAYSIZE(question));

        if (ImGui::Button("Ask")) {
            answer = "Answer generation is not implemented yet. Next step: connect VectorStore, ApiClient, and RAGPipeline.";
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Answer");
        ImGui::BeginChild("answer", ImVec2(0.0F, 180.0F), true);
        ImGui::TextWrapped("%s", answer.c_str());
        ImGui::EndChild();

        ImGui::TextUnformatted("Sources");
        ImGui::BeginChild("sources", ImVec2(0.0F, 100.0F), true);
        ImGui::TextDisabled("No sources yet.");
        ImGui::EndChild();

        ImGui::TextUnformatted("Retrieved chunks debug");
        ImGui::BeginChild("debug_chunks", ImVec2(0.0F, 0.0F), true);
        ImGui::TextDisabled("No retrieved chunks yet.");
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
