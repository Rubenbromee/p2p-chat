# Minimum CMake version requirement
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(P2PChat VERSION 1.0)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add the executable
add_executable(p2p_chat main.c)
