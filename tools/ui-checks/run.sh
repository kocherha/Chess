#!/usr/bin/env bash
# The game, driven by a pretend mouse: a few things a player does, and whether
# each had the effect it should.
#
#   tools/ui-checks/run.sh <the built Chess executable> [check ...]
#
# With no checks named, all of them run. Each starts the game afresh in a
# virtual X display, with settings of its own, clicks and drags with xdotool,
# and reads what happened from the game's event log (CHESS_EVENT_LOG=1, see
# game/Source/EventLog.h) — and, for the one check about going to sleep, from
# CHESS_IDLE_LOG. A picture of the window at the end of each is kept next to
# its log, in the folder printed at the end, for a person to look at.
#
# Linux only: it needs Xvfb, xdotool and ImageMagick's `import`, and Stockfish
# where the game finds it. The board and the buttons are found where they are
# in a window of the size the game opens at (1180 × 780); a change of layout
# means changing the coordinates below. These are the checks the bugs of the
# 2026-11-18 review were found and confirmed with, kept so that refactoring the
# scenes cannot quietly bring them back.

set -u

if [ $# -lt 1 ] || [ ! -x "$1" ]; then
    echo "usage: $0 <path to the built Chess executable> [check ...]" >&2
    exit 2
fi
for tool in Xvfb xdotool import; do
    command -v "$tool" >/dev/null || { echo "$tool is needed and was not found" >&2; exit 2; }
done

GAME="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
shift
OUT="$(mktemp -d /tmp/chess-ui-checks.XXXXXX)"
DISPLAY_NUMBER=77
export DISPLAY=":$DISPLAY_NUMBER"
if [ -e "/tmp/.X$DISPLAY_NUMBER-lock" ]; then
    echo "display $DISPLAY is taken (/tmp/.X$DISPLAY_NUMBER-lock); is another run going?" >&2
    exit 2
fi

APP=""
Xvfb "$DISPLAY" -screen 0 1400x900x24 >"$OUT/xvfb.log" 2>&1 &
XVFB=$!
trap '[ -n "$APP" ] && kill $APP 2>/dev/null; kill $XVFB 2>/dev/null' EXIT
sleep 1

# --- driving the game ------------------------------------------------------------

LOG=""

# launch <check> <game arguments...>: a fresh game with settings of its own.
launch() {
    local name=$1
    shift
    LOG="$OUT/$name.log"
    mkdir -p "$OUT/home-$name"
    # From its own folder: the game finds Content/ there. `exec`, so that
    # $APP is the game itself.
    (cd "$(dirname "$GAME")" &&
        exec env HOME="$OUT/home-$name" XDG_CONFIG_HOME="$OUT/home-$name/.config" CHESS_EVENT_LOG=1 \
            ${EXTRA_ENV:-} "$GAME" "$@" >"$LOG" 2>&1) &
    APP=$!
}

finish() {
    import -window root -crop 1180x780+0+0 "$OUT/$1.png" 2>/dev/null
    kill $APP 2>/dev/null
    wait $APP 2>/dev/null
    sleep 0.5
}

# wait_for <extended regex> <seconds>: until a line of the log matches.
wait_for() {
    local pattern=$1 seconds=$2 waited=0
    while ! grep -qE "$pattern" "$LOG" 2>/dev/null; do
        sleep 0.25
        waited=$((waited + 1))
        [ $waited -ge $((seconds * 4)) ] && return 1
    done
    return 0
}

# wait_for_count <extended regex> <how many> <seconds>: until that many lines match.
wait_for_count() {
    local pattern=$1 count=$2 seconds=$3 waited=0
    while [ "$(grep -cE "$pattern" "$LOG" 2>/dev/null)" -lt "$count" ]; do
        sleep 0.25
        waited=$((waited + 1))
        [ $waited -ge $((seconds * 4)) ] && return 1
    done
    return 0
}

# The centre of a square on screen, board not turned round: the game's board
# (a1 at 165,673, squares of 81) and the problems mode's (a1 at 175,645,
# squares of 73).
square() {
    local scene=$1 sq=$2
    local file=$(($(printf '%d' "'${sq:0:1}") - 97)) rank=$((${sq:1:1} - 1))
    if [ "$scene" = game ]; then
        echo "$((165 + 81 * file)) $((673 - 81 * rank))"
    else
        echo "$((175 + 73 * file)) $((645 - 73 * rank))"
    fi
}
click_at() { xdotool mousemove "$1" "$2" click 1; sleep 0.3; }
click_square() { click_at $(square "$1" "$2"); }

# The buttons, where they are in a window of 1180 × 780.
BUTTON_SOLVE="1020 740"     # editor: "Розв'язувати"
BUTTON_SOLUTION="1092 693"  # solving: "Розв'язок"

# --- the checks ------------------------------------------------------------------

PASSED=0
FAILED=0
pass() { echo "  pass  $1"; PASSED=$((PASSED + 1)); }
fail() { echo "  FAIL  $1: $2 (see $LOG)"; FAILED=$((FAILED + 1)); }

# A move made with two clicks is answered by the engine.
check_engine_replies() {
    launch engine_replies --play --level 1 --side white
    wait_for '^event: game ' 20 || { fail engine_replies "the game did not start"; finish engine_replies; return; }
    sleep 1
    click_square game e2
    click_square game e4
    if ! wait_for '^event: move e2e4' 5; then
        fail engine_replies "the click did not make the move"
    elif ! wait_for_count '^event: move ' 2 15; then
        fail engine_replies "the engine did not answer"
    else
        pass engine_replies
    fi
    finish engine_replies
}

# A piece is being carried when the move is taken back from the keyboard:
# letting go must not make a move in the position the board went back to.
check_undo_while_dragging() {
    launch undo_while_dragging --play --level 3 --side white --moves "e4 e5"
    wait_for '^event: game ' 20 || { fail undo_while_dragging "the game did not start"; finish undo_while_dragging; return; }
    sleep 2
    read -r x y <<<"$(square game g1)"
    xdotool mousemove "$x" "$y" mousedown 1; sleep 0.2
    xdotool mousemove "$x" $((y - 40)); sleep 0.2
    xdotool key BackSpace
    wait_for '^event: undo ' 3 || { fail undo_while_dragging "BackSpace did not take the move back"; finish undo_while_dragging; return; }
    read -r x y <<<"$(square game f3)"
    xdotool mousemove "$x" "$y"; sleep 0.3
    xdotool mouseup 1; sleep 2
    if sed -n '/^event: undo /,$p' "$LOG" | grep -q '^event: move '; then
        fail undo_while_dragging "a move was made after the take-back"
    else
        pass undo_while_dragging
    fi
    finish undo_while_dragging
}

# A dialog opened from the keyboard while the button is down takes the
# release; the game must still go to sleep afterwards.
check_sleep_after_dialog() {
    EXTRA_ENV="CHESS_IDLE_LOG=1" launch sleep_after_dialog --play --level 3 --side white --moves "e4 e5"
    wait_for '^event: game ' 20 || { fail sleep_after_dialog "the game did not start"; finish sleep_after_dialog; return; }
    sleep 2
    read -r x y <<<"$(square game g1)"
    xdotool mousemove "$x" "$y" mousedown 1; sleep 0.2
    xdotool mousemove "$x" $((y - 30)); sleep 0.2
    xdotool key n; sleep 0.5
    xdotool mouseup 1; sleep 4
    if grep '^idle: ' "$LOG" | tail -1 | grep -q 'asleep'; then
        pass sleep_after_dialog
    else
        fail sleep_after_dialog "still awake: $(grep '^idle: ' "$LOG" | tail -1)"
    fi
    finish sleep_after_dialog
}

# In the editor: a right click takes off the very piece being carried, and
# letting go of it over another square must leave that square as it was.
check_editor_right_click_while_dragging() {
    local name=editor_right_click_while_dragging
    launch $name --edit "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    wait_for '^event: setup ' 20 || { fail $name "the editor did not open"; finish $name; return; }
    sleep 1
    read -r x y <<<"$(square puzzle b1)"
    xdotool mousemove "$x" "$y" mousedown 1; sleep 0.2
    xdotool mousemove "$x" $((y - 35)); sleep 0.2
    xdotool mousemove "$x" $((y - 5)); sleep 0.2
    xdotool click 3; sleep 0.3
    read -r x y <<<"$(square puzzle c2)"
    xdotool mousemove "$x" "$y"; sleep 0.3
    xdotool mouseup 1; sleep 1
    local last expected="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R1BQKBNR w KQkq - 0 1"
    last=$(grep '^event: setup ' "$LOG" | tail -1 | cut -d' ' -f3-)
    if [ "$last" = "$expected" ]; then
        pass $name
    else
        fail $name "the position is $last"
    fi
    finish $name
}

# A best-move problem, solved (or not), then "Розв'язок": the engine shows
# the move.
check_solution_of_a_best_move_problem() {
    local name=solution_of_a_best_move_problem
    launch $name --edit "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    wait_for '^event: setup ' 20 || { fail $name "the editor did not open"; finish $name; return; }
    sleep 6  # the engine looks at the position before "solve" can be pressed
    click_at $BUTTON_SOLVE
    wait_for '^event: puzzle open .* mate 0' 5 || { fail $name "the problem did not open"; finish $name; return; }
    sleep 4  # the engine finds the best move first
    click_square puzzle e2
    click_square puzzle e4
    if ! wait_for '^event: puzzle (finished player|wrong)' 10; then
        fail $name "the move was not judged"
        finish $name
        return
    fi
    click_at $BUTTON_SOLUTION
    if wait_for '^event: puzzle finished solution' 15; then
        pass $name
    else
        fail $name "the solution was not shown"
    fi
    finish $name
}

# Mate in one from the collection, solved with two clicks.
check_mate_in_one() {
    launch mate_in_one --puzzle m1-back-rank
    wait_for '^event: puzzle open m1-back-rank' 20 || { fail mate_in_one "the problem did not open"; finish mate_in_one; return; }
    sleep 1
    click_square puzzle e1
    click_square puzzle e8
    if wait_for '^event: puzzle finished player$' 5; then
        pass mate_in_one
    else
        fail mate_in_one "not solved"
    fi
    finish mate_in_one
}

# A wrong move in a problem is called wrong and taken back by itself.
check_wrong_move_is_taken_back() {
    local name=wrong_move_is_taken_back
    launch $name --puzzle m1-back-rank
    wait_for '^event: puzzle open m1-back-rank' 20 || { fail $name "the problem did not open"; finish $name; return; }
    sleep 1
    click_square puzzle g1
    click_square puzzle h1
    if ! wait_for '^event: puzzle wrong' 10; then
        fail $name "the move was not called wrong"
    elif ! wait_for '^event: puzzle takeback wrong' 5; then
        fail $name "the move was not taken back"
    else
        pass $name
    fi
    finish $name
}

# --- what the game asks of its engines (ADR-002: the opponent and the watcher) ---

# A hint asked for at once, before the watching engine has finished with the
# position, comes when it has.
check_hint_before_the_analysis() {
    local name=hint_before_the_analysis
    launch $name --play --level 1 --side white
    wait_for '^event: game ' 20 || { fail $name "the game did not start"; finish $name; return; }
    xdotool key h
    if wait_for '^event: hint [a-h][1-8][a-h][1-8]' 30; then
        pass $name
    else
        fail $name "no hint came"
    fi
    finish $name
}

# A move taken back while the opponent is still thinking about it: its
# answer, when it comes, is to a position that is gone, and is not played.
check_undo_while_the_opponent_thinks() {
    local name=undo_while_the_opponent_thinks
    launch $name --play --level 8 --side white
    wait_for '^event: game ' 20 || { fail $name "the game did not start"; finish $name; return; }
    sleep 1
    click_square game e2
    click_square game e4
    wait_for '^event: move e2e4' 5 || { fail $name "the click did not make the move"; finish $name; return; }
    sleep 0.3
    xdotool key u
    if ! wait_for '^event: undo ' 5; then
        fail $name "the move was not taken back"
    else
        sleep 4  # longer than level 8 thinks
        if [ "$(grep -c '^event: move ' "$LOG")" -ne 1 ]; then
            fail $name "the opponent played into the position that was taken back"
        else
            pass $name
        fi
    fi
    finish $name
}

# The same move played again after taking it back gets the same answer: a
# weakened engine rolls dice, and "take back" must not double as "roll again".
check_same_answer_after_undo() {
    local name=same_answer_after_undo
    launch $name --play --level 1 --side white
    wait_for '^event: game ' 20 || { fail $name "the game did not start"; finish $name; return; }
    sleep 1
    click_square game e2
    click_square game e4
    wait_for_count '^event: move ' 2 15 || { fail $name "the opponent did not answer"; finish $name; return; }
    xdotool key u
    wait_for '^event: undo ' 5 || { fail $name "the move was not taken back"; finish $name; return; }
    sleep 1
    click_square game e2
    click_square game e4
    wait_for_count '^event: move ' 4 15 || { fail $name "the opponent did not answer again"; finish $name; return; }
    local first second
    first=$(grep '^event: move ' "$LOG" | sed -n 2p)
    second=$(grep '^event: move ' "$LOG" | sed -n 4p)
    if [ "$first" = "$second" ]; then
        pass $name
    else
        fail $name "first $first, then $second"
    fi
    finish $name
}

# A whole game against level 1, the person's moves made at random: every kind
# of move and ending, the random replies below the engine's floor, and the
# game ends on its own (--autoplay exits when it is done).
check_a_whole_game_at_level_one() {
    local name=a_whole_game_at_level_one
    launch $name --play --level 1 --side white --autoplay 1
    if wait_for 'autoplay: ' 300; then
        pass $name
    else
        fail $name "the game did not come to an end"
    fi
    finish $name
}

# For a person to look at: a game set up at move 23 numbers its moves from 23.
check_numbering_picture() {
    launch numbering_picture --play --two-players \
        --fen "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 2 23" --moves "Nf6 Nc3 Bc5 Bc4 d6"
    wait_for '^event: game ' 20
    sleep 3
    echo "  look  numbering_picture: the list should run 23., 24., 25. ($OUT/numbering_picture.png)"
    finish numbering_picture
}

ALL="engine_replies undo_while_dragging sleep_after_dialog editor_right_click_while_dragging
     solution_of_a_best_move_problem mate_in_one wrong_move_is_taken_back
     hint_before_the_analysis undo_while_the_opponent_thinks same_answer_after_undo a_whole_game_at_level_one
     numbering_picture"
for check in ${@:-$ALL}; do
    if ! declare -F "check_$check" >/dev/null; then
        echo "  no check called $check" >&2
        FAILED=$((FAILED + 1))
        continue
    fi
    "check_$check"
done

echo "$PASSED passed, $FAILED failed; logs and pictures in $OUT"
[ $FAILED -eq 0 ]
