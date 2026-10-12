cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SEED_BUILD_DIR OR NOT DEFINED SEED_EXPORT_DIR)
    message(FATAL_ERROR
        "Usage: cmake -DSEED_BUILD_DIR=<build> -DSEED_EXPORT_DIR=<dist> "
        "[-DSEED_PROJECT_DIR=<project>] [-DSEED_CONFIG=Release] "
        "-P tools/export_project.cmake"
    )
endif()

if(NOT EXISTS "${SEED_BUILD_DIR}/cmake_install.cmake")
    message(FATAL_ERROR "Build directory is not configured: ${SEED_BUILD_DIR}")
endif()

if(NOT DEFINED SEED_CONFIG)
    set(SEED_CONFIG Release)
endif()

file(REMOVE_RECURSE "${SEED_EXPORT_DIR}")
file(MAKE_DIRECTORY "${SEED_EXPORT_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${SEED_BUILD_DIR}"
        --prefix "${SEED_EXPORT_DIR}"
        --config "${SEED_CONFIG}"
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Seed binary installation failed with code ${install_result}")
endif()

if(DEFINED SEED_PROJECT_DIR AND NOT SEED_PROJECT_DIR STREQUAL "")
    if(NOT IS_DIRECTORY "${SEED_PROJECT_DIR}")
        message(FATAL_ERROR "Seed project directory does not exist: ${SEED_PROJECT_DIR}")
    endif()
    file(COPY "${SEED_PROJECT_DIR}" DESTINATION "${SEED_EXPORT_DIR}/project")
endif()

message(STATUS "Seed export created at ${SEED_EXPORT_DIR}")
