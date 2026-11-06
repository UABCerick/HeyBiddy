# Hey Biddy — firmware del ESP32-S3 (un solo firmware con todas las apps)
cmake_minimum_required(VERSION 3.16)

# La version del firmware completo vive en version.txt
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.txt" PROJECT_VER LIMIT_COUNT 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hey_biddy)
