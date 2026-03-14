/**
 * @file BrainMonitorWindow.h
 * @brief Spatial SNN Inspector with Zoom, Pan, and IO Labeling.
 */
#pragma once
#include "UIWindow.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

class BrainMonitorWindow : public UIWindow {
private:
    int selectedNeuronIndex = -1;
    int selectedOrgIndex = 0;
    
    // Neural Canvas State
    ImVec2 brainOffset = ImVec2(0.5f, 0.5f); // Start centered in 0-1 space
    float brainZoom = 0.8f;                  // Slight zoom out to see borders
    std::vector<float> voltHistory;

    /**
     * @brief Translates SNN role/ID into a human-readable label for the UI.
     */
    std::string GetInputSourceName(const Organism& org, int neuronIdx) {
        if (neuronIdx < 0 || neuronIdx >= (int)org.brain.neurons.size()) return "Unknown";
        
        int nId = org.brain.neurons[neuronIdx].id;
        for (const auto& gene : org.dna.morphology) {
            if (gene.ioNeuronId == nId) {
                if (gene.type == ColorType::GREEN) return "Photosyn-Sense";
                if (gene.sensorRange > 0) return "Eye/Antenna";
                return "Part-Stress";
            }
        }
        // Fallback for hardcoded world inputs
        if (neuronIdx == 0) return "Energy-Sense";
        if (neuronIdx == 1) return "Density-Sense";
        return "Hidden/Internal";
    }

    /**
     * @brief Renders the SNN based on genetic 2D coordinates.
     */
    void DrawSpatialTopology(ImDrawList* drawList, Organism& org, ImVec2 canvasPos, ImVec2 canvasSize) {
        auto& brain = org.brain;
        if (brain.neurons.empty()) return;

        // --- SAFETY CHECK: Fix Zero Zoom/Offset ---
        if (brainZoom < 0.01f) brainZoom = 0.01f;

        // --- Coordinate Mapping ---
        auto ToCanvas = [&](float x, float y) {
            float viewX = (x - 0.5f) * brainZoom + 0.5f + (brainOffset.x - 0.5f);
            float viewY = (y - 0.5f) * brainZoom + 0.5f + (brainOffset.y - 0.5f);
            return ImVec2(canvasPos.x + viewX * canvasSize.x, canvasPos.y + viewY * canvasSize.y);
        };

        // --- Interaction: Pan & Zoom ---
        if (ImGui::IsWindowHovered()) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                brainOffset.x += delta.x / canvasSize.x;
                brainOffset.y += delta.y / canvasSize.y;
            }
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                brainZoom *= (wheel > 0) ? 1.1f : 0.9f;
            }
        }

        // --- 1. Draw Background Grid (Pans & Zooms with network) ---
        // Draws a grid from -2.0 to 3.0 in brain-space to allow for plenty of panning
        for (float t = -2.0f; t <= 3.0f; t += 0.1f) {
            ImVec2 pStartV = ToCanvas(t, -2.0f); ImVec2 pEndV = ToCanvas(t, 3.0f);
            ImVec2 pStartH = ToCanvas(-2.0f, t); ImVec2 pEndH = ToCanvas(3.0f, t);
            ImU32 gridCol = IM_COL32(40, 40, 45, 255); // Subtle dark grey
            drawList->AddLine(pStartV, pEndV, gridCol, 1.0f);
            drawList->AddLine(pStartH, pEndH, gridCol, 1.0f);
        }

        // --- 2. Dynamic Bounding Box with Margin ---
        float minX = 999.0f, maxX = -999.0f;
        float minY = 999.0f, maxY = -999.0f;
        for (const auto& n : brain.neurons) {
            minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
            minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
        }
        
        if (minX > maxX) { minX = 0; maxX = 1; minY = 0; maxY = 1; }

        // Calculate a 5% margin based on the network's overall width/height
        float marginX = std::max(0.05f, (maxX - minX) * 0.05f); 
        float marginY = std::max(0.05f, (maxY - minY) * 0.05f);

        ImVec2 tl = ToCanvas(minX - marginX, minY - marginY);
        ImVec2 br = ToCanvas(maxX + marginX, maxY + marginY);
        drawList->AddRect(tl, br, IM_COL32(0, 150, 255, 180), 0, 0, 1.5f); // Scaled Blue Box

        // --- 3. Draw Synapses (Varying strength & polarity color) ---
        for (const auto& syn : brain.synapses) {
            if (syn.source_idx >= 0 && syn.source_idx < (int)brain.neurons.size() &&
                syn.target_idx >= 0 && syn.target_idx < (int)brain.neurons.size()) {
                
                auto& sourceNode = brain.neurons[syn.source_idx];
                auto& targetNode = brain.neurons[syn.target_idx];
                
                ImVec2 p1 = ToCanvas(sourceNode.x, sourceNode.y);
                ImVec2 p2 = ToCanvas(targetNode.x, targetNode.y);
                
                // Map the connection weight to thickness and alpha (opacity)
                // Weights typically range 0.0 to 3.0 in your init
                float thickness = std::max(1.0f, syn.weight * 1.5f * std::sqrt(brainZoom));
                int alpha = std::min(255, std::max(30, static_cast<int>(syn.weight * 70)));
                
                // Color the line based on the SOURCE neuron's polarity
                ImU32 lineCol = (sourceNode.polarity == NeuronPolarity::EXCITATORY) ? 
                                IM_COL32(255, 255, 0, alpha) :   // Excitatory = Yellow Tint
                                IM_COL32(0, 150, 255, alpha);    // Inhibitory = Blue Tint
                
                drawList->AddLine(p1, p2, lineCol, thickness);
            }
        }

        // --- 4. Draw Neurons & Handle Selection ---
        ImVec2 mousePos = ImGui::GetMousePos();
        bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        for (int i = 0; i < (int)brain.neurons.size(); i++) {
            auto& node = brain.neurons[i];
            ImVec2 p = ToCanvas(node.x, node.y);
            
            float radius = 5.0f * std::sqrt(brainZoom);
            radius = std::max(4.0f, std::min(radius, 12.0f));

            if (isMouseClicked && ImGui::IsWindowHovered()) {
                float dx = mousePos.x - p.x;
                float dy = mousePos.y - p.y;
                if ((dx * dx + dy * dy) <= (radius * radius)) {
                    selectedNeuronIndex = i;
                }
            }

            ImU32 fillCol = (node.polarity == NeuronPolarity::EXCITATORY) ? 
                            IM_COL32(255, 255, 0, 255) : 
                            IM_COL32(0, 150, 255, 255);
            
            if (selectedNeuronIndex == i) {
                drawList->AddCircleFilled(p, radius + 3.0f, IM_COL32(255, 255, 255, 255));
            }

            drawList->AddCircleFilled(p, radius, fillCol);
            
            ImU32 outlineCol = IM_COL32(100, 100, 100, 255); 
            if (node.role == NeuronRole::SENSORY) outlineCol = IM_COL32(0, 255, 0, 255); 
            if (node.role == NeuronRole::MOTOR) outlineCol = IM_COL32(255, 0, 0, 255);   
            
            drawList->AddCircle(p, radius, outlineCol, 0, 2.0f);
        }
    }
    
public:
    std::string GetName() const override { return "Brain Monitor (Spatial)"; }

    void Draw(World& world) override {
        if (!ImGui::Begin(GetName().c_str(), &isOpen)) {
            ImGui::End();
            return;
        }

        if (world.population.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "POPULATION EXTINCT");
            ImGui::End();
            return;
        }

        // Auto-sync with world selection
        if (world.selectedOrgId != -1) {
            for (int i = 0; i < (int)world.population.size(); i++) {
                if (world.population[i]->id == world.selectedOrgId) {
                    selectedOrgIndex = i;
                    break;
                }
            }
        }

        // Safety clamp
        if (selectedOrgIndex >= (int)world.population.size()) selectedOrgIndex = 0;
        auto& org = world.population[selectedOrgIndex];

        // Header Info
        ImGui::Text("Inspecting Org ID: %d", org->id);
        ImGui::SameLine();
        if (ImGui::Button("Recenter View")) { brainOffset = ImVec2(0.5f, 0.5f); brainZoom = 1.0f; }

        ImGui::Columns(2, "MonitorSplit", true);

        // --- Left Column: Spatial Network ---
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y = std::max(canvasSize.y, 400.0f); // Minimum height

        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Background
        dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(25, 25, 30, 255));
        
        DrawSpatialTopology(dl, *org, canvasPos, canvasSize);
        ImGui::Dummy(canvasSize); // Reserve the space for ImGui layout

        ImGui::NextColumn();

        // --- Right Column: Stats & Selection ---
        ImGui::Text("Neural Vitals");
        
        // Potential Tracking
        float currentAvgVolt = 0;
        for(auto& n : org->brain.neurons) currentAvgVolt += n.membranePotential;
        voltHistory.push_back(currentAvgVolt / std::max(1, (int)org->brain.neurons.size()));
        if(voltHistory.size() > 100) voltHistory.erase(voltHistory.begin());

        ImGui::PlotLines("##volts", voltHistory.data(), voltHistory.size(), 0, "Avg Population Potential", 0.0f, 1.2f, ImVec2(-1, 80));
        
        ImGui::Separator();
        
        if (selectedNeuronIndex != -1 && selectedNeuronIndex < (int)org->brain.neurons.size()) {
            auto& sn = org->brain.neurons[selectedNeuronIndex];
            ImGui::Text("Selected Neuron Index: %d", selectedNeuronIndex);
            ImGui::Text("ID: %d | Role: %s", sn.id, (sn.role == NeuronRole::SENSORY ? "Sensory" : sn.role == NeuronRole::MOTOR ? "Motor" : "Hidden"));
            ImGui::Text("Type: %s", (sn.polarity == NeuronPolarity::EXCITATORY ? "Excitatory" : "Inhibitory"));
            
            ImGui::Spacing();
            ImGui::Value("Membrane Potential", sn.membranePotential);
            ImGui::ProgressBar(sn.membranePotential / sn.threshold, ImVec2(-1, 20), "Charge Level");
            ImGui::Text("Threshold: %.2f | Leak: %.3f", sn.threshold, sn.leakRate);
        } else {
            ImGui::TextDisabled("Left-click a neuron to inspect.");
        }

        ImGui::Columns(1);
        ImGui::End();
    }
};