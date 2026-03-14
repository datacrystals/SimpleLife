/**
 * @file BrainMonitorWindow.h
 * @brief Spatial SNN Inspector with Zoom, Pan, IO Labeling, and ImPlot Telemetry.
 */
#pragma once
#include "UIWindow.h"
#include "imgui.h"
#include "implot.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <deque>

// Simple scrolling buffer for ImPlot line graphs
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 500) {
        MaxSize = max_size;
        Offset  = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x,y));
        else {
            Data[Offset] = ImVec2(x,y);
            Offset =  (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset  = 0;
        }
    }
};

class BrainMonitorWindow : public UIWindow {
private:
    int selectedNeuronIndex = -1;
    int selectedOrgIndex = 0;
    int lastObservedOrgId = -1;
    
    // Neural Canvas State
    ImVec2 brainOffset = ImVec2(0.5f, 0.5f);
    float brainZoom = 0.8f;                  

    // ImPlot Telemetry Buffers
    float historyWindow = 10.0f; // Show last 10 seconds of simulation
    ScrollingBuffer popVoltBuffer;
    ScrollingBuffer selectedVoltBuffer;
    
    // Scatter Plot data for Raster (X = time, Y = Neuron Index)
    std::vector<ImVec2> spikeScatterData;

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
        if (neuronIdx == 0) return "Energy-Sense";
        if (neuronIdx == 1) return "Density-Sense";
        return "Hidden/Internal";
    }

    void DrawSpatialTopology(ImDrawList* drawList, Organism& org, ImVec2 canvasPos, ImVec2 canvasSize) {
        auto& brain = org.brain;
        if (brain.neurons.empty()) return;

        if (brainZoom < 0.01f) brainZoom = 0.01f;

        auto ToCanvas = [&](float x, float y) {
            float viewX = (x - 0.5f) * brainZoom + 0.5f + (brainOffset.x - 0.5f);
            float viewY = (y - 0.5f) * brainZoom + 0.5f + (brainOffset.y - 0.5f);
            return ImVec2(canvasPos.x + viewX * canvasSize.x, canvasPos.y + viewY * canvasSize.y);
        };

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

        for (float t = -2.0f; t <= 3.0f; t += 0.1f) {
            ImVec2 pStartV = ToCanvas(t, -2.0f); ImVec2 pEndV = ToCanvas(t, 3.0f);
            ImVec2 pStartH = ToCanvas(-2.0f, t); ImVec2 pEndH = ToCanvas(3.0f, t);
            ImU32 gridCol = IM_COL32(40, 40, 45, 255);
            drawList->AddLine(pStartV, pEndV, gridCol, 1.0f);
            drawList->AddLine(pStartH, pEndH, gridCol, 1.0f);
        }

        float minX = 999.0f, maxX = -999.0f, minY = 999.0f, maxY = -999.0f;
        for (const auto& n : brain.neurons) {
            minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
            minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
        }
        if (minX > maxX) { minX = 0; maxX = 1; minY = 0; maxY = 1; }

        float marginX = std::max(0.05f, (maxX - minX) * 0.05f); 
        float marginY = std::max(0.05f, (maxY - minY) * 0.05f);

        ImVec2 tl = ToCanvas(minX - marginX, minY - marginY);
        ImVec2 br = ToCanvas(maxX + marginX, maxY + marginY);
        drawList->AddRect(tl, br, IM_COL32(0, 150, 255, 180), 0, 0, 1.5f);

        for (const auto& syn : brain.synapses) {
            if (syn.source_idx >= 0 && syn.source_idx < (int)brain.neurons.size() &&
                syn.target_idx >= 0 && syn.target_idx < (int)brain.neurons.size()) {
                
                auto& sourceNode = brain.neurons[syn.source_idx];
                auto& targetNode = brain.neurons[syn.target_idx];
                
                ImVec2 p1 = ToCanvas(sourceNode.x, sourceNode.y);
                ImVec2 p2 = ToCanvas(targetNode.x, targetNode.y);
                
                float thickness = std::max(1.0f, syn.weight * 1.5f * std::sqrt(brainZoom));
                int alpha = std::min(255, std::max(30, static_cast<int>(syn.weight * 70)));
                
                ImU32 lineCol = (sourceNode.polarity == NeuronPolarity::EXCITATORY) ? 
                                IM_COL32(255, 255, 0, alpha) : 
                                IM_COL32(0, 150, 255, alpha); 
                
                drawList->AddLine(p1, p2, lineCol, thickness);
            }
        }

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
                    if (selectedNeuronIndex != i) {
                        selectedNeuronIndex = i;
                        selectedVoltBuffer.Erase(); // Clear history for clean graph
                    }
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

        if (world.selectedOrgId != -1) {
            for (int i = 0; i < (int)world.population.size(); i++) {
                if (world.population[i]->id == world.selectedOrgId) {
                    selectedOrgIndex = i;
                    break;
                }
            }
        }

        if (selectedOrgIndex >= (int)world.population.size()) selectedOrgIndex = 0;
        auto& org = world.population[selectedOrgIndex];

        // --- Handle Organism Switching ---
        if (org->id != lastObservedOrgId) {
            lastObservedOrgId = org->id;
            selectedNeuronIndex = -1;
            popVoltBuffer.Erase();
            selectedVoltBuffer.Erase();
            spikeScatterData.clear();
        }

        // --- Update Telemetry Data using Actual World Time ---
        float currentAvgVolt = 0;
        size_t numNeurons = org->brain.neurons.size();
        
        for (size_t i = 0; i < numNeurons; i++) {
            auto& n = org->brain.neurons[i];
            currentAvgVolt += n.membranePotential;
            
            if (n.spikedThisTick) {
                // Record the spike event for the raster scatter plot
                spikeScatterData.push_back(ImVec2(world.worldTime, (float)i));
            }
        }

        // Cleanup old raster scatter points outside the rolling window
        while (!spikeScatterData.empty() && spikeScatterData.front().x < world.worldTime - historyWindow) {
            spikeScatterData.erase(spikeScatterData.begin());
        }

        popVoltBuffer.AddPoint(world.worldTime, currentAvgVolt / std::max(1.0f, (float)numNeurons));

        if (selectedNeuronIndex != -1 && selectedNeuronIndex < (int)numNeurons) {
            auto& sn = org->brain.neurons[selectedNeuronIndex];
            
            // THE FIX: If it spiked this tick, override the 0.0f reading with a visual peak 
            // so the line graph actually shows the spike jumping above the threshold.
            float displayVolt = sn.spikedThisTick ? (sn.threshold * 1.2f) : sn.membranePotential;
            selectedVoltBuffer.AddPoint(world.worldTime, displayVolt);
        }

        // --- UI Layout ---
        ImGui::Text("Inspecting Org ID: %d", org->id);
        ImGui::SameLine();
        if (ImGui::Button("Recenter View")) { brainOffset = ImVec2(0.5f, 0.5f); brainZoom = 1.0f; }

        ImGui::Columns(2, "MonitorSplit", true);

        // --- Left Column: Spatial Network ---
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.y = std::max(canvasSize.y, 500.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(25, 25, 30, 255));
        
        DrawSpatialTopology(dl, *org, canvasPos, canvasSize);
        ImGui::Dummy(canvasSize);

        ImGui::NextColumn();

        // --- Right Column: ImPlot Graphs ---
        
        // 1. Population Average Plot
        // The ResizeY flag adds a draggable handle at the bottom!
        ImGui::BeginChild("PopPlotChild", ImVec2(0, 150), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);
        if (ImPlot::BeginPlot("Population Avg Potential", ImVec2(-1, -1))) { // -1, -1 tells ImPlot to fill the child
            ImPlot::SetupAxes("Time (s)", "Voltage");
            ImPlot::SetupAxisLimits(ImAxis_X1, world.worldTime - historyWindow, world.worldTime, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.2, ImGuiCond_Once); 
            
            if (popVoltBuffer.Data.size() > 0) {
                ImPlot::PlotLine("Avg Membrane", &popVoltBuffer.Data[0].x, &popVoltBuffer.Data[0].y, 
                                 popVoltBuffer.Data.size(), 0, popVoltBuffer.Offset, sizeof(ImVec2));
            }
            ImPlot::EndPlot();
        }
        ImGui::EndChild();
        
        // 2. Selected Neuron Plot
        if (selectedNeuronIndex != -1 && selectedNeuronIndex < (int)numNeurons) {
            auto& sn = org->brain.neurons[selectedNeuronIndex];
            ImGui::Text("Selected: %s (ID: %d)", GetInputSourceName(*org, selectedNeuronIndex).c_str(), sn.id);
            
            ImGui::BeginChild("SelPlotChild", ImVec2(0, 150), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);
            if (ImPlot::BeginPlot("Selected Neuron", ImVec2(-1, -1))) {
                ImPlot::SetupAxes("Time (s)", "Voltage");
                ImPlot::SetupAxisLimits(ImAxis_X1, world.worldTime - historyWindow, world.worldTime, ImGuiCond_Always);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -0.2, sn.threshold * 1.5, ImGuiCond_Once); 
                
                if (selectedVoltBuffer.Data.size() > 0) {
                    ImPlot::PlotLine("Membrane", &selectedVoltBuffer.Data[0].x, &selectedVoltBuffer.Data[0].y, 
                                     selectedVoltBuffer.Data.size(), 0, selectedVoltBuffer.Offset, sizeof(ImVec2));
                }
                
                double thresh = sn.threshold;
                ImPlot::SetNextLineStyle(ImVec4(1, 0, 0, 0.5f), 1.0f); 
                ImPlot::PlotInfLines("Threshold", &thresh, 1, ImPlotInfLinesFlags_Horizontal);

                ImPlot::EndPlot();
            }
            ImGui::EndChild();
        } else {
            ImGui::Dummy(ImVec2(0, 50));
            ImGui::TextDisabled("Select a neuron to view potential.");
        }

        // 3. Spike Raster Plot
        // We pass ImVec2(0, 0) here so the last graph simply auto-fills the remaining vertical space 
        // left over by whatever you resized the top two graphs to.
        ImGui::BeginChild("RasterPlotChild", ImVec2(0, 0), ImGuiChildFlags_Border); 
        if (ImPlot::BeginPlot("Spike Raster", ImVec2(-1, -1))) {
            ImPlot::SetupAxes("Time (s)", "Neuron Index");
            ImPlot::SetupAxisLimits(ImAxis_X1, world.worldTime - historyWindow, world.worldTime, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1, numNeurons, ImGuiCond_Always);
            
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 2.0f, ImVec4(1.0f, 1.0f, 0.0f, 1.0f), IMPLOT_AUTO, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            
            if (!spikeScatterData.empty()) {
                ImPlot::PlotScatter("Spikes", &spikeScatterData[0].x, &spikeScatterData[0].y, 
                                    spikeScatterData.size(), 0, 0, sizeof(ImVec2));
            }
            ImPlot::EndPlot();
        }
        ImGui::EndChild();

        ImGui::Columns(1);
        ImGui::End();
    }
};