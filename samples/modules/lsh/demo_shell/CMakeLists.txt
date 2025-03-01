# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(demo_shell)

set(LSH_DIR ${ZEPHYR_LSH_MODULE_DIR})
target_sources(
    app PRIVATE src/main.c
)

target_include_directories(app PRIVATE
    ${LSH_DIR}/shell/
)

target_sources_ifdef(app PRIVATE
    ${LSH_DIR}/shell/shell.c
    ${LSH_DIR}/shell/shell_commands.c
)