cmake_minimum_required(VERSION 3.10)
project(GBA_Emulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(GBA_Emulator ARM7TDMI.cpp)
target_include_directories(GBA_Emulator PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(GBA_Emulator PRIVATE ${SDL2_LIBRARIES})
