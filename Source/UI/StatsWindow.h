#pragma once
#include "UIWindow.h"
#include "imgui.h"
#include <iomanip>
#include <sstream>

class StatsWindow : public UIWindow {
public:
    std::string GetName() const override { return "Organism Inspector"; }

    void Draw(World& world) override {
        if (!ImGui::Begin(GetName().c_str(), &isOpen)) {
            ImGui::End();
            return;
        }

        if (world.selectedOrgId == -1) {
            ImGui::TextDisabled("No organism selected.");
            ImGui::TextWrapped("Click an organism in the viewport to inspect its vitals.");
            ImGui::End();
            return;
        }

        // Find the selected organism
        Organism* selected = nullptr;
        for (const auto& org : world.population) {
            if (org->id == world.selectedOrgId) {
                selected = org.get();
                break;
            }
        }

        if (!selected) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected organism no longer exists.");
            ImGui::End();
            return;
        }

        // --- Vital Signs ---
        ImGui::SeparatorText("Vitals");
        
        // State
        if (selected->isAlive) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: ALIVE");
        } else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Status: DEAD");
        }

        // Energy (Health)
        float energy = selected->energy; //
        ImGui::Text("Energy:"); ImGui::SameLine();
        if (energy > 100.0f) ImGui::TextColored(ImVec4(0.5, 1, 0.5, 1), "%.2f", energy);
        else if (energy > 20.0f) ImGui::Text("%.2f", energy);
        else ImGui::TextColored(ImVec4(1, 0.5, 0.5, 1), "%.2f (Starving)", energy);
        
        // Simple Health Bar
        float energyPct = energy / world.config.reproductionEnergyThreshold;
        ImGui::ProgressBar(energyPct, ImVec2(-1, 0), "Reproduction Readiness");

        // Age
        ImGui::Text("Age: %.2f seconds", selected->age);

        // --- Physical Specs ---
        ImGui::SeparatorText("Morphology");
        ImGui::BulletText("Segments: %zu", selected->bodyParts.size());
        ImGui::BulletText("Physics Nodes: %zu", selected->points.size());
        
        int muscles = 0, plants = 0, weapons = 0;
        for(const auto& bp : selected->bodyParts) {
            if (bp.isMuscle) muscles++;
            if (bp.type == ColorType::GREEN) plants++;
            if (bp.type == ColorType::RED) weapons++;
        }
        ImGui::Indent();
        ImGui::Text("- Muscles: %d", muscles);
        ImGui::Text("- Photo-cells: %d", plants);
        ImGui::Text("- Weaponized: %d", weapons);
        ImGui::Unindent();

        // --- Reproductive Status ---
        ImGui::SeparatorText("Evolution");
        float cooldown = selected->reproCooldown; //
        if (cooldown > 0) {
            ImGui::Text("Repro Cooldown: %.2fs", cooldown);
        } else {
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Ready to reproduce!");
        }

        // --- Actions ---
        ImGui::Separator();
        if (ImGui::Button("Kill", ImVec2(80, 0))) selected->isAlive = false;
        ImGui::SameLine();
        if (ImGui::Button("Boost Energy", ImVec2(100, 0))) selected->energy += 50.0f;

        ImGui::End();
    }
};