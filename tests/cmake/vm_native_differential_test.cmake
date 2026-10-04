foreach(required WIO_EXECUTABLE WIO_ROOT WIO_OUTPUT_DIR WIO_SHARED_SUFFIX)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "VM native differential test requires ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")
set(source "${WIO_ROOT}/tests/vm_native_differential.wio")
set(native "${WIO_OUTPUT_DIR}/native${WIO_EXECUTABLE_SUFFIX}")
set(bridge "${WIO_OUTPUT_DIR}/bridge${WIO_SHARED_SUFFIX}")
set(bytecode "${WIO_OUTPUT_DIR}/program.wiob")

function(build_backend label)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
            "${WIO_EXECUTABLE}" "${source}" --cpp-backend wir
            --include-dir "${WIO_ROOT}/tests/native" ${ARGN}
        WORKING_DIRECTORY "${WIO_ROOT}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 180
    )
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "${label} compilation failed (${result}):\n${output}\n${error}")
    endif()
endfunction()

build_backend("C++" --target exe --output "${native}")
build_backend("VM bridge + bytecode" --target shared --output "${bridge}" --bytecode-sidecar "${bytecode}")
foreach(artifact "${native}" "${bridge}" "${bytecode}")
    if(NOT EXISTS "${artifact}")
        message(FATAL_ERROR "Missing compiled differential artifact: ${artifact}")
    endif()
endforeach()

function(compare_run argument succeeds)
    execute_process(
        COMMAND "${native}" "${argument}"
        WORKING_DIRECTORY "${WIO_OUTPUT_DIR}"
        RESULT_VARIABLE native_result OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error
        TIMEOUT 30
    )
    execute_process(
        COMMAND "${WIO_EXECUTABLE}" --vm-run-bytecode "${bytecode}" --native-library "${bridge}" -- "${argument}"
        WORKING_DIRECTORY "${WIO_OUTPUT_DIR}"
        RESULT_VARIABLE vm_result OUTPUT_VARIABLE vm_output ERROR_VARIABLE vm_error
        TIMEOUT 30
    )
    if(succeeds)
        if(NOT "${native_result}" STREQUAL "0" OR NOT "${vm_result}" STREQUAL "0")
            message(FATAL_ERROR "Expected successful executions: C++=${native_result}, VM=${vm_result}\nC++: ${native_output}\n${native_error}\nVM: ${vm_output}\n${vm_error}")
        endif()
        if(NOT native_output MATCHES "native-differential-ok:scalar/ref/alias/pod/generic/opaque/any/callback/string/text/async")
            message(FATAL_ERROR "Success oracle missing; identical silent failures cannot pass this gate")
        endif()
        if(NOT native_output STREQUAL vm_output OR NOT native_error STREQUAL vm_error)
            message(FATAL_ERROR "Successful execution output mismatch\nC++: ${native_output}\n${native_error}\nVM: ${vm_output}\n${vm_error}")
        endif()
    else()
        # Diagnostic wrappers differ by backend, but the boundary must fail and
        # retain the original native payload; normal return codes cannot pass.
        if(NOT "${native_result}" STREQUAL "1" OR NOT "${vm_result}" STREQUAL "1")
            message(FATAL_ERROR "Expected native-boundary failures: C++=${native_result}, VM=${vm_result}\n${native_error}\n${vm_error}")
        endif()
        # The native executable currently writes its diagnostic to stdout,
        # whereas the VM runner writes it to stderr. Compare program effects
        # before that wrapper, not the wrapper's output channel.
        set(native_effects "${native_output}")
        string(FIND "${native_output}" "Runtime Error:" diagnostic_offset)
        if(NOT diagnostic_offset EQUAL -1)
            string(SUBSTRING "${native_output}" 0 ${diagnostic_offset} native_effects)
        endif()
        if(NOT native_effects STREQUAL vm_output OR NOT native_effects MATCHES "before-native-failure")
            message(FATAL_ERROR "Native pre-failure effects differ: C++=${native_output}, VM=${vm_output}")
        endif()
        if(NOT "${native_output}${native_error}" MATCHES "native-differential-failure" OR
           NOT vm_error MATCHES "WVM1222.*native-differential-failure")
            message(FATAL_ERROR "Native failure identity lost\nC++: ${native_output}\n${native_error}\nVM: ${vm_output}\n${vm_error}")
        endif()
    endif()
endfunction()

compare_run("two words" TRUE)
compare_run("throw" FALSE)
function(expect_remaining_boundary argument diagnostic)
    execute_process(COMMAND "${native}" "${argument}" RESULT_VARIABLE native_result
        OUTPUT_VARIABLE native_output ERROR_VARIABLE native_error TIMEOUT 30)
    execute_process(COMMAND "${WIO_EXECUTABLE}" --vm-run-bytecode "${bytecode}"
        --native-library "${bridge}" -- "${argument}" RESULT_VARIABLE vm_result
        OUTPUT_VARIABLE vm_output ERROR_VARIABLE vm_error TIMEOUT 30)
    if(NOT "${native_result}" STREQUAL "0" OR NOT "${vm_result}" STREQUAL "1" OR
       NOT vm_error MATCHES "${diagnostic}")
        message(FATAL_ERROR "${argument} boundary changed: C++=${native_result}, VM=${vm_result}\n${native_output}\n${native_error}\n${vm_output}\n${vm_error}")
    endif()
endfunction()
# These are explicit remaining VM/native conversions, not parity claims.
# Fail deterministically instead of interpreting a VM aggregate as C++ bytes.
expect_remaining_boundary("local-pod" "WVM1223")
expect_remaining_boundary("local-any" "WVM1223")
expect_remaining_boundary("pod-callback" "WVM1225")
message(STATUS "C++/VM native differential gate passed: values, references, ownership, callbacks, async, failure")
