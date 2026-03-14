#pragma once
#include "UIWindow.h"
#include "imgui.h"

class SettingsWindow : public UIWindow {
public:
    std::string GetName() const override { return "Core Settings"; }

    void Draw(World& world) override {
        if (!ImGui::Begin(GetName().c_str(), &isOpen)) {
            ImGui::End();
            return;
        }

        SimConfig& cfg = world.getConfig();
        ImGui::Text("Population: %zu / %d", world.population.size(), cfg.maxPopulation);
        ImGui::SliderFloat("Time Scale", &cfg.timeScale, 0.0f, 5.0f);

        if (ImGui::Button("Reset Eden", ImVec2(150.0f, 40.0f))) {
            world.initializeEden();
        }
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
            world.spawnSimpleGreen(10, 10);
        }

        ImGui::SameLine();
        if (ImGui::Button("Add Worm", ImVec2(120.0f, 30.0f))) {
            world.spawnWorm(10, 10);
        }
        // ------------------------------------

        ImGui::End();
    }
};