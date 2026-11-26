# The generic export exception is canonical-only. Do not silently expand the
# older C ABI contract while enabling owning task parameters in the SDK path.
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "WIO_ROOT=${WIO_ROOT}"
    "${WIO_EXECUTABLE}" "${WIO_LIBRARY_SOURCE}" --no-builtin
    --target shared --cpp-backend legacy --dry-run
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 45 ENCODING UTF-8)
if("${result}" STREQUAL "0" OR
   NOT "${output}\n${error}" MATCHES "instantiated with.*non-C-ABI-safe parameter.*coroutine")
    message(FATAL_ERROR "Legacy generic task export must retain its C ABI rejection: ${output}\n${error}")
endif()
include("${WIO_ROOT}/tests/cmake/vm_sdk_session_artifacts_test.cmake")
