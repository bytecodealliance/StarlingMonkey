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
        cmake_parse_arguments(PARSE_ARGV 1 "" "DISABLED_BY_DEFAULT" "" "SRC;INCLUDE_DIRS;DEPENDENCIES")
        list(GET ARGN 0 NS)
        set(SRC ${_SRC})
        set(INCLUDE_DIRS ${_INCLUDE_DIRS})
        set(DEPENDENCIES ${_DEPENDENCIES})
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

    if (DEPENDENCIES)
        message(VERBOSE "${LIB_NAME} depends on ${DEPENDENCIES}")

        add_dependencies(${LIB_NAME} ${DEPENDENCIES})

        foreach(DEPENDENCY IN ITEMS ${DEPENDENCIES})
            get_target_property(TYPE ${DEPENDENCY} TYPE)
            if(NOT ${TYPE} MATCHES "UTILITY")
                # A built-in might specify a speficic dependency
                # (e.g. "OpenSSL::Crypto").  Ensure that if this dependency
                # is of the right type (i.e. libraries), this particular
                # built-in is linked against this library.
                # (CMake will take care of building e.g. "OpenSSL" first.)
                target_link_libraries(${LIB_NAME} PRIVATE ${DEPENDENCY})
            endif()

            # If a dependency has the format "PackageName::SubDependency",
            # e.g. "OpenSSL::Crypto", we need to grab only the PackageName
            # to ensure that the target finds the libraries includes in the
            # right place (in the next step).
            string(FIND "${DEPENDENCY}" "::" COLON_COLON)
            if(COLON_COLON GREATER_EQUAL 0)
                string(SUBSTRING "${DEPENDENCY}" 0 COLON_COLON DEPENDENCY)
            endif()

            # Only linking against a library isn't sufficient to build this
            # built-in:  the compiler also needs to find the include files
            # that this dependency brought.  CMake doesn't do this automatically,
            # so give it a hand.
            #
            # Not all dependencies will have their include files in this
            # location, but we can't, at this point, check if it exists,
            # because this code runs at configure time and this directory
            # will be created during compilation time.  This, however,
            # doesn't cause compilation issues.
            target_include_directories(${LIB_NAME}
                       PRIVATE ${CMAKE_BINARY_DIR}/deps/${DEPENDENCY}/include)
        endforeach()
    endif ()

    target_link_libraries(${LIB_NAME} PRIVATE spidermonkey extension_api)
    target_link_libraries(builtins PRIVATE ${LIB_NAME})
    target_include_directories(${LIB_NAME} PRIVATE ${INCLUDE_DIRS})

    file(APPEND $CACHE{INSTALL_BUILTINS} "NS_DEF(${NS})\n")
    return(PROPAGATE LIB_NAME)
endfunction()
