cmake_minimum_required(VERSION 3.16)

project(PrintEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The WIN32 flag tells CMake to build a Windows GUI application
add_executable(PrintEngine WIN32 main.cpp)

# We link directly to the Windows Operating System's built-in DLLs!
target_link_libraries(PrintEngine PRIVATE opengl32 glu32 user32 gdi32)
