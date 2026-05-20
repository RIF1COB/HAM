cmake_minimum_required(VERSION 3.16)

# Find Python interpreter (for generator) - optional; try to use system python if available
find_package(Python3 COMPONENTS Interpreter)

# Resolve project root reliably relative to this CMakeLists location
get_filename_component(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../ ABSOLUTE)

set(GEN_SCRIPT ${PROJECT_ROOT_DIR}/tools/config_generator/generate_config.py)
set(GEN_INPUT ${PROJECT_ROOT_DIR}/tools/config_generator/vendor_devices.csv)
set(GEN_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(GEN_H ${GEN_OUT_DIR}/config_generated.h)
set(GEN_C ${GEN_OUT_DIR}/config_generated.c)

# Run generator at CMake configure time if input changed or output missing.
if (Python3_FOUND)
    message(STATUS "Python3 executable: ${Python3_EXECUTABLE}")
    message(STATUS "Config generator script: ${GEN_SCRIPT}")
    file(TIMESTAMP ${GEN_INPUT} _gen_in_ts)
    if (EXISTS ${GEN_H})
        file(TIMESTAMP ${GEN_H} _gen_out_ts)
    else()
        set(_gen_out_ts "0")
    endif()
    if (NOT EXISTS ${GEN_H} OR _gen_in_ts STRGREATER _gen_out_ts)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT} -i ${GEN_INPUT} -o ${GEN_OUT_DIR} --cfg-version CFG_2026_05_V1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE _gen_result
            OUTPUT_VARIABLE _gen_out
            ERROR_VARIABLE _gen_err
        )
        if (NOT _gen_result EQUAL 0)
            message(FATAL_ERROR "Config generator failed: ${_gen_err}")
        endif()
    endif()
else()
    message(WARNING "Python3 not found; using existing config_generated.* if present")
endif()

idf_component_register(SRCS "device_registry.c" "driver_pool.c" "gpio_manager.c" "nvs_identity.c" "feature_flags.c" "structured_logging.c" "config_generated.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver nvs_flash)
