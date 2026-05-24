if(NOT DEFINED KERA_SAMPLE_EXE)
    message(FATAL_ERROR "KERA_SAMPLE_EXE is required")
endif()

if(NOT DEFINED KERA_SAMPLE_LABEL)
    set(KERA_SAMPLE_LABEL "Kera sample smoke")
endif()

if(NOT DEFINED KERA_SAMPLE_TIMEOUT)
    set(KERA_SAMPLE_TIMEOUT 20)
endif()

if(NOT "$ENV{KERA_RUN_GPU_SMOKE}" STREQUAL "1")
    message(STATUS "${KERA_SAMPLE_LABEL} skipped; set KERA_RUN_GPU_SMOKE=1 to run GPU-backed sample smoke tests.")
    return()
endif()

execute_process(
    COMMAND "${KERA_SAMPLE_EXE}" ${KERA_SAMPLE_ARGS}
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

message(STATUS "${KERA_SAMPLE_LABEL} passed.")
