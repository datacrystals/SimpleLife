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



static std::vector<float> historySensory;
static std::vector<float> historyHidden;
static std::vector<float> historyMotor;
static std::vector<float> historySpikes;
static int selectedNeuronIndex = -1;
static int selectedOrgIndex = 0;





float camX = 100.0f, camY = 70.0f, camZoom = 100.0f, aspect = 1.0f;







// Helper to identify what a sensory neuron is actually looking at
std::string GetInputSourceName(const Organism& org, int neuronIdx) {
    int nId = org.brain.neurons[neuronIdx].id;
    for (const auto& gene : org.dna.morphology) {
        if (gene.ioNeuronId == nId) {
            if (gene.type == ColorType::GREEN) return "Photosyn-Sense";
            if (gene.sensorRange > 0) return "Eye/Antenna";
            return "Part-Stress";
        }
    }
    // Default hardcoded inputs from World.h updateTick
    if (neuronIdx == 0) return "Clock-Sin";
    if (neuronIdx == 1) return "Clock-Cos";
    return "Unknown Input";
}

void DrawLayeredTopology(ImDrawList* drawList, Organism& org, ImVec2 canvasPos, ImVec2 canvasSize) {
    auto& brain = org.brain;
    int n = (int)brain.neurons.size();
    if (n == 0) return;

    // 1. Layer Categorization
    std::vector<int> sensory, hidden, motor;
    for (int i = 0; i < n; i++) {
        if (brain.neurons[i].role == NeuronRole::SENSORY) sensory.push_back(i);
        else if (brain.neurons[i].role == NeuronRole::MOTOR) motor.push_back(i);
        else hidden.push_back(i);
    }

    // 2. Position Mapping
    std::vector<ImVec2> pos(n);
    auto MapLayer = [&](const std::vector<int>& idxs, float xPct) {
        for (size_t i = 0; i < idxs.size(); i++) {
            float yPct = (idxs.size() > 1) ? (float)i / (idxs.size() - 1) : 0.5f;
            pos[idxs[i]] = ImVec2(canvasPos.x + canvasSize.x * xPct, canvasPos.y + 50.0f + (canvasSize.y - 100.0f) * yPct);
        }
    };
    MapLayer(sensory, 0.12f); MapLayer(hidden, 0.5f); MapLayer(motor, 0.88f);

    // 3. Draw Synapses with Weight Labels
    for (const auto& syn : brain.synapses) {
        ImVec2 start = pos[syn.source_idx], end = pos[syn.target_idx];
        bool isExc = (brain.neurons[syn.source_idx].polarity == NeuronPolarity::EXCITATORY);
        ImU32 col = isExc ? IM_COL32(255, 255, 100, 120) : IM_COL32(100, 150, 255, 120);
        
        drawList->AddLine(start, end, col, std::clamp(std::abs(syn.weight), 0.5f, 4.0f));
        
        // Mid-point weight label
        ImVec2 mid = ImVec2((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
        drawList->AddText(mid, IM_COL32(200, 200, 200, 180), std::to_string((int)syn.weight).c_str());
    }

    // 4. Draw Neurons & Interaction
    ImVec2 mPos = ImGui::GetMousePos();
    for (int i = 0; i < n; i++) {
        auto& node = brain.neurons[i];
        
        // Special Roles: Green (In), Red (Out), Yellow/Blue (Hidden)
        ImU32 col = (node.role == NeuronRole::SENSORY) ? IM_COL32(0, 255, 0, 255) :
                    (node.role == NeuronRole::MOTOR)   ? IM_COL32(255, 0, 0, 255) :
                    (node.polarity == NeuronPolarity::EXCITATORY) ? IM_COL32(255, 255, 0, 255) : IM_COL32(0, 150, 255, 255);

        if (node.spikedThisTick) col = IM_COL32(255, 255, 255, 255);
        
        // Hit-test for selection
        if (ImGui::IsMouseClicked(0) && std::hypot(mPos.x - pos[i].x, mPos.y - pos[i].y) < 15.0f) 
            selectedNeuronIndex = i;

        drawList->AddCircleFilled(pos[i], (i == selectedNeuronIndex ? 12.0f : 8.0f), col);
        
        // Restore Labels
        if (node.role != NeuronRole::HIDDEN) {
             std::string label = (node.role == NeuronRole::SENSORY) ? GetInputSourceName(org, i) : "Muscle";
             drawList->AddText(ImVec2(pos[i].x - 20, pos[i].y - 25), IM_COL32(255, 255, 255, 200), label.c_str());
        }
    }
}

void RenderBrainMonitor(World& world) {
    ImGui::Begin("Brain Monitor");

    if (world.population.empty()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "POPULATION EXTINCT");
        ImGui::End();
        return;
    }

    // --- Subject Selector ---
    if (selectedOrgIndex >= (int)world.population.size()) selectedOrgIndex = 0;
    auto& org = world.population[selectedOrgIndex];
    
    if (ImGui::BeginCombo("Select Org", ("ID: " + std::to_string(org->id)).c_str())) {
        for (int i = 0; i < (int)world.population.size(); i++) {
            if (ImGui::Selectable(("ID: " + std::to_string(world.population[i]->id)).c_str(), selectedOrgIndex == i))
                selectedOrgIndex = i;
        }
        ImGui::EndCombo();
    }

    ImGui::Columns(2, "MonitorSplit", true);

    // --- Left: Network View ---
    ImGui::Text("Network Topology");
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 400);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 20, 25, 255));
    
    DrawLayeredTopology(dl, *org, canvasPos, canvasSize); // Fixed dereference
    ImGui::Dummy(canvasSize);

    ImGui::NextColumn();

    // --- Right: High-Density Graphs ---
    ImGui::Text("Live Neural Data");
    
    // Auto-update history (Simplified version of your history logic)
    static std::vector<float> voltHistory;
    float currentAvgVolt = 0;
    for(auto& n : org->brain.neurons) currentAvgVolt += n.membranePotential;
    voltHistory.push_back(currentAvgVolt / std::max(1, (int)org->brain.neurons.size()));
    if(voltHistory.size() > 100) voltHistory.erase(voltHistory.begin());

    ImGui::PlotLines("##volts", voltHistory.data(), voltHistory.size(), 0, "Avg Potential", 0.0f, 1.2f, ImVec2(0, 80));
    
    ImGui::Separator();
    
    if (selectedNeuronIndex != -1 && selectedNeuronIndex < (int)org->brain.neurons.size()) {
        auto& sn = org->brain.neurons[selectedNeuronIndex];
        ImGui::Text("Neuron %d [Role: %d]", selectedNeuronIndex, (int)sn.role);
        ImGui::Value("Potential", sn.membranePotential);
        ImGui::Value("Threshold", sn.threshold);
        ImGui::ProgressBar(sn.membranePotential / sn.threshold, ImVec2(-1, 0));
    } else {
        ImGui::TextDisabled("Click a neuron to inspect.");
    }

    ImGui::Columns(1);
    ImGui::End();
}




void drawJoint(float x, float y, float size, bool isFlexible) {
    if (isFlexible) {
        // Blue Circle
        glColor3f(0.2f, 0.4f, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= 12; i++) {
            // Fixing syntax error: i * 3.14159f * 2.0f / 12.0f
            float angle = i * 3.14159f * 2.0f / 12.0f;
            // Using 'size' instead of 1.2f
            glVertex2f(x + cos(angle) * size, y + sin(angle) * size);
        }
        glEnd();
    } else {
        // Purple Square
        glColor3f(0.6f, 0.2f, 0.8f);
        glBegin(GL_QUADS);
        // Using 'size' instead of 1.0f (half-width)
        glVertex2f(x - size, y - size);
        glVertex2f(x + size, y - size);
        glVertex2f(x + size, y + size);
        glVertex2f(x - size, y + size);
        glEnd();
    }
}

void renderWorld(World& world) {
    float ww = world.getConfig().worldWidth;
    float wh = world.getConfig().worldHeight;

    for (const auto& org : world.population) {
        if (org->points.empty()) continue;

        // 1. Determine joint types (Flexible if attached to a muscle)
        std::vector<bool> flexibleJoints(org->points.size(), false);
        for (size_t i = 0; i < org->springs.size(); ++i) {
            if (org->bodyParts[i].isMuscle) {
                flexibleJoints[org->springs[i].p1_idx] = true;
                flexibleJoints[org->springs[i].p2_idx] = true;
            }
        }

        // 2. Draw Segments (Bones, Muscles, Sensors)
        glLineWidth(3.0f);
        glBegin(GL_LINES); 
        for (size_t i = 0; i < org->springs.size(); ++i) {
            const auto& spring = org->springs[i];
            const auto& bp = org->bodyParts[i];
            const auto& p1 = org->points[spring.p1_idx];
            const auto& p2 = org->points[spring.p2_idx];

            if (std::abs(p2.x - p1.x) > ww * 0.5f || std::abs(p2.y - p1.y) > wh * 0.5f) continue;

            if (!org->isAlive) {
                glColor3f(0.3f, 0.3f, 0.3f);
            } else if (bp.isMuscle) {
                // Maroon to Bright Red based on tension
                float tension = bp.currentTension; 
                glColor3f(0.4f + (tension * 0.6f), 0.05f + (tension * 0.15f), 0.1f + (tension * 0.1f));
            } else if (bp.type == ColorType::DEAD || bp.type == ColorType::WHITE) {
                glColor3f(0.9f, 0.9f, 0.9f); // Bone
            } else if (bp.type == ColorType::GREEN) {
                glColor3f(0.2f, 0.8f, 0.2f); // Plant
            } else if (bp.type == ColorType::RED) {
                // Glow brighter if the attack neuron is spiking
                glColor3f(0.9f, 0.1f, 0.1f); 
            }

            glVertex2f(p1.x, p1.y);
            glVertex2f(p2.x, p2.y);

            // Draw Sensory Raycasts
            if (bp.sensorRange > 0.0f && org->isAlive) {
                glColor4f(1.0f, 1.0f, 0.0f, 0.3f); // Faint yellow ray
                float dx = p2.x - p1.x; float dy = p2.y - p1.y;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len > 0.001f) {
                    glVertex2f(p2.x, p2.y);
                    glVertex2f(p2.x + (dx/len) * bp.sensorRange, p2.y + (dy/len) * bp.sensorRange);
                }
            }
        }
        glEnd();

        // 3. Draw Joints
        if (org->isAlive) {
            for (size_t i = 0; i < org->points.size(); ++i) {
                drawJoint(org->points[i].x, org->points[i].y, 0.3f, flexibleJoints[i]);
            }
        }
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

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 600), ImVec2(6000, 9000));
        ImGui::Begin("Sandbox Core Monitor");

        SimConfig& cfg = world.getConfig();

        ImGui::Text("Population: %zu / %d", world.population.size(), cfg.maxPopulation);
        ImGui::SliderFloat("Time Scale", &cfg.timeScale, 0.0f, 5.0f);

        if (ImGui::Button("Reset Eden", ImVec2(150.0f, 40.0f))) world.initializeEden();

        if (ImGui::CollapsingHeader("Global Physical Rules", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Base Metabolism", &cfg.baseMetabolism, 0.01f, 0.5f, "%.3f");
            ImGui::SliderFloat("Segment Cost", &cfg.segmentCost, 0.0f, 0.1f, "%.3f");
            ImGui::SliderFloat("World X", &cfg.worldWidth, 50.0f, 1000.0f);
            ImGui::SliderFloat("World Y", &cfg.worldHeight, 50.0f, 1000.0f);
            world.engine.updateBounds(cfg.worldWidth, cfg.worldHeight);
            ImGui::SliderFloat("Friction (Drag)", &cfg.friction, 0.8f, 1.0f);

            ImGui::SliderInt("Max Population", &cfg.maxPopulation, 10, 10000);
        }

        if (ImGui::CollapsingHeader("Evolution & Mutation Limits")) {
            ImGui::SliderFloat("Global Mutation Rate", &cfg.globalMutationRate, 0.01f, 1.0f);
            ImGui::SeparatorText("Morphology Probabilities");
            ImGui::SliderFloat("Type Change Chance", &cfg.mutChanceType, 0.0f, 1.0f);
            ImGui::SliderFloat("Joint Flex Toggle", &cfg.mutChanceMotor, 0.0f, 1.0f);
            ImGui::SliderFloat("Add Node Chance", &cfg.mutChanceAddNode, 0.0f, 2.0f);
            ImGui::SeparatorText("SNN Probabilities");
            ImGui::SliderFloat("Add Neuron", &cfg.mutChanceAddNeuron, 0.0f, 1.0f);
            ImGui::SliderFloat("Add Synapse", &cfg.mutChanceAddSynapse, 0.0f, 1.0f);
            ImGui::SliderFloat("Change Weight", &cfg.mutChanceChangeWeight, 0.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Segment Types & Rules")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
            ImGui::SeparatorText("GREEN (Plant)");
            ImGui::PopStyleColor();
            ImGui::SliderFloat("Photosynthesis Rate", &cfg.photosynthesisRate, 0.1f, 2.0f);
            ImGui::SliderFloat("Shade Radius", &cfg.greenCrowdRadius, 1.0f, 20.0f);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            ImGui::SeparatorText("RED (Active Weapon)");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("Must be triggered by a motor neuron. Damage scales with collision force.");
            ImGui::SliderFloat("Base Damage Output", &cfg.carnivoreDamagePerSec, 10.0f, 200.0f);
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.4f, 1.0f));
            ImGui::SeparatorText("MAROON (Muscle)");
            ImGui::PopStyleColor();
            ImGui::SliderFloat("Flex Cost", &cfg.movementCost, 0.0001f, 0.05f, "%.4f");
        }


        // --- Spawning & Clearing Controls ---
        ImGui::SeparatorText("World Controls");
                
        if (ImGui::Button("Clear World", ImVec2(100.0f, 30.0f))) {
            world.clearWorld();
        }

        ImGui::SameLine();
        if (ImGui::Button("Spawn Eden", ImVec2(100.0f, 30.0f))) {
            world.initializeEden();
        }

        if (ImGui::Button("Add Green Blob", ImVec2(120.0f, 30.0f))) {
            // Spawn in the center of the camera
            world.spawnSimpleGreen(camX, camY);
        }

        ImGui::SameLine();
        if (ImGui::Button("Add Worm", ImVec2(120.0f, 30.0f))) {
            world.spawnWorm(camX, camY);
        }
        // ------------------------------------

        ImGui::End();

        RenderBrainMonitor(world);

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