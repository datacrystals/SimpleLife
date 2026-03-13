#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "World.h"
#include "Types.h"
#include <vector>
#include <thread>
#include <algorithm> // for std::max

std::mt19937 rng(42);
std::uniform_real_distribution<float> randFloat(0.0f, 1.0f);

float camX = 0.0f, camY = 0.0f, camZoom = 100.0f, aspect = 1.0f;

std::vector<float> popHistory;
std::vector<float> plantHistory;
std::vector<float> herbivoreHistory;
std::vector<float> carnivoreHistory;

void renderWorld(const World& world) {
    glLineWidth(3.0f);
    
    for (const auto* org : world.population) {
        if (org->points.empty()) continue;

        glBegin(GL_LINES); 
        for (const auto& stick : org->sticks) {
            if (stick.isHidden) continue;

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

            if (std::abs(dx) > world.WORLD_WIDTH * 0.5f || std::abs(dy) > world.WORLD_HEIGHT * 0.5f) {
                float g2x = p2.x, g2y = p2.y;
                if (dx > world.WORLD_WIDTH * 0.5f) g2x -=world.WORLD_WIDTH;
                else if (dx < -world.WORLD_WIDTH * 0.5f) g2x += world.WORLD_WIDTH;
                if (dy > world.WORLD_HEIGHT * 0.5f) g2y -= world.WORLD_HEIGHT;
                else if (dy < -world.WORLD_HEIGHT * 0.5f) g2y += world.WORLD_HEIGHT;

                glVertex2f(p1.x, p1.y);
                glVertex2f(g2x, g2y);

                float g1x = p1.x, g1y = p1.y;
                if (dx > world.WORLD_WIDTH * 0.5f) g1x += world.WORLD_WIDTH;
                else if (dx < -world.WORLD_WIDTH * 0.5f) g1x -= world.WORLD_WIDTH;
                if (dy > world.WORLD_HEIGHT * 0.5f) g1y += world.WORLD_HEIGHT;
                else if (dy < -world.WORLD_HEIGHT * 0.5f) g1y -= world.WORLD_HEIGHT;

                glVertex2f(g1x, g1y);
                glVertex2f(p2.x, p2.y);
            } else {
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
        float avgSym = 0.0f, avgLife = 0.0f;
        int aliveCount = 0;

        for(auto* org : world.population) {
            if(!org->sticks.empty() && org->isAlive) {
                avgSym += org->dna.symmetry;
                avgLife += org->dna.lifespan;
                aliveCount++;
                if(org->sticks[0].type == ColorType::GREEN) plants++;
                else if(org->sticks[0].type == ColorType::WHITE) herbivores++;
                else if(org->sticks[0].type == ColorType::RED) carnivores++;
            }
        }

        if (aliveCount > 0) {
            avgSym /= aliveCount;
            avgLife /= aliveCount;
        }

        if(frameCounter++ % 10 == 0) {
            popHistory.erase(popHistory.begin()); popHistory.push_back((float)world.population.size());
            plantHistory.erase(plantHistory.begin()); plantHistory.push_back((float)plants);
            herbivoreHistory.erase(herbivoreHistory.begin()); herbivoreHistory.push_back((float)herbivores);
            carnivoreHistory.erase(carnivoreHistory.begin()); carnivoreHistory.push_back((float)carnivores);
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 600), ImVec2(600, 900));
        ImGui::Begin("Jolt Sandbox Core Monitor");
        
        // --- Core Simulation Stats ---
        ImGui::Text("Population: %zu / %d", world.population.size(), world.maxPopulation);
        ImGui::SliderFloat("Time Scale", &world.timeScale, 0.0f, 5.0f);
        ImGui::SliderInt("Max Population", &world.maxPopulation, 10, 10000); 
        ImGui::Text("Active Cores: %d", std::thread::hardware_concurrency());
        
        if (ImGui::CollapsingHeader("Demographics & Traits", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Plants: %d", plants);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Herbivores: %d", herbivores);
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Carnivores: %d", carnivores);
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Avg Symmetry: %.2f", avgSym);
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Avg Lifespan: %.1f", avgLife);
            
            ImGui::PlotLines("Total", popHistory.data(), popHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
            ImGui::PlotLines("Plants", plantHistory.data(), plantHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
            ImGui::PlotLines("Herbivores", herbivoreHistory.data(), herbivoreHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
            ImGui::PlotLines("Carnivores", carnivoreHistory.data(), carnivoreHistory.size(), 0, NULL, 0.0f, (float)world.maxPopulation, ImVec2(0, 40));
        }

        if (ImGui::CollapsingHeader("Global Physical Rules")) {
            ImGui::SliderFloat("Base Metabolism", &world.baseMetabolism, 0.01f, 0.5f, "%.3f");
            ImGui::SliderFloat("Segment Upkeep Cost", &world.segmentCost, 0.0f, 0.1f, "%.3f");
            ImGui::SliderFloat("Size Upkeep Discount", &world.sizeDiscount, 0.0f, 0.05f, "%.3f");
            ImGui::SliderFloat("World X", &world.WORLD_WIDTH, 50.0f, 1000.0f);
            ImGui::SliderFloat("World Y", &world.WORLD_HEIGHT, 50.0f, 1000.0f);
        }

        if (ImGui::CollapsingHeader("Evolution & Mutation Limits")) {
            ImGui::SliderFloat("Global Mutation Rate", &world.mutationRate, 0.01f, 1.0f);
            
            ImGui::SeparatorText("Probabilities (when mutating)");
            ImGui::SliderFloat("Type Change Chance", &world.mutChanceType, 0.0f, 1.0f);
            ImGui::SliderFloat("Joint Flex Toggle Chance", &world.mutChanceMotor, 0.0f, 1.0f);
            ImGui::SliderFloat("Add Segment Chance", &world.mutChanceAddNode, 0.0f, 2.0f);
            
            ImGui::SeparatorText("Trait Boundaries");
            ImGui::SliderInt("Min Symmetry", &world.minSymmetry, 1, 8);
            ImGui::SliderInt("Max Symmetry", &world.maxSymmetry, 1, 8);
            if (world.minSymmetry > world.maxSymmetry) world.maxSymmetry = world.minSymmetry; // Keep bounds sane
            
            ImGui::SliderFloat("Min Lifespan", &world.minLifespan, 5.0f, 200.0f);
            ImGui::SliderFloat("Max Lifespan", &world.maxLifespan, 5.0f, 200.0f);
            if (world.minLifespan > world.maxLifespan) world.maxLifespan = world.minLifespan;
        }
        
        if (ImGui::CollapsingHeader("Segment Types & Rules")) {
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
            ImGui::SeparatorText("GREEN (Plant)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Generates energy passively via photosynthesis. Grouping segments too closely causes shading, which lowers efficiency.");
            ImGui::SliderFloat("Photosynthesis Rate", &world.photosynthesisRate, 0.1f, 2.0f);
            ImGui::SliderFloat("Shade Penalty", &world.shadePenalty, 0.0f, 2.0f);
            ImGui::SliderFloat("Shade Radius", &world.greenCrowdRadius, 1.0f, 20.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::SeparatorText("WHITE (Herbivore)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Instantly kills other organisms by touching them, stealing a fixed chunk of energy.");
            ImGui::SliderFloat("Energy Gained/Kill", &world.herbivoreEatEnergy, 10.0f, 500.0f);
            ImGui::SliderFloat("Eat Range", &world.herbivoreAttackRange, 0.5f, 10.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            ImGui::SeparatorText("RED (Carnivore)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Deals continuous DPS to segments in range, converting a percentage of damage into energy.");
            ImGui::SliderFloat("Damage / Second", &world.damageAmount, 10.0f, 200.0f);
            ImGui::SliderFloat("Attack Range", &world.carnivoreAttackRange, 0.5f, 10.0f);
            ImGui::SliderFloat("Energy Efficiency", &world.carnivoreEfficiency, 0.1f, 2.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.9f, 1.0f));
            ImGui::SeparatorText("PURPLE (Shield Armor)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Defensive armor. Mitigates incoming red carnivore damage but requires constant energy upkeep.");
            ImGui::SliderFloat("Shield Efficiency", &world.shieldEfficiency, 0.0f, 1.0f);
            ImGui::SliderFloat("Shield Upkeep Cost", &world.shieldCost, 0.0f, 2.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.8f, 0.2f, 1.0f));
            ImGui::SeparatorText("YELLOW (Thruster Muscle)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Fires linear thrust based on neural sensors, consuming energy relative to the force applied.");
            ImGui::SliderFloat("Thrust Power", &world.thrustMultiplier, 0.0f, 2.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.4f, 0.9f, 1.0f));
            ImGui::SeparatorText("BLUE (Torque Muscle)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Fires rotational torque based on neural sensors, consuming energy relative to the turning force.");
            ImGui::SliderFloat("Turn Power", &world.turnMultiplier, 0.0f, 1.0f);

            ImGui::Text("Movement Energy Cost");
            ImGui::SliderFloat("Action Cost", &world.movementCost, 0.0001f, 0.01f, "%.4f");
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::SeparatorText("DEAD (Grey)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Dead segments that act as rigid structural scaffolding or corpses. They do nothing but take up space.");
        }
        
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