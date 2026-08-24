if(NOT DEFINED WIO_EXE OR NOT DEFINED WIO_SCRATCH_ROOT)
    message(FATAL_ERROR "WIO_EXE and WIO_SCRATCH_ROOT are required.")
endif()

file(REMOVE_RECURSE "${WIO_SCRATCH_ROOT}")
file(MAKE_DIRECTORY "${WIO_SCRATCH_ROOT}")
set(source_file "${WIO_SCRATCH_ROOT}/legacy.wio")
file(WRITE "${source_file}"
    "// @Native must stay in this comment\n"
    "const example: string = \"@CppName(quoted)\";\n"
    "@Native\n"
    "@CppName(Thing)\n"
    "object Thing {}\n")

execute_process(
    COMMAND "${WIO_EXE}" migrate attributes "${source_file}" --check
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_stdout
    ERROR_VARIABLE check_stderr
)
if(NOT check_result EQUAL 1 OR
   NOT "${check_stdout}${check_stderr}" MATCHES "require attribute migration")
    message(FATAL_ERROR
        "Attribute migration check did not report the legacy file.\n"
        "${check_stdout}${check_stderr}")
endif()

execute_process(
    COMMAND "${WIO_EXE}" migrate attributes "${source_file}" --write
    RESULT_VARIABLE write_result
    OUTPUT_VARIABLE write_stdout
    ERROR_VARIABLE write_stderr
)
if(NOT write_result EQUAL 0)
    message(FATAL_ERROR "Attribute migration write failed.\n${write_stdout}${write_stderr}")
endif()

file(READ "${source_file}" migrated)
foreach(expected
        "// @Native must stay in this comment"
        "\"@CppName(quoted)\""
        "[Native]"
        "[CppName(Thing)]")
    string(FIND "${migrated}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR
            "Migrated source is missing '${expected}'.\nSource:\n${migrated}")
    endif()
endforeach()

execute_process(
    COMMAND "${WIO_EXE}" migrate attributes "${source_file}" --check
    RESULT_VARIABLE current_result
    OUTPUT_VARIABLE current_stdout
    ERROR_VARIABLE current_stderr
)
if(NOT current_result EQUAL 0 OR
   NOT "${current_stdout}${current_stderr}" MATCHES "Attribute syntax is current")
    message(FATAL_ERROR
        "Attribute migration was not idempotent.\n${current_stdout}${current_stderr}")
endif()

file(REMOVE_RECURSE "${WIO_SCRATCH_ROOT}")
message(STATUS "Attribute migration smoke succeeded.")
