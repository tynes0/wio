foreach(required WIO_EXECUTABLE WIO_ROOT WIO_OUTPUT_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "VM project CLI test requires ${required}")
    endif()
endforeach()

set(project "${WIO_ROOT}/tests/projects/vm_native_cli")

function(run_checked label expected_result)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}" "${WIO_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT "${result}" STREQUAL "${expected_result}")
        message(FATAL_ERROR "${label} failed (${result}):\n${output}\n${error}")
    endif()
    set(command_output "${output}" PARENT_SCOPE)
    set(command_error "${error}" PARENT_SCOPE)
endfunction()

run_checked("VM native project build" 0 project build --backend vm --build-dir "${WIO_OUTPUT_DIR}" --rebuild)
run_checked("VM native project run" 0 project run --backend vm --build-dir "${WIO_OUTPUT_DIR}"
    --no-build -- "two words" "--literal")
if(NOT command_output MATCHES "vm-native:42")
    message(FATAL_ERROR "Native async bridge did not execute: ${command_output}\n${command_error}")
endif()

run_checked("VM incremental run" 0 project run --backend vm --build-dir "${WIO_OUTPUT_DIR}"
    -- "two words" "--literal")
if(NOT command_output MATCHES "up to date" OR NOT command_output MATCHES "vm-native:42")
    message(FATAL_ERROR "VM project did not reuse both outputs: ${command_output}\n${command_error}")
endif()

run_checked("Unknown backend rejection" 1 project run --backend invalid --build-dir "${WIO_OUTPUT_DIR}" --no-build)
run_checked("Legacy VM rejection" 1 project build --backend vm --cpp-backend legacy --build-dir "${WIO_OUTPUT_DIR}")

set(bytecode "${WIO_OUTPUT_DIR}/interop/vm_native_cli.wiob")
file(GLOB bridge "${WIO_OUTPUT_DIR}/interop/vm_native_cli.vm-native.dll"
    "${WIO_OUTPUT_DIR}/interop/vm_native_cli.vm-native.so"
    "${WIO_OUTPUT_DIR}/interop/vm_native_cli.vm-native.dylib")
list(LENGTH bridge bridge_count)
if(NOT bridge_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one VM native bridge: ${bridge}")
endif()
set(unicode_dir "${WIO_OUTPUT_DIR}/Unicode-ö-λ")
file(MAKE_DIRECTORY "${unicode_dir}")
file(COPY "${bytecode}" "${bridge}" DESTINATION "${unicode_dir}")
get_filename_component(bridge_name "${bridge}" NAME)
run_checked("UTF-8 module and native library paths" 0 --vm-run-bytecode "${unicode_dir}/vm_native_cli.wiob"
    --native-library "${unicode_dir}/${bridge_name}" -- "manifest-arg" "two words" "--literal")
run_checked("Missing dynamic registry rejection" 1 --vm-run-bytecode "${bytecode}"
    --native-library "${WIO_OUTPUT_DIR}/missing-registry")
if(NOT command_error MATCHES "does not exist")
    message(FATAL_ERROR "Missing registry diagnostic was lost: ${command_output}\n${command_error}")
endif()
file(WRITE "${WIO_OUTPUT_DIR}/invalid.wiob" "not bytecode")
run_checked("Malformed bytecode rejection" 1 --vm-run-bytecode "${WIO_OUTPUT_DIR}/invalid.wiob")
if(NOT command_error MATCHES "bytecode decode failed")
    message(FATAL_ERROR "Malformed module diagnostic was lost: ${command_output}\n${command_error}")
endif()

file(SHA256 "${bytecode}" previous_bytecode)
file(SHA256 "${bridge}" previous_bridge)
run_checked("Failed native build preserves bytecode" 1 "${project}/wio/main.wio"
    --target shared --output "${bridge}" --bytecode-sidecar "${bytecode}"
    --include-dir "${project}/native/include" --backend-arg "-WioTestForcedFailure")
file(SHA256 "${bytecode}" current_bytecode)
file(SHA256 "${bridge}" current_bridge)
if(NOT previous_bytecode STREQUAL current_bytecode OR NOT previous_bridge STREQUAL current_bridge)
    message(FATAL_ERROR "A failed native build replaced a previously runnable VM artifact")
endif()
