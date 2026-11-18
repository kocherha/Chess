// How long the computer seems to take over a reply that needed no thinking.
#pragma once

namespace chess {

// A reply that is already known — the defence in a problem, a move the
// opponent made here before, one picked at random below the engine's
// weakest setting — is still played after this long, in seconds. At once,
// it would land the instant the player let go of their own piece, and read
// as a glitch rather than as an answer.
inline constexpr float kReplyPause = 0.45f;

}  // namespace chess
