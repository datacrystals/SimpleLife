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
    std::vector<float> voltHistory;

    std::string GetInputSourceName(const Organism& org, int neuronIdx) {
        int nId = org.brain.neurons[neuronIdx].id;
        for (const auto& gene : org.dna.morphology) {
            if (gene.ioNeuronId == nId) {
                if (gene.type == ColorType::GREEN) return "Photosyn-Sense";
                if (gene.sensorRange > 0) return "Eye/Antenna";
                return "Part-Stress";
            }
        }
        if (neuronIdx == 0) return "Clock-Sin";
        if (neuronIdx == 1) return "Clock-Cos";
        return "Unknown Input";
    }

    void DrawLayeredTopology(ImDrawList* drawList, Organism& org, ImVec2 canvasPos, ImVec2 canvasSize) {
        auto& brain = org.brain;
        int n = (int)brain.neurons.size();
        if (n == 0) return;

        std::vector<int> sensory, hidden, motor;
        for (int i = 0; i < n; i++) {
            if (brain.neurons[i].role == NeuronRole::SENSORY) sensory.push_back(i);
            else if (brain.neurons[i].role == NeuronRole::MOTOR) motor.push_back(i);
            else hidden.push_back(i);
        }

        std::vector<ImVec2> pos(n);
        auto MapLayer = [&](const std::vector<int>& idxs, float xPct) {
            for (size_t i = 0; i < idxs.size(); i++) {
                float yPct = (idxs.size() > 1) ? (float)i / (idxs.size() - 1) : 0.5f;
                pos[idxs[i]] = ImVec2(canvasPos.x + canvasSize.x * xPct, canvasPos.y + 50.0f + (canvasSize.y - 100.0f) * yPct);
            }
        };
        MapLayer(sensory, 0.12f); MapLayer(hidden, 0.5f); MapLayer(motor, 0.88f);

        for (const auto& syn : brain.synapses) {
            ImVec2 start = pos[syn.source_idx], end = pos[syn.target_idx];
            bool isExc = (brain.neurons[syn.source_idx].polarity == NeuronPolarity::EXCITATORY);
            ImU32 col = isExc ? IM_COL32(255, 255, 100, 120) : IM_COL32(100, 150, 255, 120);
            
            drawList->AddLine(start, end, col, std::clamp(std::abs(syn.weight), 0.5f, 4.0f));
            
            ImVec2 mid = ImVec2((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
            drawList->AddText(mid, IM_COL32(200, 200, 200, 180), std::to_string((int)syn.weight).c_str());
        }

        ImVec2 mPos = ImGui::GetMousePos();
        for (int i = 0; i < n; i++) {
            auto& node = brain.neurons[i];
            
            ImU32 col = (node.role == NeuronRole::SENSORY) ? IM_COL32(0, 255, 0, 255) :
                        (node.role == NeuronRole::MOTOR)   ? IM_COL32(255, 0, 0, 255) :
                        (node.polarity == NeuronPolarity::EXCITATORY) ? IM_COL32(255, 255, 0, 255) : IM_COL32(0, 150, 255, 255);

            if (node.spikedThisTick) col = IM_COL32(255, 255, 255, 255);
            
            if (ImGui::IsMouseClicked(0) && std::hypot(mPos.x - pos[i].x, mPos.y - pos[i].y) < 15.0f) 
                selectedNeuronIndex = i;

            drawList->AddCircleFilled(pos[i], (i == selectedNeuronIndex ? 12.0f : 8.0f), col);
            
            if (node.role != NeuronRole::HIDDEN) {
                 std::string label = (node.role == NeuronRole::SENSORY) ? GetInputSourceName(org, i) : "Muscle";
                 drawList->AddText(ImVec2(pos[i].x - 20, pos[i].y - 25), IM_COL32(255, 255, 255, 200), label.c_str());
            }
        }
    }

public:
    std::string GetName() const override { return "Brain Monitor"; }

    void Draw(World& world) override {
        if (!ImGui::Begin(GetName().c_str(), &isOpen)) {
            ImGui::End();
            return;
        }
    
        // --- Auto-Sync Selection ---
        if (world.selectedOrgId != -1) {
            // Find the index of the organism with this ID
            for (int i = 0; i < (int)world.population.size(); i++) {
                if (world.population[i]->id == world.selectedOrgId) {
                    selectedOrgIndex = i;
                    break;
                }
            }
        }

        if (world.population.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "POPULATION EXTINCT");
            ImGui::End();
            return;
        }

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

        ImGui::Text("Network Topology");
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 400);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 20, 25, 255));
        
        DrawLayeredTopology(dl, *org, canvasPos, canvasSize);
        ImGui::Dummy(canvasSize);

        ImGui::NextColumn();

        ImGui::Text("Live Neural Data");
        
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
};