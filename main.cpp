#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "World.h"
#include "UI/SimulationUI.h"
#include "UI/SettingsWindow.h"
#include "UI/ViewportWindow.h"
#include "UI/BrainMonitorWindow.h" 

int main() {

    // 1. Initialize GLFW
    if (!glfwInit()) return -1;

    // 2. Create the window
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "SNN Evolution Sandbox", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    // 3. MAKE CONTEXT CURRENT (Crucial: GLEW needs this to 'see' OpenGL)
    glfwMakeContextCurrent(window);

    // 4. Initialize GLEW
    glewExperimental = GL_TRUE; // Helps with modern features/FBOs
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return -1;
    }

    // 5. Now it is safe to initialize your UI and call glGenFramebuffers
    SimulationUI ui;
    ui.Init(window);



    SimConfig config;
    World world(config);
    world.initializeEden();

    
    // Register your windows here
    ui.AddWindow(std::make_unique<SettingsWindow>());
    ui.AddWindow(std::make_unique<ViewportWindow>());
    ui.AddWindow(std::make_unique<BrainMonitorWindow>());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        world.updateTick();

        // The OS window clear
        int w, h; 
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render UI over the top
        ui.Render(world);

        glfwSwapBuffers(window);
    }

    ui.Shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}