cmake_minimum_required(VERSION 3.18) # Bumped to 3.18 to support SOURCE_SUBDIR
project(ALifeGarden)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TBB REQUIRED) 

include(FetchContent)

# --- Fetch Jolt Physics ---
FetchContent_Declare(
    jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.0.0     # <--- Change this from 'master' to 'v5.0.0'
    SOURCE_SUBDIR Build
)
# We only need the core library
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jolt)

# --- Fetch Dear ImGui ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
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

# Compile everything together
add_executable(garden 
    main.cpp 
    ${IMGUI_SOURCES}
)

target_include_directories(garden PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
    ${jolt_SOURCE_DIR}
)

# Link Jolt, ImGui, GLFW, OpenGL, and TBB
target_link_libraries(garden PRIVATE OpenGL::GL glfw Jolt TBB::tbb)