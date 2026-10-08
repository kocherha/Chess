# Shared by both entry points: the build at the repository root and the axmol
# application under game/. Neither can assume it is the top-level project, so
# nothing here uses CMAKE_SOURCE_DIR.

get_filename_component(CHESS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Off for an environment prepared in advance (CI, no network): nothing is
# downloaded, and whatever is missing is an error that says what to fetch.
option(CHESS_BOOTSTRAP "Fetch what is missing from third-party/: dependencies and axmol's shader compiler" ON)

# AddressSanitizer and UBSan for our own code — the rules, the engine client,
# their tests and the game's sources — without rebuilding axmol under them.
option(CHESS_SANITIZE "Build this project's own code with ASan and UBSan" OFF)

function(chess_sanitize target)
    if(CHESS_SANITIZE AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        # PUBLIC: whatever links this needs the sanitizer runtime too.
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endif()
endfunction()

# --- third-party --------------------------------------------------------------
# Everything this project depends on lives in third-party/, pinned to a tag:
# nothing is borrowed from whatever else happens to be checked out next door,
# and nothing is installed into the system.
#
#   third-party/axmol        the UI engine
#   third-party/Stockfish    the opponent
#   third-party/googletest   the tests only
#
# In a git checkout these are submodules, and the commit each one is at is part
# of this repository's history. The table below says the same thing for a copy
# of the sources that is not a git checkout (an archive, a folder not yet under
# version control): there the first configure clones each one at its tag. Keep
# the two in step when a dependency is moved to a new version.
set(CHESS_AXMOL_URL      "https://github.com/axmolengine/axmol.git")
set(CHESS_AXMOL_TAG      "v2.9.1")
set(CHESS_STOCKFISH_URL  "https://github.com/official-stockfish/Stockfish.git")
set(CHESS_STOCKFISH_TAG  "stockfish-dev-20260930-49ea5ded")
set(CHESS_GOOGLETEST_URL "https://github.com/google/googletest.git")
set(CHESS_GOOGLETEST_TAG "v1.18.0")

# Makes sure third-party/<name> is there, fetching it if it is not — that one
# only, the first time something needs it: building the rules and their tests
# does not download a graphics engine. `sentinel` is a file that exists once
# the dependency does.
function(chess_require_dependency name url tag sentinel)
    set(path "third-party/${name}")
    if(EXISTS "${CHESS_ROOT}/${path}/${sentinel}")
        return()
    endif()

    if(NOT CHESS_BOOTSTRAP)
        message(FATAL_ERROR
            "${path} is missing and CHESS_BOOTSTRAP is off.\n"
            "Run:  git submodule update --init --recursive\n"
            "or:   git clone --depth 1 --branch ${tag} ${url} ${path}")
    endif()
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR "${path} is missing and git was not found to fetch it with.")
    endif()

    # Registered as a submodule here? Then git already knows which commit.
    set(entry "")
    if(EXISTS "${CHESS_ROOT}/.git")
        execute_process(COMMAND "${GIT_EXECUTABLE}" ls-files --stage -- "${path}"
                        WORKING_DIRECTORY "${CHESS_ROOT}" OUTPUT_VARIABLE entry ERROR_QUIET)
    endif()

    if(entry MATCHES "^160000 ")
        message(STATUS "fetching submodule ${path} (once)")
        execute_process(COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive -- "${path}"
                        WORKING_DIRECTORY "${CHESS_ROOT}" RESULT_VARIABLE result)
    else()
        message(STATUS "cloning ${name} ${tag} into ${path} (once)")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -c advice.detachedHead=false clone --depth 1
                                --branch "${tag}" "${url}" "${CHESS_ROOT}/${path}"
                        RESULT_VARIABLE result)
    endif()

    if(NOT result EQUAL 0 OR NOT EXISTS "${CHESS_ROOT}/${path}/${sentinel}")
        message(FATAL_ERROR "fetching ${path} failed (${result}).\n"
                            "By hand:  git clone --depth 1 --branch ${tag} ${url} ${path}")
    endif()
endfunction()

# --- the rules and the engine client ----------------------------------------
# Safe to call from either project; the guard lets the app include it without
# caring whether the root already did.
function(chess_add_engine)
    if(TARGET chess_rules)
        return()
    endif()
    add_subdirectory(${CHESS_ROOT}/engine/rules ${CMAKE_BINARY_DIR}/engine/rules)
    add_subdirectory(${CHESS_ROOT}/engine/uci ${CMAKE_BINARY_DIR}/engine/uci)
endfunction()

# --- axmol -------------------------------------------------------------------
# The UI engine is third-party/axmol. -DAX_ROOT=... builds against an engine
# somewhere else instead.
#
# The AX_ROOT *environment* variable is deliberately not consulted. axmol's own
# setup script writes it into the shell profile of whoever runs it, so on a
# machine with another axmol project it points at that project's engine — and a
# build that trusted it would quietly compile against a neighbour's checkout,
# which is exactly what keeping dependencies in third-party/ is meant to rule
# out. axmol's CMake modules do read the variable, so it is set here, for the
# duration of this configure only.
function(chess_locate_axmol)
    # (Compared against the empty string on purpose: if("some/path") is false
    # in CMake, so a bare truth test would ignore the variable.)
    if(DEFINED AX_ROOT AND NOT "${AX_ROOT}" STREQUAL "")
        get_filename_component(root "${AX_ROOT}" ABSOLUTE)
        if(NOT EXISTS "${root}/core/CMakeLists.txt")
            message(FATAL_ERROR "AX_ROOT does not look like axmol: ${root}")
        endif()
    else()
        chess_require_dependency(axmol "${CHESS_AXMOL_URL}" "${CHESS_AXMOL_TAG}" core/CMakeLists.txt)
        set(root "${CHESS_ROOT}/third-party/axmol")
    endif()
    set(ENV{AX_ROOT} "${root}")
    message(STATUS "axmol: ${root}")

    # A build directory remembers where the engine was: dozens of cached paths
    # to its headers, its shader compiler and the libraries it ships. Pointed
    # at an engine somewhere else, it would go on using half of the old one —
    # quietly, until something does not link. So refuse, and say what to do.
    # (ax_libs is the name of axmol's core project; its source directory is in
    # the cache of any build that has configured the engine before.)
    get_filename_component(now "$ENV{AX_ROOT}/core" REALPATH)
    if(DEFINED CACHE{ax_libs_SOURCE_DIR})
        get_filename_component(before "$CACHE{ax_libs_SOURCE_DIR}" REALPATH)
        if(NOT before STREQUAL now)
            message(FATAL_ERROR
                "This build directory was configured against the axmol in\n"
                "  ${before}\n"
                "and the engine is now\n"
                "  ${now}\n"
                "A build directory cannot follow the engine to a new place. "
                "Delete it and configure again:\n"
                "  rm -rf \"${CMAKE_BINARY_DIR}\"")
        endif()
    endif()
endfunction()

# Two things axmol needs that are not in its repository.
#
# PowerShell: its build helpers run .ps1 scripts on every build, not only the
# first. That is a tool for the machine, like the compiler, so it is asked for
# rather than installed from here.
#
# axslcc, its shader compiler: a prebuilt binary that axmol's CMake expects in
# $AX_ROOT/tools/external/axslcc and stops without. axmol's own setup.ps1 would
# fetch it — and would also rewrite AX_ROOT in the user's shell profiles and
# launchd environment, install a CMake of its own and offer to install system
# packages. All this project needs from that is the one download, so the one
# download is what it does: the version axmol's build profile asks for, from
# the release page axmol's tooling uses, into the place axmol looks.
function(chess_setup_axmol root)
    find_program(CHESS_PWSH NAMES pwsh powershell)
    if(NOT CHESS_PWSH)
        message(FATAL_ERROR
            "axmol's build scripts need PowerShell (pwsh), and it was not found.\n"
            "Install it (macOS: brew install powershell; others: https://aka.ms/powershell) "
            "and configure again, or pass -DCHESS_BUILD_APP=OFF to build the rules and "
            "their tests alone.")
    endif()

    set(directory "${root}/tools/external/axslcc")
    set(axslcc "${directory}/axslcc")
    if(CMAKE_HOST_WIN32)
        string(APPEND axslcc ".exe")
    endif()
    if(EXISTS "${axslcc}")
        return()
    endif()
    if(NOT CHESS_BOOTSTRAP)
        message(FATAL_ERROR "axmol's shader compiler is missing and CHESS_BOOTSTRAP is off.\n"
                            "Expected: ${axslcc}")
    endif()

    # "axslcc=1.9.6+" in the build profile means that version or newer; the
    # version named is the one axmol was released against.
    file(STRINGS "${root}/1k/build.profiles" profile REGEX "^axslcc=")
    string(REGEX MATCH "[0-9]+(\\.[0-9]+)+" version "${profile}")
    if(NOT version)
        message(FATAL_ERROR "could not read the axslcc version from ${root}/1k/build.profiles")
    endif()

    if(CMAKE_HOST_WIN32)
        set(package "win64.zip")
    elseif(CMAKE_HOST_APPLE)
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(package "osx-arm64.tar.gz")
        else()
            set(package "osx-x64.tar.gz")
        endif()
    else()
        set(package "linux.tar.gz")
    endif()

    set(url "https://github.com/axmolengine/axslcc/releases/download/v${version}/axslcc-${version}-${package}")
    set(archive "${root}/cache/axslcc-${version}-${package}")
    message(STATUS "axmol: fetching its shader compiler, axslcc ${version} (once)")
    file(DOWNLOAD "${url}" "${archive}" STATUS status TLS_VERIFY ON)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${archive}")
        message(FATAL_ERROR "downloading ${url} failed: ${status}\n"
                            "Put the axslcc binary from that archive into ${directory} by hand.")
    endif()

    file(MAKE_DIRECTORY "${directory}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${directory}")
    if(NOT EXISTS "${axslcc}")
        message(FATAL_ERROR "${archive} did not contain axslcc; expected ${axslcc}")
    endif()
endfunction()

# --- Stockfish ----------------------------------------------------------------
# The opponent is a separate program: the game starts it and talks to it over
# UCI, exactly as any chess GUI does. All the build has to do is make sure a
# binary exists and put it next to ours.
#
# It is built from third-party/Stockfish, in place, by Stockfish's own Makefile
# — that is the only build it supports, and the only one that knows how to
# fetch the network the engine evaluates with (about 100 MB, once).
# Everything it produces there — objects, the binary, the network — is covered
# by Stockfish's own .gitignore, so its checkout stays clean.
#
#   -DCHESS_STOCKFISH_BINARY=/path/to/stockfish   use this one, build nothing
set(CHESS_STOCKFISH_BINARY "" CACHE FILEPATH
    "A ready Stockfish (or any UCI engine) binary to bundle instead of building one")

# Where the engine binary is, or will be once it has been built. Asking does
# not fetch anything.
function(chess_stockfish_path out)
    if(CHESS_STOCKFISH_BINARY)
        set(${out} "${CHESS_STOCKFISH_BINARY}" PARENT_SCOPE)
    else()
        set(${out} "${CHESS_ROOT}/third-party/Stockfish/src/stockfish${CMAKE_EXECUTABLE_SUFFIX}" PARENT_SCOPE)
    endif()
endfunction()

# Makes sure the engine exists by the time the game is built. Sets
# CHESS_ENGINE_BINARY in the caller's scope (empty when there is none) and,
# when the binary is produced by this build, defines the target `stockfish`.
function(chess_add_stockfish)
    set(CHESS_ENGINE_BINARY "" PARENT_SCOPE)

    if(CHESS_STOCKFISH_BINARY)
        if(NOT EXISTS "${CHESS_STOCKFISH_BINARY}")
            message(FATAL_ERROR "CHESS_STOCKFISH_BINARY does not exist: ${CHESS_STOCKFISH_BINARY}")
        endif()
        message(STATUS "stockfish: using ${CHESS_STOCKFISH_BINARY}")
        set(CHESS_ENGINE_BINARY "${CHESS_STOCKFISH_BINARY}" PARENT_SCOPE)
        return()
    endif()

    find_program(CHESS_MAKE NAMES gmake make)
    if(WIN32 OR NOT CHESS_MAKE)
        message(WARNING
            "Stockfish cannot be built here (it needs make and a POSIX shell).\n"
            "The game will still build, but only the two-player mode will work until "
            "an engine is available: pass -DCHESS_STOCKFISH_BINARY=..., or set "
            "CHESS_ENGINE at run time.")
        return()
    endif()

    chess_require_dependency(Stockfish "${CHESS_STOCKFISH_URL}" "${CHESS_STOCKFISH_TAG}" src/Makefile)
    set(source "${CHESS_ROOT}/third-party/Stockfish/src")
    chess_stockfish_path(output)

    include(ProcessorCount)
    ProcessorCount(jobs)
    if(jobs EQUAL 0)
        set(jobs 2)
    endif()

    # The Makefile picks the architecture itself (ARCH=native). The compiler it
    # has to be told on a Mac, where its default "gcc" is really clang wearing
    # a different name.
    set(flags "")
    if(APPLE)
        list(APPEND flags COMP=clang)
    endif()

    # Only what is under version control is a dependency — not the objects or
    # the network the build itself leaves beside the sources.
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${source}/*.cpp" "${source}/*.h" "${source}/Makefile")

    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CHESS_MAKE}" -C "${source}" -j${jobs} build ${flags}
        DEPENDS ${sources}
        COMMENT "building Stockfish in third-party/Stockfish (a few minutes, once)"
        VERBATIM)
    add_custom_target(stockfish DEPENDS "${output}")

    message(STATUS "stockfish: built from third-party/Stockfish")
    set(CHESS_ENGINE_BINARY "${output}" PARENT_SCOPE)
endfunction()
