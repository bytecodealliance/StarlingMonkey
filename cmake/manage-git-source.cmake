# Function to manage git-based source dependencies with shallow cloning and tag management.
# TAG may be either a git tag name or a full 40-character commit hash.
function(manage_git_source)
    cmake_parse_arguments(
            GIT_SRC
            ""
            "NAME;REPO_URL;TAG;SOURCE_DIR"
            ""
            ${ARGN}
    )

    if(NOT DEFINED GIT_SRC_NAME OR NOT DEFINED GIT_SRC_REPO_URL OR NOT DEFINED GIT_SRC_TAG OR NOT DEFINED GIT_SRC_SOURCE_DIR)
        message(FATAL_ERROR "manage_git_source requires NAME, REPO_URL, TAG, and SOURCE_DIR arguments")
    endif()

    set(LOCK_FILE ${CMAKE_SOURCE_DIR}/deps/.${GIT_SRC_NAME}-clone.lock)

    # Use file locking to prevent concurrent clone operations
    file(LOCK ${LOCK_FILE} GUARD FUNCTION)

    # CMake's regex syntax has no bounded repetition, hence the length check.
    string(LENGTH "${GIT_SRC_TAG}" GIT_SRC_TAG_LENGTH)
    if(GIT_SRC_TAG_LENGTH EQUAL 40 AND GIT_SRC_TAG MATCHES "^[0-9a-f]+$")
        set(GIT_SRC_IS_COMMIT TRUE)
    else()
        set(GIT_SRC_IS_COMMIT FALSE)
    endif()

    # Check if source directory already exists and has the correct tag
    set(NEED_CLONE TRUE)
    set(NEED_CHECKOUT FALSE)

    if(EXISTS ${GIT_SRC_SOURCE_DIR}/.git)
        # Check current revision
        if(GIT_SRC_IS_COMMIT)
            execute_process(
                    COMMAND git -C ${GIT_SRC_SOURCE_DIR} rev-parse HEAD
                    OUTPUT_VARIABLE CURRENT_TAG
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE TAG_CHECK_RESULT
            )
        else()
            execute_process(
                    COMMAND git -C ${GIT_SRC_SOURCE_DIR} describe --tags --exact-match HEAD
                    OUTPUT_VARIABLE CURRENT_TAG
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE TAG_CHECK_RESULT
            )
        endif()

        if(TAG_CHECK_RESULT EQUAL 0 AND CURRENT_TAG STREQUAL ${GIT_SRC_TAG})
            set(NEED_CLONE FALSE)
            message(STATUS "${GIT_SRC_NAME} source already at correct tag ${GIT_SRC_TAG}")
        else()
            # Repository exists but wrong tag - fetch and checkout instead of re-cloning
            set(NEED_CLONE FALSE)
            set(NEED_CHECKOUT TRUE)
            message(STATUS "${GIT_SRC_NAME} source not at correct tag, checking out ${GIT_SRC_TAG}")
        endif()
    endif()

    if(NEED_CLONE)
        message(STATUS "Cloning ${GIT_SRC_NAME} source at tag ${GIT_SRC_TAG}")
        # Remove existing directory if it exists but isn't a git repo
        if(EXISTS ${GIT_SRC_SOURCE_DIR})
            file(REMOVE_RECURSE ${GIT_SRC_SOURCE_DIR})
        endif()

        if(GIT_SRC_IS_COMMIT)
            # `git clone --branch` only accepts branch and tag names, so a
            # commit is fetched into a fresh repository instead.
            file(MAKE_DIRECTORY ${GIT_SRC_SOURCE_DIR})
            execute_process(
                    COMMAND git -C ${GIT_SRC_SOURCE_DIR} init --quiet
                    COMMAND_ERROR_IS_FATAL ANY
            )
            execute_process(
                    COMMAND git -C ${GIT_SRC_SOURCE_DIR} remote add origin ${GIT_SRC_REPO_URL}
                    COMMAND_ERROR_IS_FATAL ANY
            )
            execute_process(
                    COMMAND git -C ${GIT_SRC_SOURCE_DIR} fetch --depth 1 origin ${GIT_SRC_TAG}
                    RESULT_VARIABLE CLONE_RESULT
                    ERROR_VARIABLE CLONE_ERROR
            )
            if(CLONE_RESULT EQUAL 0)
                execute_process(
                        COMMAND git -C ${GIT_SRC_SOURCE_DIR} checkout --detach FETCH_HEAD
                        RESULT_VARIABLE CLONE_RESULT
                        ERROR_VARIABLE CLONE_ERROR
                )
            endif()
        else()
            # Perform shallow clone of specific tag
            execute_process(
                    COMMAND git clone --depth 1 --branch ${GIT_SRC_TAG}
                    ${GIT_SRC_REPO_URL}
                    ${GIT_SRC_SOURCE_DIR}
                    RESULT_VARIABLE CLONE_RESULT
                    ERROR_VARIABLE CLONE_ERROR
            )
        endif()

        if(NOT CLONE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to clone ${GIT_SRC_NAME} source: ${CLONE_ERROR}")
        endif()
    elseif(NEED_CHECKOUT)
        # Check if the revision already exists locally
        if(GIT_SRC_IS_COMMIT)
            set(GIT_SRC_REF "${GIT_SRC_TAG}^{commit}")
        else()
            set(GIT_SRC_REF "refs/tags/${GIT_SRC_TAG}")
        endif()
        execute_process(
                COMMAND git -C ${GIT_SRC_SOURCE_DIR} rev-parse --verify --quiet "${GIT_SRC_REF}"
                OUTPUT_QUIET
                ERROR_QUIET
                RESULT_VARIABLE TAG_EXISTS_RESULT
        )

        if(NOT TAG_EXISTS_RESULT EQUAL 0)
            # Revision doesn't exist locally, fetch it
            message(STATUS "Fetching ${GIT_SRC_TAG}")
            if(GIT_SRC_IS_COMMIT)
                set(FETCH_REF ${GIT_SRC_TAG})
            else()
                set(FETCH_REF tag ${GIT_SRC_TAG})
            endif()
            execute_process(
                    COMMAND git -C ${GIT_SRC_SOURCE_DIR} fetch --depth 1 ${GIT_SRC_REPO_URL} ${FETCH_REF}
                    RESULT_VARIABLE FETCH_RESULT
                    ERROR_VARIABLE FETCH_ERROR
            )

            if(NOT FETCH_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to fetch ${GIT_SRC_TAG}: ${FETCH_ERROR}")
            endif()
        endif()

        # Checkout the revision (whether it was already local or just fetched)
        execute_process(
                COMMAND git -C ${GIT_SRC_SOURCE_DIR} checkout --detach ${GIT_SRC_TAG}
                RESULT_VARIABLE CHECKOUT_RESULT
                ERROR_VARIABLE CHECKOUT_ERROR
        )

        if(NOT CHECKOUT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to checkout ${GIT_SRC_TAG}: ${CHECKOUT_ERROR}")
        endif()
    endif()
endfunction()
