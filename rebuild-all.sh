#!/bin/bash



#cmake -S . -B build-sdl-rmlui -DBZ3_WINDOW_BACKEND=sdl -DBZ3_UI_BACKEND=rmlui
cmake --build build-sdl-rmlui -j

#cmake -S . -B build-sdl-imgui -DBZ3_WINDOW_BACKEND=sdl -DBZ3_UI_BACKEND=imgui
cmake --build build-sdl-imgui -j

#cmake -S . -B build-glfw-rmlui -DBZ3_WINDOW_BACKEND=glfw -DBZ3_UI_BACKEND=rmlui
cmake --build build-glfw-rmlui -j

#cmake -S . -B build-glfw-imgui -DBZ3_WINDOW_BACKEND=glfw -DBZ3_UI_BACKEND=imgui
cmake --build build-glfw-imgui -j

