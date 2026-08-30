if(NOT DEFINED WIO_EXECUTABLE OR NOT DEFINED WIO_SOURCE OR NOT DEFINED WIO_OUTPUT_DIR)
    message(FATAL_ERROR "WIO_EXECUTABLE, WIO_SOURCE, and WIO_OUTPUT_DIR are required")
endif()

file(REMOVE_RECURSE "${WIO_OUTPUT_DIR}")
set(typed_output "${WIO_OUTPUT_DIR}/nested/input.typed.wir")
set(lowered_output "${WIO_OUTPUT_DIR}/input.lowered.wir")

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${WIO_SOURCE}"
        --no-builtin --emit-typed-wir --ir-output "${typed_output}"
    RESULT_VARIABLE typed_result
    OUTPUT_VARIABLE typed_stdout
    ERROR_VARIABLE typed_stderr
)
if(NOT typed_result EQUAL 0)
    message(FATAL_ERROR "Typed WIR emission failed (${typed_result}):\n${typed_stdout}\n${typed_stderr}")
endif()
if(NOT EXISTS "${typed_output}")
    message(FATAL_ERROR "Typed WIR emission did not create the requested nested output path")
endif()
file(READ "${typed_output}" typed_text)
if(NOT typed_text MATCHES "typed-wir module" OR
   NOT typed_text MATCHES "cond-branch %v0" OR
   NOT typed_text MATCHES "if.merge")
    message(FATAL_ERROR "Typed WIR output does not contain the expected local control flow")
endif()

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${WIO_SOURCE}"
        --no-builtin --emit-lowered-wir --ir-output "${lowered_output}"
    RESULT_VARIABLE lowered_result
    OUTPUT_VARIABLE lowered_stdout
    ERROR_VARIABLE lowered_stderr
)
if(NOT lowered_result EQUAL 0)
    message(FATAL_ERROR "Lowered WIR emission failed (${lowered_result}):\n${lowered_stdout}\n${lowered_stderr}")
endif()
file(READ "${lowered_output}" lowered_text)
if(NOT lowered_text MATCHES "lowered-wir module" OR
   NOT lowered_text MATCHES "cond-jump %v0" OR
   NOT lowered_text MATCHES "if.merge")
    message(FATAL_ERROR "Lowered WIR output does not contain canonical conditional control flow")
endif()

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${WIO_SOURCE}"
        --no-builtin --ir-output "${WIO_OUTPUT_DIR}/invalid.wir"
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_stderr
)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "--ir-output without an emission mode unexpectedly succeeded")
endif()

if(DEFINED WIO_CLI_EXECUTABLE AND EXISTS "${WIO_CLI_EXECUTABLE}")
    set(file_mode_output "${WIO_OUTPUT_DIR}/file-mode.lowered.wir")
    execute_process(
        COMMAND "${WIO_CLI_EXECUTABLE}" file lowered-wir "${WIO_SOURCE}"
            --no-builtin --ir-output "${file_mode_output}"
        RESULT_VARIABLE file_mode_result
        OUTPUT_VARIABLE file_mode_stdout
        ERROR_VARIABLE file_mode_stderr
    )
    if(NOT file_mode_result EQUAL 0 OR NOT EXISTS "${file_mode_output}")
        message(FATAL_ERROR
            "Self-hosted 'file lowered-wir' failed (${file_mode_result}):\n"
            "${file_mode_stdout}\n${file_mode_stderr}")
    endif()

    set(project_root "${WIO_OUTPUT_DIR}/project")
    file(MAKE_DIRECTORY "${project_root}/wio")
    configure_file("${WIO_SOURCE}" "${project_root}/wio/main.wio" COPYONLY)
    file(WRITE "${project_root}/wio.makewio"
        "schemaVersion = 1\n"
        "name = \"WirEmitProject\"\n"
        "template = \"wio-app\"\n\n"
        "[wio]\n"
        "entry = \"wio/main.wio\"\n"
        "target = \"exe\"\n"
        "sourceRoots = [\"wio\"]\n\n"
        "[host]\n"
        "enabled = false\n\n"
        "[build]\n"
        "buildDir = \".wio-build\"\n"
        "config = \"Debug\"\n\n"
        "[outputs]\n"
        "directory = \".wio-build/interop\"\n"
        "baseName = \"wir_emit_project\"\n"
        "wioName = \"wir_emit_project\"\n"
        "hostName = \"wir_emit_project_host\"\n")
    set(project_output "${WIO_OUTPUT_DIR}/project-output.typed.wir")
    execute_process(
        COMMAND "${WIO_CLI_EXECUTABLE}" project build
            --project "${project_root}" --emit-typed-wir --no-builtin
            --ir-output "${project_output}"
        RESULT_VARIABLE project_result
        OUTPUT_VARIABLE project_stdout
        ERROR_VARIABLE project_stderr
    )
    if(NOT project_result EQUAL 0 OR NOT EXISTS "${project_output}")
        message(FATAL_ERROR
            "Self-hosted project WIR emission failed (${project_result}):\n"
            "${project_stdout}\n${project_stderr}")
    endif()
endif()
