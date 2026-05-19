if(NOT DEFINED WIO_EXE)
    message(FATAL_ERROR "WIO_EXE was not provided.")
endif()

if(NOT DEFINED WIO_BUILD_DIR OR WIO_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_BUILD_DIR was not provided.")
endif()

if(NOT DEFINED WIO_OUTPUT_DIR OR WIO_OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "WIO_OUTPUT_DIR was not provided.")
endif()

if(NOT DEFINED WIO_CONFIG OR WIO_CONFIG STREQUAL "")
    set(WIO_CONFIG Debug)
endif()

if(NOT DEFINED WIO_SOURCE OR WIO_SOURCE STREQUAL "")
    message(FATAL_ERROR "WIO_SOURCE was not provided.")
endif()

file(MAKE_DIRECTORY "${WIO_OUTPUT_DIR}")

file(GLOB existing_entries LIST_DIRECTORIES true "${WIO_OUTPUT_DIR}/wio-*")
foreach(existing_entry IN LISTS existing_entries)
    file(REMOVE_RECURSE "${existing_entry}")
endforeach()

execute_process(
    COMMAND "${WIO_EXE}" package --build-dir "${WIO_BUILD_DIR}" --config "${WIO_CONFIG}" --output-dir "${WIO_OUTPUT_DIR}" --no-zip --clean
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE package_result
    OUTPUT_VARIABLE package_stdout
    ERROR_VARIABLE package_stderr
)

set(package_output "${package_stdout}${package_stderr}")

if(NOT package_result EQUAL 0)
    message(FATAL_ERROR
        "Packaged file-run smoke could not stage a package. Code: ${package_result}\n"
        "Tool output:\n${package_output}"
    )
endif()

file(GLOB package_roots LIST_DIRECTORIES true "${WIO_OUTPUT_DIR}/wio-*")
list(LENGTH package_roots package_root_count)
if(NOT package_root_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one staged package root under '${WIO_OUTPUT_DIR}', but found ${package_root_count}.\n"
        "Tool output:\n${package_output}"
    )
endif()

list(GET package_roots 0 package_root)

if(WIN32)
    set(packaged_wio "${package_root}/bin/wio.exe")
else()
    set(packaged_wio "${package_root}/bin/wio")
endif()

if(NOT EXISTS "${packaged_wio}")
    message(FATAL_ERROR "Packaged wio executable was not found at '${packaged_wio}'.")
endif()

set(work_dir "${WIO_OUTPUT_DIR}/packaged-file-run-work")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")

get_filename_component(source_name "${WIO_SOURCE}" NAME)
set(temp_source "${work_dir}/${source_name}")
configure_file("${WIO_SOURCE}" "${temp_source}" COPYONLY)

get_filename_component(source_stem "${temp_source}" NAME_WE)
set(adjacent_cpp "${temp_source}.cpp")

if(WIN32)
    set(adjacent_output "${work_dir}/${source_stem}.exe")
else()
    set(adjacent_output "${work_dir}/${source_stem}")
endif()

file(REMOVE "${adjacent_cpp}" "${adjacent_output}")
file(REMOVE_RECURSE "${work_dir}/.wio-build" "${package_root}/.wio-build")

execute_process(
    COMMAND "${packaged_wio}" file run "${temp_source}" -- alpha beta
    WORKING_DIRECTORY "${package_root}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

set(run_output "${run_stdout}${run_stderr}")

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "Packaged file-run smoke failed with code ${run_result}.\n"
        "Tool output:\n${run_output}"
    )
endif()

string(FIND "${run_output}" "file-run-args-ok" output_index)
if(output_index EQUAL -1)
    message(FATAL_ERROR
        "Packaged file-run smoke did not print the expected success marker.\n"
        "Tool output:\n${run_output}"
    )
endif()

if(EXISTS "${adjacent_cpp}")
    message(FATAL_ERROR
        "Packaged file-run left a source-adjacent generated C++ file behind:\n"
        "  ${adjacent_cpp}\n"
        "Tool output:\n${run_output}"
    )
endif()

if(EXISTS "${adjacent_output}")
    message(FATAL_ERROR
        "Packaged file-run left a source-adjacent backend output behind:\n"
        "  ${adjacent_output}\n"
        "Tool output:\n${run_output}"
    )
endif()

if(EXISTS "${work_dir}/.wio-build")
    message(FATAL_ERROR
        "Packaged file-run should not create a local '.wio-build' cache under the scratch source directory.\n"
        "Found: ${work_dir}/.wio-build\n"
        "Tool output:\n${run_output}"
    )
endif()

if(EXISTS "${package_root}/.wio-build")
    message(FATAL_ERROR
        "Packaged file-run should not create a '.wio-build' cache under the installed package root.\n"
        "Found: ${package_root}/.wio-build\n"
        "Tool output:\n${run_output}"
    )
endif()

message(STATUS "Packaged file-run smoke succeeded for ${packaged_wio}")
