#pragma once
#include "UIWindow.h"
#include "imgui.h"
#include <GL/glew.h>
#include <cmath>
#include <vector>

class ViewportWindow : public UIWindow {
private:
    GLuint fbo = 0;
    GLuint texture = 0;
    int fboWidth = 0, fboHeight = 0;

    float camX = 100.0f, camY = 70.0f, camZoom = 100.0f;

    void resizeFBO(int width, int height) {
        if (width <= 0 || height <= 0) return;
        if (width == fboWidth && height == fboHeight) return;

        if (fbo != 0) {
            glDeleteFramebuffers(1, &fbo);
            glDeleteTextures(1, &texture);
        }

        fboWidth = width;
        fboHeight = height;

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void drawJoint(float x, float y, float size, bool isFlexible) {
        if (isFlexible) {
            glColor3f(0.2f, 0.4f, 1.0f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x, y);
            for (int i = 0; i <= 12; i++) {
                float angle = i * 3.14159f * 2.0f / 12.0f;
                glVertex2f(x + cos(angle) * size, y + sin(angle) * size);
            }
            glEnd();
        } else {
            glColor3f(0.6f, 0.2f, 0.8f);
            glBegin(GL_QUADS);
            glVertex2f(x - size, y - size);
            glVertex2f(x + size, y - size);
            glVertex2f(x + size, y + size);
            glVertex2f(x - size, y + size);
            glEnd();
        }
    }

    void renderWorldGL(World& world) {
        float ww = world.getConfig().worldWidth;
        float wh = world.getConfig().worldHeight;

        for (const auto& org : world.population) {
            if (org->points.empty()) continue;

            std::vector<bool> flexibleJoints(org->points.size(), false);
            for (size_t i = 0; i < org->springs.size(); ++i) {
                if (org->bodyParts[i].isMuscle) {
                    flexibleJoints[org->springs[i].p1_idx] = true;
                    flexibleJoints[org->springs[i].p2_idx] = true;
                }
            }

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
                    float tension = bp.currentTension; 
                    glColor3f(0.4f + (tension * 0.6f), 0.05f + (tension * 0.15f), 0.1f + (tension * 0.1f));
                } else if (bp.type == ColorType::DEAD || bp.type == ColorType::WHITE) {
                    glColor3f(0.9f, 0.9f, 0.9f); 
                } else if (bp.type == ColorType::GREEN) {
                    glColor3f(0.2f, 0.8f, 0.2f); 
                } else if (bp.type == ColorType::RED) {
                    glColor3f(0.9f, 0.1f, 0.1f); 
                }

                glVertex2f(p1.x, p1.y);
                glVertex2f(p2.x, p2.y);

                if (bp.sensorRange > 0.0f && org->isAlive) {
                    glColor4f(1.0f, 1.0f, 0.0f, 0.3f); 
                    float dx = p2.x - p1.x; float dy = p2.y - p1.y;
                    float len = std::sqrt(dx*dx + dy*dy);
                    if (len > 0.001f) {
                        glVertex2f(p2.x, p2.y);
                        glVertex2f(p2.x + (dx/len) * bp.sensorRange, p2.y + (dy/len) * bp.sensorRange);
                    }
                }
            }
            glEnd();

            if (org->isAlive) {
                for (size_t i = 0; i < org->points.size(); ++i) {
                    drawJoint(org->points[i].x, org->points[i].y, 0.3f, flexibleJoints[i]);
                }
            }
        }
    }

public:
    std::string GetName() const override { return "Simulation Viewport"; }

    void Draw(World& world) override {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (!ImGui::Begin(GetName().c_str(), &isOpen)) {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        resizeFBO((int)viewportSize.x, (int)viewportSize.y);

        if (fbo != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0, 0, fboWidth, fboHeight);
            glClearColor(0.05f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            float aspect = (float)fboWidth / (float)fboHeight;
            glMatrixMode(GL_PROJECTION); 
            glLoadIdentity();
            glOrtho(camX - camZoom * aspect, camX + camZoom * aspect, camY - camZoom, camY + camZoom, -1.0, 1.0);
            glMatrixMode(GL_MODELVIEW);

            renderWorldGL(world);
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            // Render the FBO texture into ImGui
            ImGui::Image((void*)(intptr_t)texture, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        // Camera Input (Only if mouse is hovering over the viewport)
        if (ImGui::IsWindowHovered()) {
            if (ImGui::IsKeyDown(ImGuiKey_W)) camY += camZoom * 0.03f;
            if (ImGui::IsKeyDown(ImGuiKey_S)) camY -= camZoom * 0.03f;
            if (ImGui::IsKeyDown(ImGuiKey_A)) camX -= camZoom * 0.03f;
            if (ImGui::IsKeyDown(ImGuiKey_D)) camX += camZoom * 0.03f;
            if (ImGui::IsKeyDown(ImGuiKey_E)) camZoom *= 1.02f;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) camZoom *= 0.98f;
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
};