if(NOT DEFINED KERA_SHADER_ROOT)
    message(FATAL_ERROR "KERA_SHADER_ROOT is required")
endif()

if(NOT DEFINED KERA_SLANGC_EXECUTABLE OR KERA_SLANGC_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "KERA_SLANGC_EXECUTABLE is required")
endif()

if(NOT EXISTS "${KERA_SLANGC_EXECUTABLE}")
    message(FATAL_ERROR "Missing Slang compiler: ${KERA_SLANGC_EXECUTABLE}")
endif()

if(NOT DEFINED KERA_REFLECTION_OUTPUT_DIR)
    set(KERA_REFLECTION_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shader-reflection")
endif()

file(MAKE_DIRECTORY "${KERA_REFLECTION_OUTPUT_DIR}")

function(kera_require_json_token json_path token reason)
    file(READ "${json_path}" reflection_json)
    string(FIND "${reflection_json}" "${token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR "Missing reflection token '${token}' in ${json_path}: ${reason}")
    endif()
endfunction()

function(kera_reflect_slang_entry relative_path entry_point shader_stage)
    set(shader_path "${KERA_SHADER_ROOT}/${relative_path}")
    if(NOT EXISTS "${shader_path}")
        message(FATAL_ERROR "Missing Slang shader: ${shader_path}")
    endif()

    get_filename_component(shader_name "${relative_path}" NAME_WE)
    set(output_base "${KERA_REFLECTION_OUTPUT_DIR}/${shader_name}_${entry_point}")
    set(json_path "${output_base}.reflection.json")
    set(spirv_path "${output_base}.spv")

    execute_process(
        COMMAND "${KERA_SLANGC_EXECUTABLE}"
            "${shader_path}"
            -target spirv
            -profile spirv_1_5
            -entry "${entry_point}"
            -stage "${shader_stage}"
            -reflection-json "${json_path}"
            -o "${spirv_path}"
        RESULT_VARIABLE slang_result
        OUTPUT_VARIABLE slang_output
        ERROR_VARIABLE slang_error
    )

    if(NOT slang_result EQUAL 0)
        message(FATAL_ERROR "Slang reflection failed for ${relative_path} [${entry_point}]: ${slang_output}${slang_error}")
    endif()

    if(NOT EXISTS "${json_path}")
        message(FATAL_ERROR "Slang did not write reflection JSON: ${json_path}")
    endif()

    kera_require_json_token("${json_path}" "\"name\": \"${entry_point}\"" "entry point should be reflected")
    kera_require_json_token("${json_path}" "\"stage\": \"${shader_stage}\"" "shader stage should be reflected")
endfunction()

kera_reflect_slang_entry("instanced_triangle.slang" "vertexMain" "vertex")
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_vertexMain.reflection.json"
    "\"name\": \"globalParams\""
    "instanced triangle should reflect the global parameter block"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_vertexMain.reflection.json"
    "\"kind\": \"parameterBlock\""
    "instanced triangle should reflect a Slang parameter block"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_vertexMain.reflection.json"
    "\"semanticName\": \"TRANSFORM\""
    "instanced triangle should reflect instance transform input semantics"
)

kera_reflect_slang_entry("instanced_triangle_many_lights.slang" "lightingFragmentMain" "fragment")
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
    "\"name\": \"sceneTexture\""
    "many-lights lighting pass should reflect the sampled scene texture"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
    "\"kind\": \"resource\""
    "many-lights lighting pass should reflect texture resources"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
    "\"name\": \"sceneSampler\""
    "many-lights lighting pass should reflect the scene sampler"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
    "\"kind\": \"samplerState\""
    "many-lights lighting pass should reflect sampler resources"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
    "\"name\": \"lightingParams\""
    "many-lights lighting pass should reflect the lighting uniform buffer"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
    "\"name\": \"LightingUniforms\""
    "many-lights lighting pass should reflect the lighting uniform layout"
)

kera_reflect_slang_entry("damaged_helmet.slang" "helmetVertexMain" "vertex")
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/damaged_helmet_helmetVertexMain.reflection.json"
    "\"semanticName\": \"TANGENT\""
    "DamagedHelmet should reflect generated tangent input semantics"
)
kera_reflect_slang_entry("damaged_helmet.slang" "helmetFragmentMain" "fragment")
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/damaged_helmet_helmetFragmentMain.reflection.json"
    "\"name\": \"helmetParams\""
    "DamagedHelmet should reflect material factor uniforms"
)
kera_require_json_token(
    "${KERA_REFLECTION_OUTPUT_DIR}/damaged_helmet_helmetFragmentMain.reflection.json"
    "\"name\": \"normalTexture\""
    "DamagedHelmet should reflect normal texture binding"
)

if(DEFINED KERA_REFLECTION_METADATA_TEST_EXE AND NOT KERA_REFLECTION_METADATA_TEST_EXE STREQUAL "")
    execute_process(
        COMMAND "${KERA_REFLECTION_METADATA_TEST_EXE}"
            "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_vertexMain.reflection.json"
            "${KERA_REFLECTION_OUTPUT_DIR}/instanced_triangle_many_lights_lightingFragmentMain.reflection.json"
            "${KERA_SHADER_ROOT}"
        RESULT_VARIABLE metadata_result
        OUTPUT_VARIABLE metadata_output
        ERROR_VARIABLE metadata_error
    )

    if(NOT metadata_result EQUAL 0)
        message(FATAL_ERROR "C++ Slang reflection metadata validation failed: ${metadata_output}${metadata_error}")
    endif()
endif()

message(STATUS "Slang shader reflection contracts validated.")
