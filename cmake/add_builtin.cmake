if(CMAKE_SCRIPT_MODE_FILE)
    message(STATUS "Available builtins:")
endif()

function(add_builtin)
    if (ARGC EQUAL 1)
        list(GET ARGN 0 SRC)
        cmake_path(GET SRC STEM NAME)
        cmake_path(GET SRC PARENT_PATH DIR)
        string(REPLACE "/" "::" NS ${DIR})
        set(NS ${NS}::${NAME})
        set(DEFAULT_ENABLE ON)
    else()
        cmake_parse_arguments(PARSE_ARGV 1 "" "DISABLED_BY_DEFAULT" "" "SRC;LINK_LIBS;INCLUDE_DIRS")
        list(GET ARGN 0 NS)
        set(SRC ${_SRC})
        set(LINK_LIBS ${_LINK_LIBS})
        set(INCLUDE_DIRS ${_INCLUDE_DIRS})
        if (_DISABLED_BY_DEFAULT)
            set(DEFAULT_ENABLE OFF)
        else()
            set(DEFAULT_ENABLE ON)
        endif()
    endif()
    string(REPLACE "-" "_" NS ${NS})
    string(REPLACE "::" "_" LIB_NAME ${NS})
    string(REGEX REPLACE "^builtins_" "" LIB_NAME ${LIB_NAME})
    set(LIB_NAME_NO_PREFIX ${LIB_NAME})
    string(PREPEND LIB_NAME "builtin_")
    string(TOUPPER ${LIB_NAME} LIB_NAME_UPPER)
    set(OPT_NAME ENABLE_${LIB_NAME_UPPER})
    set(DESCRIPTION "${LIB_NAME_NO_PREFIX} (option: ${OPT_NAME}, default: ${DEFAULT_ENABLE})")

    # In script-mode, just show the available builtins.
    if(CMAKE_SCRIPT_MODE_FILE)
        message(STATUS "  ${DESCRIPTION}")
        return()
    endif()

    option(${OPT_NAME} "Enable ${LIB_NAME}" ${DEFAULT_ENABLE})
    if (${${OPT_NAME}})
    else()
        message(STATUS "Skipping builtin ${DESCRIPTION}")
        return()
    endif()

    message(STATUS "Adding builtin ${DESCRIPTION}")

    add_library(${LIB_NAME} STATIC ${SRC})
    target_link_libraries(${LIB_NAME} PRIVATE spidermonkey extension_api ${LINK_LIBS})
    target_link_libraries(builtins PRIVATE ${LIB_NAME})
    target_include_directories(${LIB_NAME} PRIVATE ${INCLUDE_DIRS})
    file(APPEND $CACHE{INSTALL_BUILTINS} "NS_DEF(${NS})\n")
    return(PROPAGATE LIB_NAME)
endfunction()
