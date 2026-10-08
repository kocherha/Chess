# The rules of chess: positions, legal moves, notation, and how a game ends.
# Knows nothing about windows, engines or threads, and depends on nothing but
# the standard library.
file(GLOB CHESS_RULES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(chess_rules STATIC ${CHESS_RULES_SOURCES})
target_include_directories(chess_rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(chess_rules PUBLIC cxx_std_20)
chess_sanitize(chess_rules)
