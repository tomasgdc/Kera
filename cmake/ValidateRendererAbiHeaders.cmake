if(NOT DEFINED KERA_SOURCE_DIR)
    message(FATAL_ERROR "KERA_SOURCE_DIR must be provided")
endif()

set(KERA_PUBLIC_API_HEADERS
    "${KERA_SOURCE_DIR}/include/kera/kera.h"
    "${KERA_SOURCE_DIR}/include/kera/renderer/api.h"
    "${KERA_SOURCE_DIR}/include/kera/renderer/abi.h"
)

set(KERA_FORBIDDEN_PUBLIC_API_PATTERNS
    "std::"
    "#include <string>"
    "#include <vector>"
    "#include <span>"
    "#include <memory>"
    "#include <optional>"
    "#include <functional>"
)

foreach(header IN LISTS KERA_PUBLIC_API_HEADERS)
    file(READ "${header}" content)
    foreach(pattern IN LISTS KERA_FORBIDDEN_PUBLIC_API_PATTERNS)
        string(FIND "${content}" "${pattern}" found_at)
        if(NOT found_at EQUAL -1)
            message(FATAL_ERROR "Public renderer API header ${header} contains forbidden STL pattern: ${pattern}")
        endif()
    endforeach()
endforeach()

message(STATUS "Public renderer API headers do not expose STL types.")
