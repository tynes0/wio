if(NOT DEFINED WIO_EXECUTABLE OR NOT DEFINED WIO_SOURCE OR NOT DEFINED WIO_OUTPUT_DIR)
    message(FATAL_ERROR "WIO_EXECUTABLE, WIO_SOURCE, and WIO_OUTPUT_DIR are required")
endif()

file(REMOVE_RECURSE "${WIO_OUTPUT_DIR}")
set(typed_output "${WIO_OUTPUT_DIR}/nested/input.typed.wir")
set(lowered_output "${WIO_OUTPUT_DIR}/input.lowered.wir")
set(bytecode_output "${WIO_OUTPUT_DIR}/nested/input.wiob")

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
   NOT typed_text MATCHES "cond-branch" OR
   NOT typed_text MATCHES "while.header" OR
   NOT typed_text MATCHES "while.exit")
    message(FATAL_ERROR "Typed WIR output does not contain the expected loop control flow")
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
   NOT lowered_text MATCHES "cond-jump" OR
   NOT lowered_text MATCHES "while.header" OR
   NOT lowered_text MATCHES "while.exit")
    message(FATAL_ERROR "Lowered WIR output does not contain canonical conditional control flow")
endif()

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${WIO_SOURCE}"
        --no-builtin --emit-bytecode --bytecode-output "${bytecode_output}"
    RESULT_VARIABLE bytecode_result
    OUTPUT_VARIABLE bytecode_stdout
    ERROR_VARIABLE bytecode_stderr
)
if(NOT bytecode_result EQUAL 0)
    message(FATAL_ERROR "Bytecode emission failed (${bytecode_result}):\n${bytecode_stdout}\n${bytecode_stderr}")
endif()
if(NOT EXISTS "${bytecode_output}")
    message(FATAL_ERROR "Bytecode emission did not create the requested nested output path")
endif()
file(READ "${bytecode_output}" bytecode_magic HEX LIMIT 8)
string(TOLOWER "${bytecode_magic}" bytecode_magic)
if(NOT bytecode_magic STREQUAL "57494f42430d0a1a")
    message(FATAL_ERROR "Bytecode output does not begin with the WIOB v1 magic: ${bytecode_magic}")
endif()

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${bytecode_output}" --disassemble-bytecode
    RESULT_VARIABLE disassemble_result
    OUTPUT_VARIABLE disassemble_stdout
    ERROR_VARIABLE disassemble_stderr
)
if(NOT disassemble_result EQUAL 0 OR
   NOT disassemble_stdout MATCHES "\\.wiob 1\\.0" OR
   NOT disassemble_stdout MATCHES "fn @0")
    message(FATAL_ERROR
        "Bytecode disassembly failed (${disassemble_result}):\n"
        "${disassemble_stdout}\n${disassemble_stderr}")
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

execute_process(
    COMMAND "${WIO_EXECUTABLE}" "${WIO_SOURCE}"
        --no-builtin --bytecode-output "${WIO_OUTPUT_DIR}/invalid.wiob"
    RESULT_VARIABLE invalid_bytecode_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(invalid_bytecode_result EQUAL 0)
    message(FATAL_ERROR "--bytecode-output without --emit-bytecode unexpectedly succeeded")
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

    set(file_bytecode_output "${WIO_OUTPUT_DIR}/file-mode.wiob")
    execute_process(
        COMMAND "${WIO_CLI_EXECUTABLE}" file bytecode "${WIO_SOURCE}"
            --no-builtin --bytecode-output "${file_bytecode_output}"
        RESULT_VARIABLE file_bytecode_result
        OUTPUT_VARIABLE file_bytecode_stdout
        ERROR_VARIABLE file_bytecode_stderr
    )
    if(NOT file_bytecode_result EQUAL 0 OR NOT EXISTS "${file_bytecode_output}")
        message(FATAL_ERROR
            "Self-hosted 'file bytecode' failed (${file_bytecode_result}):\n"
            "${file_bytecode_stdout}\n${file_bytecode_stderr}")
    endif()
    execute_process(
        COMMAND "${WIO_CLI_EXECUTABLE}" file disassemble "${file_bytecode_output}"
        RESULT_VARIABLE file_disassemble_result
        OUTPUT_VARIABLE file_disassemble_stdout
        ERROR_VARIABLE file_disassemble_stderr
    )
    if(NOT file_disassemble_result EQUAL 0 OR NOT file_disassemble_stdout MATCHES "fn @0")
        message(FATAL_ERROR
            "Self-hosted 'file disassemble' failed (${file_disassemble_result}):\n"
            "${file_disassemble_stdout}\n${file_disassemble_stderr}")
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


    set(project_bytecode_output "${WIO_OUTPUT_DIR}/project-output.wiob")
    execute_process(
        COMMAND "${WIO_CLI_EXECUTABLE}" project build
            --project "${project_root}" --emit-bytecode --no-builtin
            --bytecode-output "${project_bytecode_output}"
        RESULT_VARIABLE project_bytecode_result
        OUTPUT_VARIABLE project_bytecode_stdout
        ERROR_VARIABLE project_bytecode_stderr
    )
    if(NOT project_bytecode_result EQUAL 0 OR NOT EXISTS "${project_bytecode_output}")
        message(FATAL_ERROR
            "Self-hosted project bytecode emission failed (${project_bytecode_result}):\n"
            "${project_bytecode_stdout}\n${project_bytecode_stderr}")
    endif()
endif()
