#pragma once
#include "UIWindow.h"
#include <imgui.h>
#include <filesystem>

class StateManagerWindow : public UIWindow {
private:
    // Local UI buffers for paths and manual naming
    char manualSaveName[128] = "my_experiment_snapshot";
    char loadWorldPath[256] = "autosaves/eden_prime/world_snapshot.json";
    char importOrgPath[256] = "saves/creatures/champion.json";

public:
    std::string GetName() const override { return "State Manager"; }

    void Draw(World& world) override {
        if (!isOpen) return;

        if (ImGui::Begin(GetName().c_str(), &isOpen)) {
            
            // --- SESSION & AUTOSAVE ---
            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "SYSTEM STATUS");
            ImGui::Text("Current Session: %s", world.worldSessionName.c_str());
            
            ImGui::Checkbox("Enable Autosave", &world.autosaveEnabled);
            if (world.autosaveEnabled) {
                ImGui::SliderFloat("Interval (s)", &world.autosaveIntervalSeconds, 10.0f, 600.0f);
                float remaining = world.autosaveIntervalSeconds - world.timeSinceLastSave;
                ImGui::ProgressBar(1.0f - (remaining / world.autosaveIntervalSeconds), ImVec2(-1, 0), "Until Autosave");
            }
            
            ImGui::Separator();

            // --- MANUAL SAVING (CUSTOM NAME) ---
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1), "MANUAL SNAPSHOT");
            ImGui::InputText("Snapshot Name", manualSaveName, sizeof(manualSaveName));
            
            if (ImGui::Button("Save Snapshot (Timestamped)")) {
                // Combine your manual name with a unique timestamp
                std::string uniqueName = manualSaveName + world.getTimestamp();
                world.saveWorldState(uniqueName, false);
            }
            ImGui::Text("Saves to /saves/worlds/ with your custom name + timestamp.");

            ImGui::Separator();

            // --- RESTORE & IMPORT ---
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "LOAD / IMPORT");
            
            // World Loading
            ImGui::InputText("World Path", loadWorldPath, sizeof(loadWorldPath));
            if (ImGui::Button("Load Full World")) {
                world.loadWorldState(loadWorldPath);
            }

            ImGui::Spacing();

            // Organism Importing
            ImGui::InputText("Creature DNA Path", importOrgPath, sizeof(importOrgPath));
            if (ImGui::Button("Import Creature to Center")) {
                float cx = world.config.worldWidth / 2.0f;
                float cy = world.config.worldHeight / 2.0f;
                world.importOrganism(importOrgPath, cx, cy);
            }

            ImGui::Separator();

            // --- SELECTED ORGANISM EXPORT ---
            if (world.selectedOrgId != -1) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Exporting Creature #%d", world.selectedOrgId);
                if (ImGui::Button("Export DNA to /saves/creatures/")) {
                    world.exportOrganism(world.selectedOrgId);
                }
            } else {
                ImGui::TextDisabled("Select a creature in the viewport to export.");
            }
        }
        ImGui::End();
    }
};