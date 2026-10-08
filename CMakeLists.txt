# Everything: the rules, the Stockfish client, their tests, and the game.
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j                 # all of it
#   ctest --test-dir build                 # the tests
#   cmake --build build --target run       # the game
#
# game/ is also a standalone project — axmol expects to be the top-level build,
# and that still works — but building from here builds it too.
#
#   -DCHESS_BUILD_APP=OFF     rules, engine client and tests only, no graphics
#   -DCHESS_BUILD_TESTS=OFF   the other way round
cmake_minimum_required(VERSION 3.22...4.1)
project(Chess LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Chess.cmake)

option(CHESS_BUILD_TESTS "Build the unit tests" ON)
option(CHESS_BUILD_APP   "Build the axmol application" ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

chess_add_engine()

if(CHESS_BUILD_TESTS)
    enable_testing()

    # GoogleTest, found on the system or taken from third-party/.
    # Resolved once here so every test directory just links GTest::gtest_main.
    find_package(GTest 1.14 QUIET)
    if(NOT GTest_FOUND)
        chess_require_dependency(googletest "${CHESS_GOOGLETEST_URL}" "${CHESS_GOOGLETEST_TAG}"
                                 CMakeLists.txt)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        # Our warning flags are ours, not googletest's. Directory properties are
        # copied into a subdirectory when it is added, so clearing them here and
        # putting them back afterwards keeps third-party code quiet without
        # letting our own tests off the hook.
        get_directory_property(CHESS_WARNINGS COMPILE_OPTIONS)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
        add_subdirectory(third-party/googletest ${CMAKE_BINARY_DIR}/third-party/googletest
                         EXCLUDE_FROM_ALL)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "${CHESS_WARNINGS}")

        foreach(gtest_target gtest gtest_main)
            target_include_directories(${gtest_target} SYSTEM PUBLIC
                ${CHESS_ROOT}/third-party/googletest/googletest/include)
        endforeach()
        message(STATUS "tests: using googletest from third-party/googletest")
    endif()

    include(GoogleTest)

    # The engine tests want to know where the real Stockfish is (or will be).
    chess_stockfish_path(CHESS_REAL_ENGINE)

    add_subdirectory(engine/rules/tests)
    add_subdirectory(engine/uci/tests)
endif()

if(CHESS_BUILD_APP)
    # Same treatment as googletest: axmol is a large third-party codebase and
    # our warning flags are ours. It also sets compiler options of its own
    # (-fno-char8_t among them) which stay inside this subtree.
    get_directory_property(CHESS_WARNINGS COMPILE_OPTIONS)
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
    add_subdirectory(game)
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "${CHESS_WARNINGS}")
endif()
