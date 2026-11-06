
cmake_minimum_required(VERSION 3.20)

project(Playground LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor")


add_subdirectory("${VENDOR_ROOT}/SDL3" EXCLUDE_FROM_ALL)

add_library(imgui STATIC
    "${VENDOR_ROOT}/imgui/imgui.cpp"
    "${VENDOR_ROOT}/imgui/imgui_draw.cpp"
    "${VENDOR_ROOT}/imgui/imgui_widgets.cpp"
    "${VENDOR_ROOT}/imgui/imgui_tables.cpp"
    "${VENDOR_ROOT}/imgui/imgui_demo.cpp"
    "${VENDOR_ROOT}/imgui/backends/imgui_impl_sdl3.cpp"
    "${VENDOR_ROOT}/imgui/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC
    "${VENDOR_ROOT}/imgui"
    "${VENDOR_ROOT}/imgui/backends"
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)

target_include_directories(${PROJECT_NAME} PRIVATE "${VENDOR_ROOT}/nlohmann")

find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
