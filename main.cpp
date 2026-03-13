/**
 * @file main.cpp
 * @brief The renderer and UI client. Knows nothing about simulation math.
 */
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "SimConfig.h"
#include "World.h"
#include <cmath>

float camX = 100.0f, camY = 70.0f, camZoom = 100.0f, aspect = 1.0f;

void renderWorld(World& world) {
    glLineWidth(3.0f);
    
    for (auto& org : world.population) {
        if (org->points.empty()) continue;

        glBegin(GL_LINES); 
        for (size_t i = 0; i < org->springs.size(); ++i) {
            const auto& spring = org->springs[i];
            const auto& p1 = org->points[spring.p1_idx];
            const auto& p2 = org->points[spring.p2_idx];

            if (!org->isAlive) {
                glColor3f(0.3f, 0.3f, 0.3f);
            } else {
                switch(org->bodyParts[i].type) {
                    case ColorType::GREEN:  glColor3f(0.1f, 0.8f, 0.1f); break;
                    case ColorType::RED:    glColor3f(0.9f, 0.1f, 0.1f); break;
                    case ColorType::PURPLE: glColor3f(0.8f, 0.1f, 0.9f); break;
                    case ColorType::BLUE:   glColor3f(0.1f, 0.3f, 0.9f); break;
                    case ColorType::YELLOW: glColor3f(0.9f, 0.8f, 0.1f); break;
                    case ColorType::WHITE:  glColor3f(0.9f, 0.9f, 0.9f); break;
                    case ColorType::DEAD:   glColor3f(0.3f, 0.3f, 0.3f); break;
                }
            }
            
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            
            // If the distance is physically impossible without wrapping, skip drawing the line
            if (std::abs(dx) > world.getConfig().worldWidth * 0.5f || 
                std::abs(dy) > world.getConfig().worldHeight * 0.5f) {
                continue; 
            }
            
            glVertex2f(p1.x, p1.y);
            glVertex2f(p2.x, p2.y);
        }
        glEnd();
    }
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1400, 900, "SNN Evolution Sandbox", NULL, NULL);
    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    SimConfig config;
    World world(config);
    world.initializeEden();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int w, h; 
        glfwGetFramebufferSize(window, &w, &h);
        aspect = (float)w / (float)h;
        glViewport(0, 0, w, h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Simulation Control");
        SimConfig& cfg = world.getConfig();
        ImGui::SliderFloat("Time Scale", &cfg.timeScale, 0.0f, 5.0f);
        ImGui::SliderFloat("Mutation Rate", &cfg.globalMutationRate, 0.0f, 1.0f);
        ImGui::SliderFloat("Base Metabolism", &cfg.baseMetabolism, 0.0f, 0.5f);
        if (ImGui::Button("Reset Eden")) world.initializeEden();
        ImGui::Text("Population: %zu", world.population.size());
        ImGui::End();

        ImGui::Begin("Brain Monitor (Org 0)");
        if (!world.population.empty()) {
            auto& org = world.population[0];
            ImGui::Text("Energy: %.1f", org->energy);
            ImGui::Separator();
            for (size_t i = 0; i < org->brain.neurons.size(); i++) {
                auto& n = org->brain.neurons[i];
                ImGui::Text("N%d (%s): %.2f / %.2f %s", 
                    n.id, 
                    n.role == NeuronRole::SENSORY ? "Sensory" : (n.role == NeuronRole::MOTOR ? "Motor" : "Hidden"),
                    n.membranePotential, 
                    n.threshold,
                    n.spikedThisTick ? " *SPIKE*" : "");
            }
        } else {
            ImGui::Text("Extinction.");
        }
        ImGui::End();

        // Camera Controls
        if (ImGui::IsKeyDown(ImGuiKey_W)) camY += camZoom * 0.03f;
        if (ImGui::IsKeyDown(ImGuiKey_S)) camY -= camZoom * 0.03f;
        if (ImGui::IsKeyDown(ImGuiKey_A)) camX -= camZoom * 0.03f;
        if (ImGui::IsKeyDown(ImGuiKey_D)) camX += camZoom * 0.03f;
        if (ImGui::IsKeyDown(ImGuiKey_E)) camZoom *= 1.02f;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) camZoom *= 0.98f;

        world.updateTick();

        glClearColor(0.05f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glMatrixMode(GL_PROJECTION); 
        glLoadIdentity();
        glOrtho(camX - camZoom * aspect, camX + camZoom * aspect, camY - camZoom, camY + camZoom, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);

        renderWorld(world);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    return 0;
}