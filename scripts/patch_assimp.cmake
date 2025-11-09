cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED WAYVERB_SOURCE_DIR)
    message(FATAL_ERROR "WAYVERB_SOURCE_DIR is required")
endif()

if(NOT DEFINED WAYVERB_PATCH_FILE)
    message(FATAL_ERROR "WAYVERB_PATCH_FILE is required")
endif()

if(NOT DEFINED WAYVERB_PATCH_SENTINEL)
    set(WAYVERB_PATCH_SENTINEL
        "${WAYVERB_SOURCE_DIR}/.wayverb_assimp_patch_applied")
endif()

if(NOT DEFINED WAYVERB_GIT_EXECUTABLE)
    set(WAYVERB_GIT_EXECUTABLE git)
endif()

if(EXISTS "${WAYVERB_PATCH_SENTINEL}")
    message(STATUS "[wayverb] assimp patch sentinel present, skipping")
    return()
endif()

if(NOT EXISTS "${WAYVERB_PATCH_FILE}")
    message(FATAL_ERROR
            "Patch file ${WAYVERB_PATCH_FILE} not found for assimp patch")
endif()

function(wayverb_git_apply_check out_var err_var)
    execute_process(
        COMMAND "${WAYVERB_GIT_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${WAYVERB_SOURCE_DIR}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )
    set(${out_var} "${_stdout}" PARENT_SCOPE)
    set(${err_var} "${_stderr}" PARENT_SCOPE)
    set(wayverb_git_last_result "${_result}" PARENT_SCOPE)
endfunction()

set(_out "")
set(_err "")
wayverb_git_apply_check(_out _err apply --check "${WAYVERB_PATCH_FILE}")

if(wayverb_git_last_result EQUAL 0)
    execute_process(
        COMMAND "${WAYVERB_GIT_EXECUTABLE}" apply "${WAYVERB_PATCH_FILE}"
        WORKING_DIRECTORY "${WAYVERB_SOURCE_DIR}"
        RESULT_VARIABLE _apply_result
        OUTPUT_VARIABLE _apply_out
        ERROR_VARIABLE _apply_err
    )
    if(NOT _apply_result EQUAL 0)
        message(FATAL_ERROR
                "Failed to apply assimp patch:\n${_apply_out}\n${_apply_err}")
    endif()
    file(WRITE "${WAYVERB_PATCH_SENTINEL}" "applied\n")
    message(STATUS "[wayverb] assimp patch applied")
    return()
endif()

wayverb_git_apply_check(_out _err apply -R --check "${WAYVERB_PATCH_FILE}")

if(wayverb_git_last_result EQUAL 0)
    message(STATUS "[wayverb] assimp patch already applied")
    file(WRITE "${WAYVERB_PATCH_SENTINEL}" "already_applied\n")
    return()
endif()

message(FATAL_ERROR
        "Unable to apply assimp patch. git output:\n${_out}\n${_err}")
