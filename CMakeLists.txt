# CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(ALifeGarden)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch GLFW from source
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch ImGui from source
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

add_executable(garden main.cpp ${IMGUI_SOURCES})

target_include_directories(garden PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

# Cross-platform Linking
# Pthreads is required for std::thread on Linux/MinGW
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if(WIN32)
    # Windows native
    target_link_libraries(garden PRIVATE opengl32 glfw Threads::Threads)
    
    # Statically link MinGW standard libraries
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(garden PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
else()
    # Linux native
    find_package(OpenGL REQUIRED) 
    target_link_libraries(garden PRIVATE OpenGL::GL glfw Threads::Threads)
endif()