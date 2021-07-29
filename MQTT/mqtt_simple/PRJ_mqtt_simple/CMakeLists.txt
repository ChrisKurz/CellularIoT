cmake_minimum_required(VERSION 3.13.1)

# Find external Zephyr project, and load its settings:
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Set project name:
project(mqtt-simple)

# add sources
target_sources(app PRIVATE
                  src/main.c)
