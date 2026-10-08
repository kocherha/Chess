# The playable shell.
#
# A separate project from the repository root on purpose: axmol's build wants to
# be the top-level project, and nothing in the rules needs it. Nothing else is
# required — no setup script, no environment variables:
#
#   cmake -S game -B build/game
#   cmake --build build/game --target run
#
# Configuring fetches what is missing from third-party/ — axmol, Stockfish and
# axmol's shader compiler — at the versions pinned in cmake/Chess.cmake. Pass
# -DCHESS_BOOTSTRAP=OFF to require a prepared checkout instead, and
# -DAX_ROOT=... to build against an engine that lives elsewhere. The AX_ROOT
# environment variable is ignored, and axmol's setup.ps1 is never run: see
# chess_locate_axmol and chess_setup_axmol for why.

cmake_minimum_required(VERSION 3.22...4.1)

set(APP_NAME Chess)
project(${APP_NAME})

if(XCODE)
  set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY TRUE)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/Chess.cmake)

chess_locate_axmol()
chess_setup_axmol("$ENV{AX_ROOT}")

set(_is_axmol_embed FALSE)
set(_AX_USE_PREBUILT FALSE)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# The next three blocks are toolchain workarounds for axmol 2.9.1, each of them
# found the hard way.

# axmol's Objective-C++ sources are written for manual reference counting and
# then use `__weak` in places, which normally requires ARC. Clang supports weak
# references under MRC only with -fobjc-weak, and does not enable that by
# default. Xcode does (CLANG_ENABLE_OBJC_WEAK), which is why the engine builds
# for everyone using the Xcode generator and fails under Ninja or Makefiles.
if(APPLE)
  add_compile_options($<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fobjc-weak>)
endif()

# Apple's SDK headers are written for Apple's compiler: CF_ENUM expands to a
# forward declaration of an enum with a fixed underlying type, which upstream
# clang (a Homebrew LLVM, say) rejects with -Welaborated-enum-base. AppleClang
# is a separate compiler id from Clang, so this only fires where it is needed.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(
    $<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-elaborated-enum-base>)
endif()

# The fmt bundled with axmol 2.9.1 (11.2.0) defines
# _LIBCPP_REMOVE_TRANSITIVE_INCLUDES and then uses malloc and free without
# including <cstdlib>. On a new enough libc++ that no longer compiles. Supply
# the header the way the compiler would have; SHELL: keeps the two words
# together as one option.
if(NOT MSVC)
  add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:SHELL:-include cstdlib>")
endif()

# NOTE: the order of these includes matters; this is axmol's own sequence.
include(AXGameEngineOptions)
include(AXGameEngineSetup)

# --- our own libraries ------------------------------------------------------
chess_add_engine()

# --- the opponent -----------------------------------------------------------
chess_add_stockfish()

# --- the app itself ---------------------------------------------------------
# CONFIGURE_DEPENDS: a new file in Source/ re-runs the glob on the next build.
file(GLOB_RECURSE GAME_HEADER CONFIGURE_DEPENDS Source/*.h Source/*.hpp)
file(GLOB_RECURSE GAME_SOURCE CONFIGURE_DEPENDS Source/*.cpp Source/*.c)

set(GAME_INC_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# The piece sets. On macOS they go into the bundle's Resources; elsewhere the
# folder is linked next to the executable as Content/ (AXGameFinalSetup).
set(content_folder "${CMAKE_CURRENT_SOURCE_DIR}/Content")

# axmol lists the folder's files once, when the project is configured, and a
# bundle is built from that list. Watching the folder as well means a set of
# pieces added later is noticed by the next build instead of silently missing
# from the app.
file(GLOB_RECURSE chess_content_watch CONFIGURE_DEPENDS "${content_folder}/*")

if(APPLE)
  ax_mark_multi_resources(common_content_files RES_TO "Resources" FOLDERS ${content_folder})
elseif(WINDOWS)
  ax_mark_multi_resources(common_content_files RES_TO "Content" FOLDERS ${content_folder})
endif()

include(AXGameSourceSetup)

set(APP_SOURCES ${GAME_HEADER} ${GAME_SOURCE})

include(AXGameTargetSetup)

ax_setup_app_config(${APP_NAME})

target_link_libraries(${APP_NAME} chess_rules chess_uci)
chess_sanitize(${APP_NAME})

# The template's Info.plist takes the bundle id from an Xcode build setting,
# which only exists under the Xcode generator. Setting it here as well means
# Ninja and Makefile builds produce a bundle with a real identifier.
if(APPLE)
  set_target_properties(${APP_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.chess.app"
    MACOSX_BUNDLE_BUNDLE_NAME    "Chess"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
  )
endif()

include(AXGamePlatformSetup)
include(AXGameFinalSetup)

# --- bundling the engine ----------------------------------------------------
# Stockfish goes next to our executable — inside Chess.app/Contents/MacOS on a
# Mac — which is the first place the game looks for it. The path it came from
# is compiled in as a fallback, so a binary run from an unusual place (a
# debugger, a copied executable) still finds its opponent.
if(CHESS_ENGINE_BINARY)
  if(TARGET stockfish)
    add_dependencies(${APP_NAME} stockfish)
  endif()
  add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CHESS_ENGINE_BINARY}" "$<TARGET_FILE_DIR:${APP_NAME}>/stockfish${CMAKE_EXECUTABLE_SUFFIX}"
    COMMENT "bundling Stockfish"
    VERBATIM)
  target_compile_definitions(${APP_NAME} PRIVATE
    CHESS_ENGINE_FALLBACK="${CHESS_ENGINE_BINARY}")
endif()

# --- running it -------------------------------------------------------------
# `cmake --build . --target run` beats remembering where inside a .app bundle
# the executable ended up. USES_TERMINAL keeps the app's own output attached to
# the console instead of being swallowed by the build tool.
add_custom_target(run
  COMMAND "$<TARGET_FILE:${APP_NAME}>"
  DEPENDS ${APP_NAME}
  USES_TERMINAL
  COMMENT "starting Chess")

# One frame to a PNG and out. For build machines with no screen.
add_custom_target(shot
  COMMAND "$<TARGET_FILE:${APP_NAME}>" --shot "${CMAKE_BINARY_DIR}/chess.png"
  DEPENDS ${APP_NAME}
  USES_TERMINAL
  COMMENT "rendering one frame to ${CMAKE_BINARY_DIR}/chess.png")
