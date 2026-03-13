#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "World.h"

std::mt19937 rng(42);
std::uniform_real_distribution<float> randFloat(0.0f, 1.0f);

float camX = 0.0f, camY = 0.0f, camZoom = 60.0f, aspect = 1.0f;

void renderWorld(World& world) {
    JPH::BodyInterface& bi = world.jolt.physicsSystem->GetBodyInterface();
    for (const auto* org : world.population) {
        for (const auto* seg : org->segments) {
            JPH::BodyID bID(seg->joltBodyID);
            if (!bi.IsAdded(bID)) continue;

            JPH::RVec3 pos = bi.GetPosition(bID);
            JPH::Quat rot = bi.GetRotation(bID);
            JPH::Vec3 euler = rot.GetEulerAngles(); // Get Z rotation

            glPushMatrix();
            glTranslatef(pos.GetX(), pos.GetY(), 0.0f);
            glRotatef(euler.GetZ() * 180.0f / 3.14159f, 0.0f, 0.0f, 1.0f);

            switch(seg->type) {
                case ColorType::GREEN:  glColor3f(0.1f, 0.8f, 0.1f); break;
                case ColorType::RED:    glColor3f(0.9f, 0.1f, 0.1f); break;
                case ColorType::PURPLE: glColor3f(0.6f, 0.1f, 0.8f); break;
                case ColorType::BLUE:   glColor3f(0.1f, 0.3f, 0.9f); break;
                case ColorType::YELLOW: glColor3f(0.9f, 0.8f, 0.1f); break;
                case ColorType::WHITE:  glColor3f(0.9f, 0.9f, 0.9f); break;
                case ColorType::DEAD:   glColor3f(0.3f, 0.3f, 0.3f); break;
            }

            float hw = seg->width / 2.0f; float hh = seg->height / 2.0f;
            glBegin(GL_QUADS);
                glVertex2f(-hw, -hh); glVertex2f( hw, -hh);
                glVertex2f( hw,  hh); glVertex2f(-hw,  hh);
            glEnd();
            glPopMatrix();
        }
    }
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(1200, 900, "Multithreaded Jolt ALife", NULL, NULL);
    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    World world;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int w, h; glfwGetFramebufferSize(window, &w, &h);
        aspect = (float)w / (float)h;
        glViewport(0, 0, w, h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Jolt Sandbox Core Monitor");
        ImGui::Text("Population: %zu / %d", world.population.size(), world.maxPopulation);
        ImGui::SliderFloat("Time Scale", &world.timeScale, 0.0f, 5.0f);
        ImGui::SliderInt("Max Population", &world.maxPopulation, 10, 100000); 
        ImGui::Text("Active Cores: %d", std::thread::hardware_concurrency());
        
        ImGui::Separator();
        ImGui::Text("Evolution Pressures");
        ImGui::SliderFloat("Mutation Rate", &world.mutationRate, 0.01f, 1.0f);
        ImGui::SliderFloat("Base Metabolism", &world.baseMetabolism, 0.01f, 0.5f, "%.3f");
        ImGui::SliderFloat("Movement Cost", &world.movementCost, 0.0001f, 0.01f, "%.4f");
        
        ImGui::Separator();
        ImGui::Text("Physical Forces");
        ImGui::SliderFloat("Thrust Power", &world.thrustMultiplier, 10.0f, 200.0f);
        ImGui::SliderFloat("Turn Power", &world.turnMultiplier, 5.0f, 100.0f);



        ImGui::End();

        world.updateTick();

        glClearColor(0.05f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(camX - camZoom * aspect, camX + camZoom * aspect, camY - camZoom, camY + camZoom, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);

        renderWorld(world);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    return 0;
}