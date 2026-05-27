if(NOT DEFINED KERA_SAMPLE_EXE)
    message(FATAL_ERROR "KERA_SAMPLE_EXE is required")
endif()

if(NOT DEFINED KERA_SAMPLE_LABEL)
    set(KERA_SAMPLE_LABEL "Kera sample smoke")
endif()

if(NOT DEFINED KERA_SAMPLE_TIMEOUT)
    set(KERA_SAMPLE_TIMEOUT 20)
endif()

if(NOT DEFINED KERA_FAIL_ON_VALIDATION_ERRORS)
    set(KERA_FAIL_ON_VALIDATION_ERRORS ON)
endif()

if(NOT "$ENV{KERA_RUN_GPU_SMOKE}" STREQUAL "1")
    message(STATUS "${KERA_SAMPLE_LABEL} skipped; set KERA_RUN_GPU_SMOKE=1 to run GPU-backed sample smoke tests.")
    return()
endif()

if(DEFINED KERA_SAMPLE_SCREENSHOT_DIR AND NOT KERA_SAMPLE_SCREENSHOT_DIR STREQUAL "")
    if(NOT DEFINED KERA_SAMPLE_SCREENSHOT_FRAMES)
        set(KERA_SAMPLE_SCREENSHOT_FRAMES "24")
    endif()
    file(MAKE_DIRECTORY "${KERA_SAMPLE_SCREENSHOT_DIR}")
    file(GLOB existing_screenshots "${KERA_SAMPLE_SCREENSHOT_DIR}/*.ppm")
    if(existing_screenshots)
        file(REMOVE ${existing_screenshots})
    endif()
    set(sample_command
        ${CMAKE_COMMAND} -E env
            "VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_screenshot"
            "VK_SCREENSHOT_FRAMES=${KERA_SAMPLE_SCREENSHOT_FRAMES}"
            "VK_SCREENSHOT_DIR=${KERA_SAMPLE_SCREENSHOT_DIR}"
            "${KERA_SAMPLE_EXE}" ${KERA_SAMPLE_ARGS}
    )
else()
    set(sample_command "${KERA_SAMPLE_EXE}" ${KERA_SAMPLE_ARGS})
endif()

execute_process(
    COMMAND ${sample_command}
    TIMEOUT ${KERA_SAMPLE_TIMEOUT}
    RESULT_VARIABLE sample_result
    OUTPUT_VARIABLE sample_stdout
    ERROR_VARIABLE sample_stderr
)

if(DEFINED KERA_SAMPLE_LOG AND NOT KERA_SAMPLE_LOG STREQUAL "")
    file(WRITE "${KERA_SAMPLE_LOG}" "${sample_stdout}\n${sample_stderr}")
endif()

if(NOT sample_result EQUAL 0)
    message(STATUS "${sample_stdout}")
    message(STATUS "${sample_stderr}")
    message(FATAL_ERROR "${KERA_SAMPLE_LABEL} failed with exit code ${sample_result}")
endif()

if(DEFINED KERA_SAMPLE_SCREENSHOT_DIR AND NOT KERA_SAMPLE_SCREENSHOT_DIR STREQUAL "")
    file(GLOB sample_screenshots "${KERA_SAMPLE_SCREENSHOT_DIR}/*.ppm")
    if(sample_screenshots STREQUAL "")
        message(STATUS "${sample_stdout}")
        message(STATUS "${sample_stderr}")
        message(FATAL_ERROR "${KERA_SAMPLE_LABEL} did not produce a Vulkan screenshot capture")
    endif()
    if(DEFINED KERA_SAMPLE_SCREENSHOT_METRICS_EXE AND NOT KERA_SAMPLE_SCREENSHOT_METRICS_EXE STREQUAL "")
        list(GET sample_screenshots 0 sample_screenshot)
        if(DEFINED KERA_SAMPLE_SCREENSHOT_METRICS_ARGS)
            set(metric_args ${KERA_SAMPLE_SCREENSHOT_METRICS_ARGS})
        else()
            set(metric_args "")
        endif()
        execute_process(
            COMMAND "${KERA_SAMPLE_SCREENSHOT_METRICS_EXE}" "${sample_screenshot}" ${metric_args}
            RESULT_VARIABLE metrics_result
            OUTPUT_VARIABLE metrics_stdout
            ERROR_VARIABLE metrics_stderr
        )
        if(DEFINED KERA_SAMPLE_LOG AND NOT KERA_SAMPLE_LOG STREQUAL "")
            file(APPEND "${KERA_SAMPLE_LOG}" "\n${metrics_stdout}\n${metrics_stderr}")
        endif()
        if(NOT metrics_result EQUAL 0)
            message(STATUS "${metrics_stdout}")
            message(STATUS "${metrics_stderr}")
            message(FATAL_ERROR "${KERA_SAMPLE_LABEL} screenshot metrics failed")
        endif()
    endif()
endif()

if(KERA_FAIL_ON_VALIDATION_ERRORS)
    set(sample_output "${sample_stdout}\n${sample_stderr}")
    string(FIND "${sample_output}" "[ERROR] Vulkan validation:" validation_error_index)
    if(NOT validation_error_index EQUAL -1)
        message(STATUS "${sample_stdout}")
        message(STATUS "${sample_stderr}")
        message(FATAL_ERROR "${KERA_SAMPLE_LABEL} emitted Vulkan validation errors")
    endif()
endif()

message(STATUS "${KERA_SAMPLE_LABEL} passed.")
