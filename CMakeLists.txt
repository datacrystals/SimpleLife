cmake_minimum_required(VERSION 3.18)
project(ALifeGarden)

set(CMAKE_CXX_STANDARD 17)

# --- Dependencies ---
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TBB REQUIRED) 
find_package(GLEW REQUIRED)

include(FetchContent)

# --- Fetch ImGui (Stable Docking Tag) ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1-docking 
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# --- Fetch ImPlot (Stable Release Tag) ---
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
)
FetchContent_MakeAvailable(implot)

set(IMPLOT_SOURCES
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

# --- Fetch nlohmann/json ---
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Executable ---
add_executable(garden 
    main.cpp 
    PhysicsEngine.cpp 
    Brain.cpp 
    UI/SimulationUI.cpp 
    ${IMGUI_SOURCES} 
    ${IMPLOT_SOURCES}
)

# --- Compiler Definitions ---
# Force ImGui/ImPlot to use 32-bit indices to prevent vertex truncation crashes
target_compile_definitions(garden PRIVATE "ImDrawIdx=unsigned int")

target_include_directories(garden PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
)

target_link_libraries(garden PRIVATE 
    OpenGL::GL 
    glfw 
    TBB::tbb 
    GLEW::GLEW
    nlohmann_json::nlohmann_json
)