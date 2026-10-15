cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED ${SEED_BUILD_DIR OR NOT DEFINED ${SEED_EXPORT_DIR)
    message(FATAL_ERROR
        "Usage: cmake -DSEED_BUILD_DIR=<build> -DSEED_EXPORT_DIR=<dist> "
        "[-DSEED_PROJECT_DIR=<project>] [-DSEED_CONFIG=Release] "
        "-P tools/export_project.cmake"
    )
endif()

if(NOT EXISTS "${SEED_BUILD_DIR/cmake_install.cmake")
    message(FATAL_ERROR "Build directory is not configured: ${SEED_BUILD_DIR")
endif()

if(NOT DEFINED SEED_CONFIG)
    set(SEED_CONFIG Release)
endif()

set(validation_errors)
set(validation_warnings)

if(DEFINED SEED_PROJECT_DIR AND NOT SEED_PROJECT_DIR STREQUAL "")
    if(NOT IS_DIRECTORY "${SEED_PROJECT_DIR")
        list(APPEND validation_errors "Project directory does not exist: ${SEED_PROJECT_DIR")
    else()
        file(GLOB_RECURSE seed_project_files
            LIST_DIRECTORIES false
            "${SEED_PROJECT_DIR/*.seedproject"
            "${SEED_PROJECT_DIR/*.seedproj"
        )
        list(LENGTH seed_project_files project_file_count)
        if(project_file_count EQUAL 0)
            list(APPEND validation_errors
                "No .seedproject or .seedproj descriptor was found in ${SEED_PROJECT_DIR"
            )
        else()
            if(project_file_count GREATER 1)
                list(APPEND validation_warnings
                    "Multiple project descriptors found; using the first one: ${seed_project_files"
                )
            endif()

            list(GET seed_project_files 0 seed_project_file)
            file(READ "${seed_project_file" project_json)

            string(JSON project_format ERROR_VARIABLE project_format_error
                GET "${project_json" seed_format
            )
            if(project_format_error)
                list(APPEND validation_errors
                    "Project descriptor is not valid Seed JSON: ${seed_project_file"
                )
            elseif(NOT project_format STREQUAL "seedproject")
                list(APPEND validation_errors
                    "Project descriptor has an unsupported seed_format: ${project_format"
                )
            endif()

            string(JSON startup_scene ERROR_VARIABLE startup_scene_error
                GET "${project_json" startup_scene
            )
            if(startup_scene_error OR startup_scene STREQUAL "")
                list(APPEND validation_errors
                    "Project descriptor does not define a startup_scene: ${seed_project_file"
                )
            else()
                set(startup_scene_file "${SEED_PROJECT_DIR/${startup_scene")
                if(NOT EXISTS "${startup_scene_file")
                    list(APPEND validation_errors
                        "Startup scene is missing: ${startup_scene"
                    )
                elseif(NOT IS_FILE "${startup_scene_file")
                    list(APPEND validation_errors
                        "Startup scene path is not a file: ${startup_scene"
                    )
                endif()
            endif()

            string(JSON project_name ERROR_VARIABLE project_name_error
                GET "${project_json" name
            )
            if(project_name_error OR project_name STREQUAL "")
                list(APPEND validation_warnings
                    "Project has no display name; Seed Runtime will use a fallback name"
                )
            endif()
        endif()

        if(NOT EXISTS "${SEED_PROJECT_DIR/Scenes")
            list(APPEND validation_warnings
                "Project has no Scenes directory; future scene switching may have no content"
            )
        endif()
        if(NOT EXISTS "${SEED_PROJECT_DIR/Assets")
            list(APPEND validation_warnings
                "Project has no Assets directory; the export will contain no project-owned assets"
            )
        endif()
    endif()
else()
    list(APPEND validation_warnings
        "No SEED_PROJECT_DIR was supplied; exporting engine binaries only"
    )
endif()

if(validation_errors)
    string(REPLACE ";" "\\n - " validation_error_text "${validation_errors")
    message(FATAL_ERROR
        "Seed export validation failed:\\n - ${validation_error_text\\n"
        "Fix the project issues before packaging."
    )
endif()

file(REMOVE_RECURSE "${SEED_EXPORT_DIR")
file(MAKE_DIRECTORY "${SEED_EXPORT_DIR")

set(validation_report "Seed Export Validation\\n")
string(APPEND validation_report "=======================\\n")
if(DEFINED SEED_PROJECT_DIR AND NOT SEED_PROJECT_DIR STREQUAL "")
    string(APPEND validation_report "Project: ${SEED_PROJECT_DIR\\n")
else()
    string(APPEND validation_report "Project: engine binaries only\\n")
endif()
if(validation_warnings)
    string(APPEND validation_report "\\nWarnings:\\n")
    foreach(warning IN LISTS validation_warnings)
        string(APPEND validation_report " - ${warning\\n")
    endforeach()
else()
    string(APPEND validation_report "\\nWarnings: none\\n")
endif()
string(APPEND validation_report "\\nResult: PASS\\n")
file(WRITE "${SEED_EXPORT_DIR/SeedExportValidation.txt" "${validation_report")

execute_process(
    COMMAND "${CMAKE_COMMAND" --install "${SEED_BUILD_DIR"
        --prefix "${SEED_EXPORT_DIR"
        --config "${SEED_CONFIG"
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Seed binary installation failed with code ${install_result")
endif()

if(DEFINED SEED_PROJECT_DIR AND NOT SEED_PROJECT_DIR STREQUAL "")
    file(COPY "${SEED_PROJECT_DIR" DESTINATION "${SEED_EXPORT_DIR/project")
endif()

message(STATUS "Seed export validation passed")
message(STATUS "Seed export created at ${SEED_EXPORT_DIR")
