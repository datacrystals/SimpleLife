#pragma once
#include <vector>
#include <memory>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "UIWindow.h"
#include "World.h"

class SimulationUI {
private:
    std::vector<std::unique_ptr<UIWindow>> windows;

public:
    void Init(GLFWwindow* window);
    void Shutdown();
    void AddWindow(std::unique_ptr<UIWindow> window);
    void Render(World& world);
};