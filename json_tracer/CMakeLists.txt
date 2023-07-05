cmake_minimum_required(VERSION 3.0)
project(jsontracer)

set(DynamoRIO_DIR "/home/ben/internship/DynamoRIO-Linux-9.93.19537/cmake")

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "Unable to find DynamoRIO - please set DynamoRIO_DIR")
endif (NOT DynamoRIO_FOUND)

add_library(jsontracer SHARED jsontracer.c utils.c instructionbuffer.c
        insertInstructions.c)
configure_DynamoRIO_client(jsontracer)
use_DynamoRIO_extension(jsontracer "drmgr")
use_DynamoRIO_extension(jsontracer "drx")

add_library(instrace_mod SHARED instrace_mod.c utils.c)
configure_DynamoRIO_client(instrace_mod)
use_DynamoRIO_extension(instrace_mod "drmgr")
use_DynamoRIO_extension(instrace_mod "drreg")
use_DynamoRIO_extension(instrace_mod "drx")
