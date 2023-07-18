cmake_minimum_required(VERSION 3.0)
project(jsontracer)

set(DynamoRIO_DIR "/home/ben/internship/DynamoRIO-Linux-9.93.19537/cmake")

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "Unable to find DynamoRIO - please set DynamoRIO_DIR")
endif (NOT DynamoRIO_FOUND)

find_library(DWARF_PATH dwarf)
if (NOT DEFINED DWARF_PATH)
    message(FATAL_ERROR "Unable to find path to libdwarf")
endif(NOT DEFINED DWARF_PATH)
message(${DWARF_PATH})

add_library(debuginfo SHARED debug_info.c)
target_include_directories(debuginfo PRIVATE
                           /usr/include/libdwarf/libdwarf-0)
target_link_libraries(debuginfo "${DWARF_PATH}")

add_library(jsontracer SHARED tracer.c insert_instrumentation.c json_writer.c
                              debug_info_client.c)
configure_DynamoRIO_client(jsontracer)
use_DynamoRIO_extension(jsontracer "drmgr")
use_DynamoRIO_extension(jsontracer "drreg")
use_DynamoRIO_extension(jsontracer "drx")
use_DynamoRIO_extension(jsontracer "drsyms")
