# Talking to a UCI chess engine (Stockfish) running as a separate process, and
# the table of difficulty levels built on top of its strength options.
# No dependency on the rules library or on axmol: moves cross this boundary as
# plain UCI strings.
file(GLOB CHESS_UCI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

find_package(Threads REQUIRED)

add_library(chess_uci STATIC ${CHESS_UCI_SOURCES})
target_include_directories(chess_uci PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(chess_uci PUBLIC cxx_std_20)
target_link_libraries(chess_uci PUBLIC Threads::Threads)
chess_sanitize(chess_uci)
