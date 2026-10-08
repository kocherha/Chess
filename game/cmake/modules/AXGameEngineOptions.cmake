# Which parts of axmol this game is built with.
#
# Chess draws flat shapes, sprites and text, so most of the engine is dead
# weight here: Lua, Spine, FairyGUI, ImGui, two physics engines, the 3D
# pipeline, audio, video and the network clients. Leaving them out takes the
# first build from "a quarter of an hour" to a few minutes and the binary to a
# fraction of its size.
#
# Each of these is one of axmol's own documented switches. If a trimmed engine
# ever fails to build on some platform, -DCHESS_FULL_ENGINE=ON goes back to the
# engine's defaults.
option(CHESS_FULL_ENGINE "Build axmol with its default feature set instead of the trimmed one" OFF)

if(NOT CHESS_FULL_ENGINE)
  # Extensions: everything that follows AX_EXT_HINT (Lua, GUI, Spine,
  # DragonBones, cocostudio, FairyGUI, ImGui, asset manager, ...).
  set(AX_EXT_HINT OFF CACHE BOOL "The default extensions hint" FORCE)

  set(AX_ENABLE_3D OFF CACHE BOOL "Build 3D support" FORCE)
  set(AX_ENABLE_PHYSICS OFF CACHE BOOL "Build Physics support" FORCE)
  set(AX_ENABLE_AUDIO OFF CACHE BOOL "Build audio support" FORCE)
  set(AX_ENABLE_OPUS OFF CACHE BOOL "Build with opus support" FORCE)
  set(AX_ENABLE_MEDIA OFF CACHE BOOL "Build media support" FORCE)
  set(AX_ENABLE_HTTP OFF CACHE BOOL "Build HTTP client based on yasio" FORCE)
  set(AX_ENABLE_WEBSOCKET OFF CACHE BOOL "Build Websocket client based on yasio" FORCE)
endif()
