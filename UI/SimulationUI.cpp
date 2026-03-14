#include "SimulationUI.h"

void SimulationUI::Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // --- SCALE UI GLOBALLY ---
    // 1.0 is default. 1.25 is 25% larger, 2.0 is double size.
    io.FontGlobalScale = 1.25f; 

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}
void SimulationUI::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void SimulationUI::AddWindow(std::unique_ptr<UIWindow> window) {
    windows.push_back(std::move(window));
}

void SimulationUI::Render(World& world) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Create the full-screen dockspace
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // Loop through and draw every registered window
    for (auto& window : windows) {
        if (window->isOpen) {
            window->Draw(world);
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}