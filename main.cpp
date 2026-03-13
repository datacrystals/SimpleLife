#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "World.h"
#include <vector>

std::mt19937 rng(42);
std::uniform_real_distribution<float> randFloat(0.0f, 1.0f);

float camX = 0.0f, camY = 0.0f, camZoom = 100.0f, aspect = 1.0f;

std::vector<float> popHistory;
std::vector<float> plantHistory;
std::vector<float> herbivoreHistory;
std::vector<float> carnivoreHistory;

// main.cpp (Rendering Snippet)
void renderWorld(const World& world) {
    glLineWidth(3.0f);
    
    for (const auto* org : world.population) {
        if (org->points.empty()) continue;

        glBegin(GL_LINES); 
        for (const auto& stick : org->sticks) {
            const Point& p1 = org->points[stick.p1_idx];
            const Point& p2 = org->points[stick.p2_idx];

            if (org->damageFlash > 0.0f) glColor3f(1.0f, 1.0f, 1.0f);
            else {
                switch(stick.type) {
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

            // Check if the stick is wrapping
            if (std::abs(dx) > WORLD_WIDTH * 0.5f || std::abs(dy) > WORLD_HEIGHT * 0.5f) {
                // Draw two lines: one from p1 to the "ghost" p2, and p2 to the "ghost" p1
                float g2x = p2.x, g2y = p2.y;
                if (dx > WORLD_WIDTH * 0.5f) g2x -= WORLD_WIDTH;
                else if (dx < -WORLD_WIDTH * 0.5f) g2x += WORLD_WIDTH;
                if (dy > WORLD_HEIGHT * 0.5f) g2y -= WORLD_HEIGHT;
                else if (dy < -WORLD_HEIGHT * 0.5f) g2y += WORLD_HEIGHT;

                // Line 1: From p1 towards the edge
                glVertex2f(p1.x, p1.y);
                glVertex2f(g2x, g2y);

                // Line 2: From p2 towards the other edge
                float g1x = p1.x, g1y = p1.y;
                if (dx > WORLD_WIDTH * 0.5f) g1x += WORLD_WIDTH;
                else if (dx < -WORLD_WIDTH * 0.5f) g1x -= WORLD_WIDTH;
                if (dy > WORLD_HEIGHT * 0.5f) g1y += WORLD_HEIGHT;
                else if (dy < -WORLD_HEIGHT * 0.5f) g1y -= WORLD_HEIGHT;

                glVertex2f(g1x, g1y);
                glVertex2f(p2.x, p2.y);
            } else {
                // Normal case: no wrapping
                glVertex2f(p1.x, p1.y);
                glVertex2f(p2.x, p2.y);
            }
        }
        glEnd();
    }
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1400, 900, "Multithreaded Jolt ALife", NULL, NULL);
    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    World world;
    
    popHistory.resize(100, 0); plantHistory.resize(100, 0);
    herbivoreHistory.resize(100, 0); carnivoreHistory.resize(100, 0);
    int frameCounter = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int w, h; 
        glfwGetFramebufferSize(window, &w, &h);
        aspect = (float)w / (float)h;
        glViewport(0, 0, w, h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int plants = 0, herbivores = 0, carnivores = 0;
        for(auto* org : world.population) {
            // CHANGED: Look for sticks instead of segments, and use dot notation
            if(!org->sticks.empty() && org->isAlive) {
                if(org->sticks[0].type == ColorType::GREEN) plants++;
                else if(org->sticks[0].type == ColorType::WHITE) herbivores++;
                else if(org->sticks[0].type == ColorType::RED) carnivores++;
            }
        }

        if(frameCounter++ % 10 == 0) {
            popHistory.erase(popHistory.begin()); popHistory.push_back((float)world.population.size());
            plantHistory.erase(plantHistory.begin()); plantHistory.push_back((float)plants);
            herbivoreHistory.erase(herbivoreHistory.begin()); herbivoreHistory.push_back((float)herbivores);
            carnivoreHistory.erase(carnivoreHistory.begin()); carnivoreHistory.push_back((float)carnivores);
        }

        ImGui::Begin("Jolt Sandbox Core Monitor");
        ImGui::Text("Population: %zu / %d", world.population.size(), world.maxPopulation);
        ImGui::SliderFloat("Time Scale", &world.timeScale, 0.0f, 5.0f);
        ImGui::SliderInt("Max Population", &world.maxPopulation, 10, 100000); 
        ImGui::Text("Active Cores: %d", std::thread::hardware_concurrency());
        
        ImGui::Separator();
        ImGui::Text("Demographics");
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Plants: %d", plants);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Herbivores: %d", herbivores);
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Carnivores: %d", carnivores);
        
        ImGui::PlotLines("Total", popHistory.data(), popHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
        ImGui::PlotLines("Plants", plantHistory.data(), plantHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
        ImGui::PlotLines("Herbivores", herbivoreHistory.data(), herbivoreHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
        ImGui::PlotLines("Carnivores", carnivoreHistory.data(), carnivoreHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));

        ImGui::Separator();
        ImGui::Text("Evolution Pressures");
        ImGui::SliderFloat("Mutation Rate", &world.mutationRate, 0.01f, 1.0f);
        ImGui::SliderFloat("Photosynthesis Rate", &world.photosynthesisRate, 0.1f, 2.0f);
        ImGui::SliderFloat("Base Metabolism", &world.baseMetabolism, 0.01f, 0.5f, "%.3f");
        ImGui::SliderFloat("Movement Cost", &world.movementCost, 0.0001f, 0.01f, "%.4f");
        
        ImGui::Separator();
        ImGui::Text("Physical Forces");
        ImGui::SliderFloat("Thrust Power", &world.thrustMultiplier, 10.0f, 200.0f);
        ImGui::SliderFloat("Turn Power", &world.turnMultiplier, 5.0f, 100.0f);
        
        ImGui::Separator();
        ImGui::Text("Camera Controls: W, A, S, D, Q (Zoom In), E (Zoom Out)");


        if (ImGui::Button("Reset World", ImVec2(150.0f, 40.0f))) {
            world.initEden();
        }

        ImGui::End();

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