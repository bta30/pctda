cmake_minimum_required(VERSION 3.0)
project(jsontracer)

set(DynamoRIO_DIR "/home/ben/internship/DynamoRIO-Linux-9.93.19537/cmake")

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "Unable to find DynamoRIO - please set DynamoRIO_DIR")
endif (NOT DynamoRIO_FOUND)

add_library(jsontracer SHARED tracer.c insertInstructions.c jsonout.c)
configure_DynamoRIO_client(jsontracer)
use_DynamoRIO_extension(jsontracer "drmgr")
use_DynamoRIO_extension(jsontracer "drreg")
use_DynamoRIO_extension(jsontracer "drx")