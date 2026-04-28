# TODO, FIXME, HACK, and XXX List

This file contains all TODO, FIXME, HACK, and XXX comments found in the project.
Generated automatically from code comments.

## Summary

**Total entries:** 465 (code comments) + 7 (compiler warnings) + 2 (v9.9 bugs fixed) + 2 (v9.11 bugs fixed)

| Type | Count |
|------|-------|
| FIXME | 149 |
| HACK | 22 |
| TODO | 294 |
| WARNING | 7 (Luanti-Secure-specific compiler warnings from CI) |
| BUG FIX | 2 (v9.9 build and runtime bugs — FIXED) |
| BUG FIX | 2 (v9.11 ECDH salt bugs — FIXED) |
| BUG FIX | 1 (v9.24 settingtypes context bug — FIXED) |
| BUG FIX | 1 (v9.25 encryption log autocreate bug — FIXED) |
| BUG FIX | 1 (v9.42 ESC key not opening pause menu — FIXED) |
| BUG FIX | 1 (v9.43 settingtypes parentheses parsing error — FIXED) |

## Luanti-Secure v9.9 Bug Fixes

These bugs were discovered and fixed during v9.9 development:

| File | Bug | Root Cause | Fix | Status |
|------|-----|-----------|-----|--------|
| `src/network/crypto.h:205,222` | `i64` not declared | `i64` is not a type alias in the project — only `s64` exists in `irrTypes.h` | Changed all `i64` → `s64` (4 instances) | Fixed |
| `src/network/connection_security.h:610` | Too many arguments to `populateRealSecurityInfo` | 10-param overload was defined before 11-param overload, causing recursive call with wrong arity | Moved 11-param overload before 10-param wrapper | Fixed |
| `src/network/crypto.cpp:362` | S2C decryption failures (GCM auth tag mismatch) | `initFromSRPSessionKey()` used `secure_random()` for HKDF salt — each side generated different salts → different keys | Changed to deterministic HKDF salt derivation from SRP session key | Fixed |

## Luanti-Secure v9.11 Bug Fixes

These bugs were discovered and fixed during v9.11 ECDH forward secrecy development:

| File | Bug | Root Cause | Fix | Status |
|------|-----|-----------|-----|--------|
| `src/network/crypto.cpp` | `mixECDHSecretIntoKeys()` used unsalted HKDF | HKDF was called with `nullptr, 0` for salt, causing key mismatch between client and server | Derive salt deterministically from combined IKM (SRP + ECDH), matching `initFromSRPSessionKey()` pattern | Fixed |
| `src/network/crypto.cpp` | `rotateKeys()` used random HKDF salt | `secure_random()` for HKDF salt caused KEY MISMATCH between client and server (same bug pattern as v9.9 salt bug) | Derive salt deterministically from rotation IKM | Fixed |

## Luanti-Secure v9.24 Bug Fix

This bug was discovered and fixed during v9.24 development:

| File | Bug | Root Cause | Fix | Status |
|------|-----|-----------|-----|--------|
| `builtin/settingtypes.txt` | `ERROR[Main]: Unknown context in settingtypes.txt "encryption_log_level (Encryption log level) [server,client] enum action none,error,action,trace"` | The Luanti settingtypes parser (`builtin/common/settings/settingtypes.lua:30`) only accepts single context values: `common`, `client`, `server`, `world_creation`. The comma-separated `[server,client]` was treated as the literal string `"server,client"` which is not a valid context. | Changed `[server,client]` to `[common]` — the correct context for settings that apply to both server and client | Fixed |

## Luanti-Secure v9.25 Bug Fix

This bug was discovered and fixed during v9.25 development:

| File | Bug | Root Cause | Fix | Status |
|------|-----|-----------|-----|--------|
| `src/network/encryption_trace.cpp` | `encryption_trace.log` not created when logging is toggled ON with `--log` (default "action" level). Manually deleting the file and restarting does not recreate it. | `ensureTraceFileOpen()` only opened the file when `shouldLog(ENC_LOG_TRACE)` was true (i.e., level >= trace). At the default "action" level, the file was never opened. Additionally, `g_trace_disabled` permanently prevented reopening even if conditions changed. | (1) Changed guard from `shouldLog(ENC_LOG_TRACE)` to `shouldLog(ENC_LOG_ERROR)` — file is created at any non-none level. (2) Removed `g_trace_disabled` — file can be opened on subsequent calls. (3) All `enclog_*` macros now write to trace file via `EncLogLine` class, making it the single destination for all encryption events. | Fixed |

## Luanti-Secure v9.42 Bug Fix

This bug was discovered and fixed during v9.42 development:

| File | Bug | Root Cause | Fix | Status |
|------|-----|-----------|-----|--------|
| `src/client/inputhandler.cpp`, `src/client/inputhandler.h`, `src/client/game.cpp` | ESC key does not open the pause menu when pressed. No visible response in-game. | Two failure modes identified: (1) **Scancode mismatch**: The `keysListenedFor` lookup uses scancodes from `KeyPress`, but on some platforms the scancode from the key event (`SystemKeyCode`) may not match the scancode stored for `EscapeKey` (derived from `getScancodeFromKey`). This causes `setKeyDown()` to skip ESC entirely, so `keyWasDown[ESC]` is never set. (2) **Focus-loss clearing**: `input->clear()` in `processUserInput()` clears `keyWasDown[ESC]` when `device->isWindowActive()` returns false. Some window managers briefly report the window as inactive when ESC is pressed, eating the flag before `processKeyInput()` can check it. | Added `m_direct_esc_was_pressed` flag in `MyEventReceiver` that is set directly from the Irrlicht `KEY_ESCAPE` keycode in `OnEvent()`, bypassing the scancode-based `keysListenedFor` system entirely. This flag is placed before the fullscreen key check so ESC is never silently consumed. It is NOT cleared by `input->clear()`, making it immune to focus-loss clearing. Added public `consumeDirectEsc()` method for safe access. `cancelPressed()` now checks both `keyWasDown[ESC]` and the direct fallback. Added diagnostic logging for scancode mismatches. | Fixed |

## Luanti-Secure v9.43 Bug Fix

This bug was discovered and fixed during v9.43 development:

| File | Bug | Root Cause | Fix | Status |
|------|-----|-----------|-----|--------|
| `builtin/settingtypes.txt:905` | `ERROR[Main]: Invalid line in settingtypes.txt "enable_voice_chat_server (Enable voice chat (server)) bool true"` | The Luanti settingtypes parser (`builtin/common/settings/settingtypes.lua:134`) uses the Lua pattern `%(([^%)]*)%)` to match the readable name. This pattern captures all characters that are NOT `)`, then expects `)`. When the readable name contains nested parentheses like `Enable voice chat (server)`, the pattern matches `(Enable voice chat ` — stopping at the first `)` — and the remaining text `server)) bool true` does not match the expected format, causing the "Invalid line" error. | Changed the readable name from `(Enable voice chat (server))` to `(Enable voice chat on server)` — removing the nested parentheses that break the parser. This is the same class of bug as the v9.24 settingtypes context error. | Fixed |

## Luanti-Secure Compiler Warnings (from GitHub Actions CI)

These warnings appear when building Luanti-Secure with `-Wall` on Ubuntu 24.04 (gcc 13+).
They are in Luanti-Secure-added code and should be fixed in a future version.

| File | Line | Warning | Description | Status |
|------|------|---------|-------------|--------|
| src/client/client.cpp | 15 | `-Wunused-function` | `getSSCMMode()` is defined but never called — either remove it or add `[[maybe_unused]]` | Open |
| src/unittest/test_crypto.cpp | 825 | `-Wsign-compare` | Loop variable `int i` compared with `const size_t GCM_NONCE_SIZE` — change `i` to `size_t` | Open |
| src/unittest/test_crypto.cpp | 842 | `-Wsign-compare` | Same as line 825 in `testBuildNonceCounterMax()` — change `i` to `size_t` | Open |
| src/unittest/test_encrypted_packet_format.cpp | 547 | `-Wunused-but-set-variable` | `bool decrypt_ok = true` is set but never read — either use it in an assertion or remove it | Open |
| src/unittest/test_peer_encryption_state.cpp | 413 | `-Wunused-but-set-variable` | `auto n1 = dir.nextNonce()` is set but not used — add `[[maybe_unused]]` or assert on the value | Open |
| src/unittest/test_peer_encryption_state.cpp | 416 | `-Wunused-but-set-variable` | `auto n2 = dir.nextNonce()` — same as n1 above | Open |
| src/unittest/test_peer_encryption_state.cpp | 419 | `-Wunused-but-set-variable` | `auto n3 = dir.nextNonce()` — same as n1 above | Open |

**Note:** `src/porting.cpp:107,111` also has `-Wunused-result` warnings on `write()` calls, but these exist in upstream Luanti and have `(void)` casts already — the compiler still warns despite the cast. Not a Luanti-Secure issue.

## All Entries (Sorted by Priority: FIXME=1 > HACK=2 > TODO=3 > XXX=4)

| Type | Priority | File | Line | Description | Status |
|------|----------|------|------|-------------|--------|
| FIXME | 1 | builtin/profiler/instrumentation.lua | 66 | these paths are not canonicalized (i.e. can be .../luanti/bin/..) | Done |
| FIXME | 1 | builtin/sscsm_client/init.lua | 10 | send actual content defs to sscsm env | Done |
| FIXME | 1 | games/devtest/mods/unittests/misc.lua | 234 | EMERGE_CANCELLED can also mean that the block was already being | Done |
| FIXME | 1 | games/devtest/mods/unittests/raycast.lua | 49 | a variation of this unit test fails in an edge case. | Done |
| FIXME | 1 | irr/include/S3DVertex.h | 124 | the following types don't handle `Aux`, but we don't use them in situations where it's relevant. | Open |
| FIXME | 1 | irr/src/CGLTFMeshFileLoader.cpp | 760 | this hack also reverses triangle draw order | Open |
| FIXME | 1 | irr/src/CImage.cpp | 101 | this interprets the memory as [R][G][B], whereas SColor is stored as | Open |
| FIXME | 1 | irr/src/COBJMeshFileLoader.cpp | 16 | should we check the endptr?? | Open |
| FIXME | 1 | irr/src/OpenGL/Driver.cpp | 70 | this is actually UB because these vertex classes are not "standard-layout" | Open |
| FIXME | 1 | irr/src/OpenGL/Driver.cpp | 852 | split the batch? or let it crash? | Open |
| FIXME | 1 | irr/src/OpenGLES2/DriverGLES2.cpp | 123 | on GLES the functions are suffixed, but our loader code ignores | Open |
| FIXME | 1 | luanti/builtin/profiler/instrumentation.lua | 66 | these paths are not canonicalized (i.e. can be .../luanti/bin/..) | Done |
| FIXME | 1 | luanti/builtin/sscsm_client/init.lua | 10 | send actual content defs to sscsm env | Done |
| FIXME | 1 | luanti/games/devtest/mods/unittests/misc.lua | 234 | EMERGE_CANCELLED can also mean that the block was already being | Done |
| FIXME | 1 | luanti/games/devtest/mods/unittests/raycast.lua | 49 | a variation of this unit test fails in an edge case. | Done |
| FIXME | 1 | luanti/irr/include/S3DVertex.h | 124 | the following types don't handle `Aux`, but we don't use them in situations where it's relevant. | Open |
| FIXME | 1 | luanti/irr/src/CGLTFMeshFileLoader.cpp | 760 | this hack also reverses triangle draw order | Open |
| FIXME | 1 | luanti/irr/src/CImage.cpp | 101 | this interprets the memory as [R][G][B], whereas SColor is stored as | Open |
| FIXME | 1 | luanti/irr/src/COBJMeshFileLoader.cpp | 16 | should we check the endptr?? | Open |
| FIXME | 1 | luanti/irr/src/OpenGL/Driver.cpp | 70 | this is actually UB because these vertex classes are not "standard-layout" | Open |
| FIXME | 1 | luanti/irr/src/OpenGL/Driver.cpp | 852 | split the batch? or let it crash? | Open |
| FIXME | 1 | luanti/irr/src/OpenGLES2/DriverGLES2.cpp | 123 | on GLES the functions are suffixed, but our loader code ignores | Open |
| FIXME | 1 | luanti/src/client/client.cpp | 185 | only read files that are relevant to sscsm, and compute sha2 digests | Partial (complex) |
| FIXME | 1 | luanti/src/client/client.cpp | 201 | network packets | Partial (complex) |
| FIXME | 1 | luanti/src/client/client.cpp | 202 | check that *client_builtin* is not overridden | Partial (complex) |
| FIXME | 1 | luanti/src/client/client.cpp | 205 | enum | Partial (complex) |
| FIXME | 1 | luanti/src/client/content_cao.cpp | 364 | work around #16221 which is caused by the camera position and thus | Done |
| FIXME | 1 | luanti/src/client/content_cao.cpp | 1641 | ^ This code is trash. It's also broken. | Done |
| FIXME | 1 | luanti/src/client/game.cpp | 2627 | I bet we can be smarter about this and don't need to redraw | Done |
| FIXME | 1 | luanti/src/client/game_formspec.cpp | 574 | m_formspec and this value are not in sync at all times. | Open |
| FIXME | 1 | luanti/src/client/game_formspec.h | 61 | Layering is already managed by `GUIModalMenu` (`g_menumgr`), hence this | Open |
| FIXME | 1 | luanti/src/client/gameui.cpp | 286 | This updates the profiler with incomplete values | Open |
| FIXME | 1 | luanti/src/client/hud.cpp | 385 | why do we have such a weird unportable hack?? | Done |
| FIXME | 1 | luanti/src/client/mapblock_mesh.cpp | 646 | ^ doesn't really make sense. and in practice, bp is always aligned | Open |
| FIXME | 1 | luanti/src/client/minimap.cpp | 482 | this is a pointless roundtrip through the gpu | Open |
| FIXME | 1 | luanti/src/client/particles.cpp | 488 | this should be moved into a TileAnimationParams class method | Open |
| FIXME | 1 | luanti/src/client/renderingengine.h | 143 | this is still global when it shouldn't be | Open |
| FIXME | 1 | luanti/src/client/shader.cpp | 229 | The node specular effect is currently disabled due to mixed in-game | Partial (setting added) |
| FIXME | 1 | luanti/src/client/sky.cpp | 708 | stupid helper that does a pointless texture upload/download | Done |
| FIXME | 1 | luanti/src/client/sound/sound_singleton.cpp | 33 | This value assumes 1 node sidelength = 1 meter, and "normal" air. | Open |
| FIXME | 1 | luanti/src/client/texturesource.cpp | 585 | we should rebuild palettes too | Open |
| FIXME | 1 | luanti/src/collision.cpp | 114 | The dtime calculation is inaccurate without acceleration information. | Open |
| FIXME | 1 | luanti/src/collision.cpp | 565 | This code is necessary until `axisAlignedCollision` takes acceleration | Open |
| FIXME | 1 | luanti/src/filesys.cpp | 730 | same problem probably exists on win32 with "C:" | Done |
| FIXME | 1 | luanti/src/gui/profilergraph.h | 21 | this data structure is awfully inefficient | Open |
| FIXME | 1 | luanti/src/mapgen/mg_decoration.cpp | 390 | We do not own this schematic, yet we only have a pointer to it | Open |
| FIXME | 1 | luanti/src/network/mtp/impl.cpp | 271 | Handle the error? | Open |
| FIXME | 1 | luanti/src/nodedef.cpp | 1345 | support arbitrary rotations (to.param2 & 0x1F) (#7696) | Done |
| FIXME | 1 | luanti/src/rollback_interface.cpp | 33 | version bump?? | Done |
| FIXME | 1 | luanti/src/rollback_interface.cpp | 161 | version bump?? | Done |
| FIXME | 1 | luanti/src/script/common/c_content.cpp | 1620 | inventory_image.animation, inventory_overlay.animation, wield_image.animation | Open |
| FIXME | 1 | luanti/src/script/common/c_converter.cpp | 298 | maybe we should have strict type checks here. compare to is_color_table() | Open |
| FIXME | 1 | luanti/src/script/cpp_api/s_async.cpp | 278 | there's no general way to report such fatal errors to our "owner" | Open |
| FIXME | 1 | luanti/src/script/lua_api/l_camera.cpp | 122 | wouldn't localplayer be a better place for this? | Open |
| FIXME | 1 | luanti/src/script/lua_api/l_camera.cpp | 133 | wouldn't localplayer be a better place for this? | Open |
| FIXME | 1 | luanti/src/script/lua_api/l_client.cpp | 33 | This should eventually be moved somewhere else | Open |
| FIXME | 1 | luanti/src/script/lua_api/l_object.cpp | 1876 | only send when actually changed | Open |
| FIXME | 1 | luanti/src/script/lua_api/l_util.cpp | 380 | zero copy possible in c++26 or with custom rdbuf | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_ievent.h | 23 | actually serialize, and replace this with a string | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 14 | remove once we have actual serialization | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 21 | actually serialize, and replace this with a std::vector<u8>. | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 35 | as above, actually serialize | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 44 | this will need to use a type tag for T | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 54 | should look something like this: | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 71 | should look something like this: | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_irequest.h | 79 | The actual deserialization will have to use a type tag, and then | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_requests.h | 46 | override global loggers to use this in sscsm process | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_requests.h | 64 | override global loggers to use this in sscsm process | Open |
| FIXME | 1 | luanti/src/script/sscsm/sscsm_stupid_channel.h | 12 | replace this with an ipc channel | Open |
| FIXME | 1 | luanti/src/server.cpp | 3747 | we don't track which client still knows about this spawner, so | Open |
| FIXME | 1 | luanti/src/server/clientiface.cpp | 287 | This only works if the client uses a small enough | Open |
| FIXME | 1 | luanti/src/server/player_sao.cpp | 694 | Bouncy nodes cause practically unbound increase in Y speed, | Open |
| FIXME | 1 | luanti/src/server/player_sao.cpp | 712 | Checking downwards movement is not easily possible currently, | Open |
| FIXME | 1 | luanti/src/serverenvironment.cpp | 983 | this is not actually correct, because the block may have been | Done |
| FIXME | 1 | luanti/src/servermap.cpp | 680 | serialization happens under mutex | Open |
| FIXME | 1 | luanti/src/servermap.cpp | 700 | zero copy possible in c++20 or with custom rdbuf | Open |
| FIXME | 1 | luanti/src/threading/thread.cpp | 119 | what if this fails, or if already locked by same thread? | Done |
| FIXME | 1 | luanti/src/tool.cpp | 457 | punch_operable is supposed to apply to "non-tool" items too | Done |
| FIXME | 1 | luanti/src/unittest/test_collision.cpp | 371 | this is actually inconsistent | Open |
| FIXME | 1 | luanti/src/unittest/test_mapdatabase.cpp | 196 | this isn't working consistently, maybe later | Open |
| FIXME | 1 | luanti/src/unittest/test_sao.cpp | 215 | figure out if this is a bug that needs to be fixed | Open |
| FIXME | 1 | luanti/src/unittest/test_socket.cpp | 84 | This fails on some systems | Open |
| FIXME | 1 | luanti/src/unittest/test_socket.cpp | 131 | This fails on some systems | Open |
| FIXME | 1 | luanti/src/util/strfnd.h | 10 | convert this class to string_view | Done |
| FIXME | 1 | luanti/src/voxel.h | 122 | possible integer overflow here | Done |
| FIXME | 1 | luanti/util/ci/run-clang-tidy.py | 10 | Integrate with clang-tidy-diff.py | Open |
| FIXME | 1 | src/client/client.cpp | 185 | only read files that are relevant to sscsm, and compute sha2 digests | Partial (complex) |
| FIXME | 1 | src/client/client.cpp | 201 | network packets | Partial (complex) |
| FIXME | 1 | src/client/client.cpp | 202 | check that *client_builtin* is not overridden | Partial (complex) |
| FIXME | 1 | src/client/client.cpp | 205 | enum | Partial (complex) |
| FIXME | 1 | src/client/content_cao.cpp | 364 | work around #16221 which is caused by the camera position and thus | Done |
| FIXME | 1 | src/client/content_cao.cpp | 1641 | ^ This code is trash. It's also broken. | Done |
| FIXME | 1 | src/client/game.cpp | 2631 | I bet we can be smarter about this and don't need to redraw | Open |
| FIXME | 1 | src/client/game_formspec.cpp | 574 | m_formspec and this value are not in sync at all times. | Open |
| FIXME | 1 | src/client/game_formspec.h | 61 | Layering is already managed by `GUIModalMenu` (`g_menumgr`), hence this | Open |
| FIXME | 1 | src/client/gameui.cpp | 483 | This updates the profiler with incomplete values | Done |
| FIXME | 1 | src/client/hud.cpp | 385 | why do we have such a weird unportable hack?? | Done |
| FIXME | 1 | src/client/mapblock_mesh.cpp | 646 | ^ doesn't really make sense. and in practice, bp is always aligned | Done |
| FIXME | 1 | src/client/minimap.cpp | 482 | this is a pointless roundtrip through the gpu | Open |
| FIXME | 1 | src/client/particles.cpp | 488 | this should be moved into a TileAnimationParams class method | Done |
| FIXME | 1 | src/client/renderingengine.h | 143 | this is still global when it shouldn't be | Open |
| FIXME | 1 | src/client/shader.cpp | 229 | The node specular effect is currently disabled due to mixed in-game | Partial (setting added) |
| FIXME | 1 | src/client/sky.cpp | 708 | stupid helper that does a pointless texture upload/download | Done |
| FIXME | 1 | src/client/sound/sound_singleton.cpp | 33 | This value assumes 1 node sidelength = 1 meter, and "normal" air. | Open |
| FIXME | 1 | src/client/texturesource.cpp | 585 | we should rebuild palettes too | Open |
| FIXME | 1 | src/collision.cpp | 114 | The dtime calculation is inaccurate without acceleration information. | Open |
| FIXME | 1 | src/collision.cpp | 567 | This code is necessary until `axisAlignedCollision` takes acceleration | Open |
| FIXME | 1 | src/filesys.cpp | 730 | same problem probably exists on win32 with "C:" | Done |
| FIXME | 1 | src/gui/profilergraph.h | 21 | this data structure is awfully inefficient | Open |
| FIXME | 1 | src/mapgen/mg_decoration.cpp | 390 | We do not own this schematic, yet we only have a pointer to it | Open |
| FIXME | 1 | src/nodedef.cpp | 1345 | support arbitrary rotations (to.param2 & 0x1F) (#7696) | Done |
| FIXME | 1 | src/rollback_interface.cpp | 33 | version bump?? | Done |
| FIXME | 1 | src/rollback_interface.cpp | 161 | version bump?? | Done |
| FIXME | 1 | src/script/common/c_content.cpp | 1620 | inventory_image.animation, inventory_overlay.animation, wield_image.animation | Open |
| FIXME | 1 | src/script/common/c_converter.cpp | 298 | maybe we should have strict type checks here. compare to is_color_table() | Open |
| FIXME | 1 | src/script/cpp_api/s_async.cpp | 278 | there's no general way to report such fatal errors to our "owner" | Open |
| FIXME | 1 | src/script/lua_api/l_camera.cpp | 122 | wouldn't localplayer be a better place for this? | Open |
| FIXME | 1 | src/script/lua_api/l_camera.cpp | 133 | wouldn't localplayer be a better place for this? | Open |
| FIXME | 1 | src/script/lua_api/l_client.cpp | 33 | This should eventually be moved somewhere else | Open |
| FIXME | 1 | src/script/lua_api/l_object.cpp | 1873 | only send when actually changed | Open (Complex) |
| FIXME | 1 | src/script/lua_api/l_util.cpp | 380 | zero copy possible in c++26 or with custom rdbuf | Open |
| FIXME | 1 | src/script/sscsm/sscsm_ievent.h | 23 | actually serialize, and replace this with a string | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 14 | remove once we have actual serialization | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 21 | actually serialize, and replace this with a std::vector<u8>. | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 35 | as above, actually serialize | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 44 | this will need to use a type tag for T | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 54 | should look something like this: | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 71 | should look something like this: | Open |
| FIXME | 1 | src/script/sscsm/sscsm_irequest.h | 79 | The actual deserialization will have to use a type tag, and then | Open |
| FIXME | 1 | src/script/sscsm/sscsm_requests.h | 46 | override global loggers to use this in sscsm process | Open |
| FIXME | 1 | src/script/sscsm/sscsm_requests.h | 64 | override global loggers to use this in sscsm process | Open |
| FIXME | 1 | src/script/sscsm/sscsm_stupid_channel.h | 12 | replace this with an ipc channel | Open |
| FIXME | 1 | src/server.cpp | 3740 | we don't track which client still knows about this spawner, so | Open |
| FIXME | 1 | src/server/clientiface.cpp | 287 | This only works if the client uses a small enough | Open |
| FIXME | 1 | src/server/player_sao.cpp | 694 | Bouncy nodes cause practically unbound increase in Y speed, | Open |
| FIXME | 1 | src/server/player_sao.cpp | 712 | Checking downwards movement is not easily possible currently, | Open |
| FIXME | 1 | src/serverenvironment.cpp | 983 | this is not actually correct, because the block may have been | Done |
| FIXME | 1 | src/servermap.cpp | 680 | serialization happens under mutex | Done |
| FIXME | 1 | src/servermap.cpp | 700 | zero copy possible in c++20 or with custom rdbuf | Open |
| FIXME | 1 | src/threading/thread.cpp | 119 | what if this fails, or if already locked by same thread? | Done |
| FIXME | 1 | src/tool.cpp | 457 | punch_operable is supposed to apply to "non-tool" items too | Done |
| FIXME | 1 | src/unittest/test_collision.cpp | 371 | this is actually inconsistent | Open |
| FIXME | 1 | src/unittest/test_mapdatabase.cpp | 196 | this isn't working consistently, maybe later | Open |
| FIXME | 1 | src/unittest/test_sao.cpp | 215 | figure out if this is a bug that needs to be fixed | Open |
| FIXME | 1 | src/unittest/test_socket.cpp | 84 | This fails on some systems | Open |
| FIXME | 1 | src/unittest/test_socket.cpp | 131 | This fails on some systems | Open |
| FIXME | 1 | src/util/strfnd.h | 10 | convert this class to string_view | Done |
| FIXME | 1 | src/voxel.h | 122 | possible integer overflow here | Done |
| FIXME | 1 | util/ci/run-clang-tidy.py | 10 | Integrate with clang-tidy-diff.py | Open |
| HACK | 2 | builtin/common/settings/dlg_settings.lua | 549 | this is needed to allow resubmitting the same formspec | Open |
| HACK | 2 | builtin/common/tests/serialize_spec.lua | 163 | math.random(a, b) requires a, b & b - a to fit within a 32-bit int | Open |
| HACK | 2 | builtin/mainmenu/dlg_create_world.lua | 342 | This timestamp prevents double-triggering when pressing Enter on an input box | Open |
| HACK | 2 | irr/include/SkinnedMesh.h | 386 | the .x and .b3d loader do not separate the "loader" class from an "extractor" class | Open |
| HACK | 2 | irr/include/irrString.h | 16 | import these string methods from MT's util/string.h | Open |
| HACK | 2 | irr/src/COpenGLDriver.cpp | 1432 | this messes with alpha blending | Open |
| HACK | 2 | luanti/builtin/common/settings/dlg_settings.lua | 527 | this is needed to allow resubmitting the same formspec | Open |
| HACK | 2 | luanti/builtin/common/tests/serialize_spec.lua | 163 | math.random(a, b) requires a, b & b - a to fit within a 32-bit int | Open |
| HACK | 2 | luanti/builtin/mainmenu/dlg_create_world.lua | 342 | This timestamp prevents double-triggering when pressing Enter on an input box | Open |
| HACK | 2 | luanti/irr/include/SkinnedMesh.h | 386 | the .x and .b3d loader do not separate the "loader" class from an "extractor" class | Open |
| HACK | 2 | luanti/irr/include/irrString.h | 16 | import these string methods from MT's util/string.h | Open |
| HACK | 2 | luanti/irr/src/COpenGLDriver.cpp | 1432 | this messes with alpha blending | Open |
| HACK | 2 | luanti/src/client/clientenvironment.cpp | 132 | the factor 2 for gravity is arbitrary and should be removed eventually | Open |
| HACK | 2 | luanti/src/client/clientenvironment.cpp | 139 | the factor 2 for gravity is arbitrary and should be removed eventually | Open |
| HACK | 2 | luanti/src/server.cpp | 3009 | to keep compatibility with 5.0.0 clients | Open |
| HACK | 2 | luanti/src/server/activeobjectmgr.cpp | 121 | defensively only update if we already know the object, | Open |
| HACK | 2 | luanti/src/test/test_serveractiveobjectmgr.cpp | 48 | work around m_env == nullptr | Open |
| HACK | 2 | src/client/clientenvironment.cpp | 132 | the factor 2 for gravity is arbitrary and should be removed eventually | Open |
| HACK | 2 | src/client/clientenvironment.cpp | 139 | the factor 2 for gravity is arbitrary and should be removed eventually | Open |
| HACK | 2 | src/server.cpp | 3002 | to keep compatibility with 5.0.0 clients | Open |
| HACK | 2 | src/server/activeobjectmgr.cpp | 121 | defensively only update if we already know the object, | Open |
| HACK | 2 | src/test/test_serveractiveobjectmgr.cpp | 48 | work around m_env == nullptr | Open |
| TODO | 3 | builtin/common/filterlist.lua | 6 | improve doc | Open |
| TODO | 3 | builtin/common/filterlist.lua | 7 | code cleanup | Open |
| TODO | 3 | builtin/common/settings/settingtypes.lua | 472 | Support game/mod settings in the pause menu too | Open |
| TODO | 3 | builtin/fstk/ui.lua | 13 | check child | Open |
| TODO | 3 | builtin/game/falling.lua | 280 | this hack could be avoided in the future if objects | Open |
| TODO | 3 | builtin/game/register.lua | 259 | most of these checks should become an error after a while (maybe in 2026?) | Open |
| TODO | 3 | builtin/game/register.lua | 468 | it's not clear if this needs to be allowed at all? | Open |
| TODO | 3 | builtin/game/tests/test_moveaction.lua | 111 | would be nice to check the dropped count here too | Open |
| TODO | 3 | builtin/mainmenu/content/contentdb.lua | 639 | use Android API to determine size of cut outs | Open |
| TODO | 3 | docs/fst_api.txt | 57 | cbf_events         = function(tabview, event, tabname),           -- callback for events | Open |
| TODO | 3 | games/devtest/mods/testnodes/textures.lua | 263 | Types 1, 2 & 10 should have two test nodes each (i.e. bottom-top | Open |
| TODO | 3 | irr/include/IEventReceiver.h | 170 | Remove this once SDL headers are available in Luanti | Open |
| TODO | 3 | irr/include/SkinnedMesh.h | 93 | ideally we shouldn't be forced to implement this | Open |
| TODO | 3 | irr/include/SkinnedMesh.h | 301 | ^ should turn this into optional meshbuffer parent field? | Open |
| TODO | 3 | irr/include/SkinnedMesh.h | 319 | refactor away: pointers -> IDs (problem: .x loader abuses SJoint) | Open |
| TODO | 3 | irr/include/quaternion.h | 239 | speed this up | Open |
| TODO | 3 | irr/include/quaternion.h | 250 | speed this up | Open |
| TODO | 3 | irr/include/quaternion.h | 260 | speed this up | Open |
| TODO | 3 | irr/include/quaternion.h | 270 | speed this up | Open |
| TODO | 3 | irr/include/quaternion.h | 337 | (no description) | Open |
| TODO | 3 | irr/src/AnimatedMeshSceneNode.cpp | 163 | if there are no bone overrides or no animation blending, this is unnecessary. | Open |
| TODO | 3 | irr/src/CB3DMeshFileLoader.cpp | 791 | create color key texture | Open |
| TODO | 3 | irr/src/CBillboardSceneNode.h | 100 | BUG - still can be wrong with scaling < 1. Billboards should calculate relative coordinates for their mesh | Open |
| TODO | 3 | irr/src/CDummyTransformationSceneNode.h | 39 | We can add least add some warnings to find troubles faster until we have | Open |
| TODO | 3 | irr/src/CEGLManager.cpp | 162 | We should also have a confStyle ECS_EGL_CHOOSE_CLOSEST | Open |
| TODO | 3 | irr/src/CGLTFMeshFileLoader.cpp | 759 | (low-priority, maybe never) also reverse winding order based on determinant of global transform | Open |
| TODO | 3 | irr/src/CGLTFMeshFileLoader.cpp | 808 | verify that the automatic normal recalculation done in Minetest indeed works correctly | Open |
| TODO | 3 | irr/src/CGUIEditBox.cpp | 1284 | that function does interpret VAlign according to line-index (indexed line is placed on top-center-bottom) | Open |
| TODO | 3 | irr/src/CGUIEditBox.cpp | 1472 | Needs a clean left and right gap removal depending on HAlign, similar to vertical scrolling tests for top/bottom. | Open |
| TODO | 3 | irr/src/CGUIEditBox.cpp | 1484 | should show more characters to the left when we're scrolling left | Open |
| TODO | 3 | irr/src/CGUIFont.cpp | 47 | spritebank still exists in gui-environment and should be removed here when it's | Open |
| TODO | 3 | irr/src/CGUIStaticText.cpp | 477 | Text can have multiple lines which are not in BrokenText | Open |
| TODO | 3 | irr/src/CGUITabControl.cpp | 608 | exact size depends on borders in draw3DTabButton which we don't get with current interface | Open |
| TODO | 3 | irr/src/CGUITabControl.cpp | 627 | exact size depends on borders in draw3DTabButton which we don't get with current interface | Open |
| TODO | 3 | irr/src/CGUITabControl.cpp | 840 | merge this with draw() ? | Open |
| TODO | 3 | irr/src/CImage.cpp | 324 | Handle other formats | Open |
| TODO | 3 | irr/src/CImageWriterPNG.cpp | 118 | Error handling in case of unsupported color format | Open |
| TODO | 3 | irr/src/CImageWriterPNG.cpp | 143 | Error handling in case of unsupported color format | Open |
| TODO | 3 | irr/src/CIrrDeviceSDL.cpp | 128 | not correct, but couldn't figure out the bitset of mouseEvent->buttons yet. | Open |
| TODO | 3 | irr/src/CIrrDeviceSDL.cpp | 149 | not correct, but couldn't figure out the bitset of mouseEvent->buttons yet. | Open |
| TODO | 3 | irr/src/CIrrDeviceSDL.cpp | 316 | respect modifiers | Open |
| TODO | 3 | irr/src/CIrrDeviceSDL.cpp | 327 | SDL_HINT_KEYCODE_OPTIONS ? | Open |
| TODO | 3 | irr/src/CIrrDeviceSDL.cpp | 1128 | Check if the multiple open/close calls are too expensive, then | Open |
| TODO | 3 | irr/src/CIrrDeviceSDL.h | 350 | This is only used for scancode/keycode conversion with EKEY_CODE (among other things, for Luanti | Open |
| TODO | 3 | irr/src/CNullDriver.cpp | 1545 | I suspect it would be nice if the material had an enum for further control. | Open |
| TODO | 3 | irr/src/CNullDriver.cpp | 1553 | I suspect IMaterialRenderer::isTransparent also often could use SMaterial as parameter | Open |
| TODO | 3 | irr/src/COpenGLDriver.cpp | 1830 | why the & 0xFFFFFFFF? | Open |
| TODO | 3 | irr/src/COpenGLDriver.cpp | 2539 | Maybe we could support more formats (floating point and some of those beyond ECF_R8), didn't really try yet | Open |
| TODO | 3 | irr/src/CSceneManager.cpp | 66 | now that we have multiple scene managers, these should be | Open |
| TODO | 3 | irr/src/CXMeshFileLoader.cpp | 589 | change face indices in material list | Open |
| TODO | 3 | irr/src/CXMeshFileLoader.cpp | 641 | read them | Open |
| TODO | 3 | irr/src/CXMeshFileLoader.cpp | 1272 | parse options. | Open |
| TODO | 3 | irr/src/CXMeshFileLoader.cpp | 1832 | Check if data is properly converted here | Open |
| TODO | 3 | irr/src/OpenGL/Driver.cpp | 1304 | why the & 0xFFFFFFFF? | Open |
| TODO | 3 | lib/jsoncpp/jsoncpp.cpp | 781 | Help the compiler do the div and mod at compile time or get rid of | Open |
| TODO | 3 | luanti/builtin/common/filterlist.lua | 6 | improve doc | Open |
| TODO | 3 | luanti/builtin/common/filterlist.lua | 7 | code cleanup | Open |
| TODO | 3 | luanti/builtin/common/settings/settingtypes.lua | 472 | Support game/mod settings in the pause menu too | Open |
| TODO | 3 | luanti/builtin/fstk/ui.lua | 13 | check child | Open |
| TODO | 3 | luanti/builtin/game/falling.lua | 280 | this hack could be avoided in the future if objects | Open |
| TODO | 3 | luanti/builtin/game/register.lua | 259 | most of these checks should become an error after a while (maybe in 2026?) | Open |
| TODO | 3 | luanti/builtin/game/register.lua | 468 | it's not clear if this needs to be allowed at all? | Open |
| TODO | 3 | luanti/builtin/game/tests/test_moveaction.lua | 111 | would be nice to check the dropped count here too | Open |
| TODO | 3 | luanti/builtin/mainmenu/content/contentdb.lua | 639 | use Android API to determine size of cut outs | Open |
| TODO | 3 | luanti/docs/fst_api.txt | 57 | cbf_events         = function(tabview, event, tabname),           -- callback for events | Open |
| TODO | 3 | luanti/games/devtest/mods/testnodes/textures.lua | 263 | Types 1, 2 & 10 should have two test nodes each (i.e. bottom-top | Open |
| TODO | 3 | luanti/irr/include/IEventReceiver.h | 170 | Remove this once SDL headers are available in Luanti | Open |
| TODO | 3 | luanti/irr/include/SkinnedMesh.h | 93 | ideally we shouldn't be forced to implement this | Open |
| TODO | 3 | luanti/irr/include/SkinnedMesh.h | 301 | ^ should turn this into optional meshbuffer parent field? | Open |
| TODO | 3 | luanti/irr/include/SkinnedMesh.h | 319 | refactor away: pointers -> IDs (problem: .x loader abuses SJoint) | Open |
| TODO | 3 | luanti/irr/include/quaternion.h | 239 | speed this up | Open |
| TODO | 3 | luanti/irr/include/quaternion.h | 250 | speed this up | Open |
| TODO | 3 | luanti/irr/include/quaternion.h | 260 | speed this up | Open |
| TODO | 3 | luanti/irr/include/quaternion.h | 270 | speed this up | Open |
| TODO | 3 | luanti/irr/include/quaternion.h | 337 | (no description) | Open |
| TODO | 3 | luanti/irr/src/AnimatedMeshSceneNode.cpp | 163 | if there are no bone overrides or no animation blending, this is unnecessary. | Open |
| TODO | 3 | luanti/irr/src/CB3DMeshFileLoader.cpp | 791 | create color key texture | Open |
| TODO | 3 | luanti/irr/src/CBillboardSceneNode.h | 100 | BUG - still can be wrong with scaling < 1. Billboards should calculate relative coordinates for their mesh | Open |
| TODO | 3 | luanti/irr/src/CDummyTransformationSceneNode.h | 39 | We can add least add some warnings to find troubles faster until we have | Open |
| TODO | 3 | luanti/irr/src/CEGLManager.cpp | 162 | We should also have a confStyle ECS_EGL_CHOOSE_CLOSEST | Open |
| TODO | 3 | luanti/irr/src/CGLTFMeshFileLoader.cpp | 759 | (low-priority, maybe never) also reverse winding order based on determinant of global transform | Open |
| TODO | 3 | luanti/irr/src/CGLTFMeshFileLoader.cpp | 808 | verify that the automatic normal recalculation done in Minetest indeed works correctly | Open |
| TODO | 3 | luanti/irr/src/CGUIEditBox.cpp | 1284 | that function does interpret VAlign according to line-index (indexed line is placed on top-center-bottom) | Open |
| TODO | 3 | luanti/irr/src/CGUIEditBox.cpp | 1472 | Needs a clean left and right gap removal depending on HAlign, similar to vertical scrolling tests for top/bottom. | Open |
| TODO | 3 | luanti/irr/src/CGUIEditBox.cpp | 1484 | should show more characters to the left when we're scrolling left | Open |
| TODO | 3 | luanti/irr/src/CGUIFont.cpp | 47 | spritebank still exists in gui-environment and should be removed here when it's | Open |
| TODO | 3 | luanti/irr/src/CGUIStaticText.cpp | 477 | Text can have multiple lines which are not in BrokenText | Open |
| TODO | 3 | luanti/irr/src/CGUITabControl.cpp | 608 | exact size depends on borders in draw3DTabButton which we don't get with current interface | Open |
| TODO | 3 | luanti/irr/src/CGUITabControl.cpp | 627 | exact size depends on borders in draw3DTabButton which we don't get with current interface | Open |
| TODO | 3 | luanti/irr/src/CGUITabControl.cpp | 840 | merge this with draw() ? | Open |
| TODO | 3 | luanti/irr/src/CImage.cpp | 324 | Handle other formats | Open |
| TODO | 3 | luanti/irr/src/CImageWriterPNG.cpp | 118 | Error handling in case of unsupported color format | Open |
| TODO | 3 | luanti/irr/src/CImageWriterPNG.cpp | 143 | Error handling in case of unsupported color format | Open |
| TODO | 3 | luanti/irr/src/CIrrDeviceSDL.cpp | 128 | not correct, but couldn't figure out the bitset of mouseEvent->buttons yet. | Open |
| TODO | 3 | luanti/irr/src/CIrrDeviceSDL.cpp | 149 | not correct, but couldn't figure out the bitset of mouseEvent->buttons yet. | Open |
| TODO | 3 | luanti/irr/src/CIrrDeviceSDL.cpp | 316 | respect modifiers | Open |
| TODO | 3 | luanti/irr/src/CIrrDeviceSDL.cpp | 327 | SDL_HINT_KEYCODE_OPTIONS ? | Open |
| TODO | 3 | luanti/irr/src/CIrrDeviceSDL.cpp | 1128 | Check if the multiple open/close calls are too expensive, then | Open |
| TODO | 3 | luanti/irr/src/CIrrDeviceSDL.h | 350 | This is only used for scancode/keycode conversion with EKEY_CODE (among other things, for Luanti | Open |
| TODO | 3 | luanti/irr/src/CNullDriver.cpp | 1545 | I suspect it would be nice if the material had an enum for further control. | Open |
| TODO | 3 | luanti/irr/src/CNullDriver.cpp | 1553 | I suspect IMaterialRenderer::isTransparent also often could use SMaterial as parameter | Open |
| TODO | 3 | luanti/irr/src/COpenGLDriver.cpp | 1830 | why the & 0xFFFFFFFF? | Open |
| TODO | 3 | luanti/irr/src/COpenGLDriver.cpp | 2539 | Maybe we could support more formats (floating point and some of those beyond ECF_R8), didn't really try yet | Open |
| TODO | 3 | luanti/irr/src/CSceneManager.cpp | 66 | now that we have multiple scene managers, these should be | Open |
| TODO | 3 | luanti/irr/src/CXMeshFileLoader.cpp | 589 | change face indices in material list | Open |
| TODO | 3 | luanti/irr/src/CXMeshFileLoader.cpp | 641 | read them | Open |
| TODO | 3 | luanti/irr/src/CXMeshFileLoader.cpp | 1272 | parse options. | Open |
| TODO | 3 | luanti/irr/src/CXMeshFileLoader.cpp | 1832 | Check if data is properly converted here | Open |
| TODO | 3 | luanti/irr/src/OpenGL/Driver.cpp | 1304 | why the & 0xFFFFFFFF? | Open |
| TODO | 3 | luanti/lib/jsoncpp/jsoncpp.cpp | 781 | Help the compiler do the div and mod at compile time or get rid of | Open |
| TODO | 3 | luanti/src/benchmark/benchmark_activeobjectmgr.cpp | 118 | benchmark active object manager update costs | Open |
| TODO | 3 | luanti/src/chat.cpp | 140 | Avoid reformatting ALL lines (even invisible ones) | Open |
| TODO | 3 | luanti/src/chat.cpp | 791 | Remove the need to parse chat messages client-side, by sending | Open |
| TODO | 3 | luanti/src/client/camera.cpp | 78 | Local caching of settings is not optimal and should at some stage | Open |
| TODO | 3 | luanti/src/client/camera.cpp | 700 | This is quite primitive. It would be better to let the GPU handle | Open |
| TODO | 3 | luanti/src/client/client.cpp | 273 | Delete this code block when server-sent CSM and verifying of builtin are | Open |
| TODO | 3 | luanti/src/client/clientevent.h | 70 | should get rid of this ctor | Open |
| TODO | 3 | luanti/src/client/clientlauncher.cpp | 449 | Re-use existing structs (GameStartData) | Open |
| TODO | 3 | luanti/src/client/clientmap.cpp | 416 | Include this as a flag for an extended debugging setting | Open |
| TODO | 3 | luanti/src/client/content_cao.cpp | 1712 | Execute defined fast response | Open |
| TODO | 3 | luanti/src/client/content_cao.cpp | 1781 | Execute defined fast response | Open |
| TODO | 3 | luanti/src/client/game.cpp | 1776 | When legacy minimap is deprecated, keep only HUD minimap stuff here | Open |
| TODO | 3 | luanti/src/client/joystick_controller.cpp | 69 | find usage for button 0 | Open |
| TODO | 3 | luanti/src/client/joystick_controller.cpp | 75 | find usage for buttons 0, 1 and 4, 5 | Open |
| TODO | 3 | luanti/src/client/joystick_controller.cpp | 252 | auto detection | Open |
| TODO | 3 | luanti/src/client/localplayer.cpp | 939 | (when fixed): Set Y-speed only to 0 when position.Y < new_y. | Open |
| TODO | 3 | luanti/src/client/localplayer.cpp | 946 | This shouldn't be hardcoded but decided by the server | Open |
| TODO | 3 | luanti/src/client/mapblock_mesh.cpp | 592 | we should change the meshgen so it already applies the tile color | Open |
| TODO | 3 | luanti/src/client/mesh_generator_thread.h | 106 | Add callback to update these when g_settings changes, and update all meshes | Open |
| TODO | 3 | luanti/src/client/mesh_generator_thread.h | 140 | Add callback to update these when g_settings changes | Open |
| TODO | 3 | luanti/src/client/node_visuals.cpp | 517 | this should be done consistently when the mesh is loaded | Open |
| TODO | 3 | luanti/src/client/sound_maker.h | 14 | move this together with the game.cpp interact code to its own file. | Open |
| TODO | 3 | luanti/src/constants.h | 92 | Use case-insensitive player names instead of this hack. | Open |
| TODO | 3 | luanti/src/content/subgames.cpp | 228 | Optimize such that `getAvailableGamePaths()` is not run N times. | Open |
| TODO | 3 | luanti/src/gamedef.h | 45 | these should be made const-safe so that a const IGameDef* is | Open |
| TODO | 3 | luanti/src/gameparams.h | 28 | unify with MainMenuData | Open |
| TODO | 3 | luanti/src/gettime.h | 25 | we should check if the function returns NULL, which would mean error | Open |
| TODO | 3 | luanti/src/gui/drawItemStack.cpp | 135 | could be moved to a shader | Open |
| TODO | 3 | luanti/src/gui/guiEngine.cpp | 661 | directly stream the response data into the file instead of first | Open |
| TODO | 3 | luanti/src/gui/guiFormSpecMenu.cpp | 3375 | getSpecByID is a linear search. It should made O(1), or cached here. | Open |
| TODO | 3 | luanti/src/gui/guiHyperText.cpp | 66 | find a way to check font validity | Open |
| TODO | 3 | luanti/src/gui/guiMainMenu.h | 19 | unify with GameStartData | Open |
| TODO | 3 | luanti/src/inventory.cpp | 548 | Implement this: | Open |
| TODO | 3 | luanti/src/irrlicht_changes/static_text.cpp | 106 | Implement colorization | Open |
| TODO | 3 | luanti/src/log.h | 196 | Search/replace these with verbose/tracestream | Open |
| TODO | 3 | luanti/src/main.cpp | 69 | luanti.conf with migration | Open |
| TODO | 3 | luanti/src/mapgen/cavegen.cpp | 14 | Remove this. Cave liquids are now defined and located using biome definitions | Open |
| TODO | 3 | luanti/src/mapgen/cavegen.cpp | 530 | 'np_caveliquids' is deprecated and should eventually be removed. | Open |
| TODO | 3 | luanti/src/mapgen/cavegen.h | 113 | 'np_caveliquids' is deprecated and should eventually be removed. | Open |
| TODO | 3 | luanti/src/mapgen/dungeongen.cpp | 417 | fix stairs code so it works 100% | Open |
| TODO | 3 | luanti/src/mapgen/mapgen_carpathian.cpp | 405 | '<=' fix from generateTerrain() | Open |
| TODO | 3 | luanti/src/mapgen/mapgen_carpathian.cpp | 543 | '<=' | Open |
| TODO | 3 | luanti/src/mapgen/mg_decoration.cpp | 134 | this is a stupid restriction, which we should lift | Open |
| TODO | 3 | luanti/src/network/mtp/impl.cpp | 31 | Clean this up. | Open |
| TODO | 3 | luanti/src/network/mtp/threads.cpp | 29 | Clean this up. | Open |
| TODO | 3 | luanti/src/nodedef.h | 839 | replace or remove | Open |
| TODO | 3 | luanti/src/objdef.h | 56 | const correctness for getter methods | Open |
| TODO | 3 | luanti/src/player.cpp | 93 | Make this generic | Open |
| TODO | 3 | luanti/src/porting.cpp | 501 | Luanti with migration | Open |
| TODO | 3 | luanti/src/porting.cpp | 566 | luanti with migration | Open |
| TODO | 3 | luanti/src/porting.cpp | 595 | luanti with migration | Open |
| TODO | 3 | luanti/src/porting.cpp | 613 | luanti with migration | Open |
| TODO | 3 | luanti/src/porting.cpp | 721 | luanti with migration | Open |
| TODO | 3 | luanti/src/porting.cpp | 725 | luanti with migration | Open |
| TODO | 3 | luanti/src/script/common/c_content.cpp | 702 | should be an error | Open |
| TODO | 3 | luanti/src/script/common/c_content.cpp | 1050 | should be an error | Open |
| TODO | 3 | luanti/src/script/common/c_content.h | 126 | rename to "read_enum_field" and replace with type-safe template | Open |
| TODO | 3 | luanti/src/script/common/c_converter.cpp | 32 | this should be turned into an error in 2026. | Open |
| TODO | 3 | luanti/src/script/common/c_converter.cpp | 66 | someone find out if it's faster to have the type check in Lua too | Open |
| TODO | 3 | luanti/src/script/common/c_converter.h | 77 | some day we should figure out the type-checking situation so it's done | Open |
| TODO | 3 | luanti/src/script/cpp_api/s_base.cpp | 555 | still needs work | Open |
| TODO | 3 | luanti/src/script/cpp_api/s_entity.cpp | 203 | this should be changed to not read the legacy place | Open |
| TODO | 3 | luanti/src/script/lua_api/l_base.cpp | 89 | Check presence first! | Open |
| TODO | 3 | luanti/src/script/lua_api/l_env.cpp | 450 | custom PointedThing (requires a converter function) | Open |
| TODO | 3 | luanti/src/script/lua_api/l_env.cpp | 954 | A similar but generalized (and therefore slower) version of this | Open |
| TODO | 3 | luanti/src/script/lua_api/l_localplayer.cpp | 483 | figure our if these are useful in any way | Open |
| TODO | 3 | luanti/src/script/lua_api/l_mainmenu.cpp | 397 | inspect call sites and make sure this is handled, then we can | Open |
| TODO | 3 | luanti/src/script/lua_api/l_metadata.cpp | 166 | this silently produces 0.0 if conversion fails, which is a footgun | Open |
| TODO | 3 | luanti/src/script/lua_api/l_object.cpp | 506 | if possible: improve the camera collision detection to allow Y <= -1.5) | Open |
| TODO | 3 | luanti/src/script/scripting_emerge.cpp | 79 | ^ these should also be renamed to InitializeRO or such | Open |
| TODO | 3 | luanti/src/server/clientiface.cpp | 840 | this should be done by client destructor!!! | Open |
| TODO | 3 | luanti/src/server/luaentity_sao.cpp | 225 | force send when acceleration changes enough? | Open |
| TODO | 3 | luanti/src/server/luaentity_sao.cpp | 371 | give Lua control over wear | Open |
| TODO | 3 | luanti/src/serverenvironment.cpp | 948 | The blocks removed here will only be picked up again | Open |
| TODO | 3 | luanti/src/serverenvironment.cpp | 1022 | reinitializing this state every time is probably not efficient? | Open |
| TODO | 3 | luanti/src/serverenvironment.h | 124 | find way to remove this fct! | Open |
| TODO | 3 | luanti/src/servermap.cpp | 752 | Block should be marked as invalid in memory so that it is | Open |
| TODO | 3 | luanti/src/servermap.h | 22 | this could wrap all calls to MapDatabase, including locking | Open |
| TODO | 3 | luanti/src/settings.cpp | 125 | Avoid copying Settings objects. Make this private. | Open |
| TODO | 3 | luanti/src/settings.cpp | 862 | Remove this function | Open |
| TODO | 3 | luanti/src/test/test_k_d_tree.cpp | 45 | check | Open |
| TODO | 3 | luanti/src/unittest/test.cpp | 446 | this method should probably be removed | Open |
| TODO | 3 | luanti/src/unittest/test.cpp | 526 | Update to new system | Open |
| TODO | 3 | luanti/src/unittest/test.cpp | 682 | Check for AlreadyExistsException | Open |
| TODO | 3 | luanti/src/unittest/test_collision.cpp | 151 | Y-, Y+, Z-, Z+ | Open |
| TODO | 3 | luanti/src/unittest/test_collision.cpp | 374 | things to test: | Open |
| TODO | 3 | luanti/src/unittest/test_keycode.cpp | 18 | Re-introduce unittests after fully switching to SDL. | Open |
| TODO | 3 | luanti/src/unittest/test_sao.cpp | 74 | best to factor entity mgmt out of ServerEnvironment, also | Open |
| TODO | 3 | luanti/src/unittest/test_servermodmanager.cpp | 74 | test LUANTI_GAME_PATH | Open |
| TODO | 3 | luanti/src/util/areastore.cpp | 59 | Compression? | Open |
| TODO | 3 | luanti/util/wireshark/minetest.lua | 1402 | check if other parts of | Open |
| TODO | 3 | src/benchmark/benchmark_activeobjectmgr.cpp | 118 | benchmark active object manager update costs | Open |
| TODO | 3 | src/chat.cpp | 140 | Avoid reformatting ALL lines (even invisible ones) | Open |
| TODO | 3 | src/chat.cpp | 791 | Remove the need to parse chat messages client-side, by sending | Open |
| TODO | 3 | src/client/camera.cpp | 78 | Local caching of settings is not optimal and should at some stage | Open |
| TODO | 3 | src/client/camera.cpp | 696 | This is quite primitive. It would be better to let the GPU handle | Open |
| TODO | 3 | src/client/client.cpp | 273 | Delete this code block when server-sent CSM and verifying of builtin are | Open |
| TODO | 3 | src/client/clientevent.h | 70 | should get rid of this ctor | Open |
| TODO | 3 | src/client/clientlauncher.cpp | 449 | Re-use existing structs (GameStartData) | Open |
| TODO | 3 | src/client/clientmap.cpp | 416 | Include this as a flag for an extended debugging setting | Open |
| TODO | 3 | src/client/content_cao.cpp | 1712 | Execute defined fast response | Open |
| TODO | 3 | src/client/content_cao.cpp | 1781 | Execute defined fast response | Open |
| TODO | 3 | src/client/game.cpp | 1780 | When legacy minimap is deprecated, keep only HUD minimap stuff here | Open |
| TODO | 3 | src/client/joystick_controller.cpp | 69 | find usage for button 0 | Open |
| TODO | 3 | src/client/joystick_controller.cpp | 75 | find usage for buttons 0, 1 and 4, 5 | Open |
| TODO | 3 | src/client/joystick_controller.cpp | 252 | auto detection | Open |
| TODO | 3 | src/client/localplayer.cpp | 937 | (when fixed): Set Y-speed only to 0 when position.Y < new_y. | Open |
| TODO | 3 | src/client/localplayer.cpp | 944 | This shouldn't be hardcoded but decided by the server | Open |
| TODO | 3 | src/client/mapblock_mesh.cpp | 592 | we should change the meshgen so it already applies the tile color | Open |
| TODO | 3 | src/client/mesh_generator_thread.h | 106 | Add callback to update these when g_settings changes, and update all meshes | Open |
| TODO | 3 | src/client/mesh_generator_thread.h | 140 | Add callback to update these when g_settings changes | Open |
| TODO | 3 | src/client/node_visuals.cpp | 517 | this should be done consistently when the mesh is loaded | Open |
| TODO | 3 | src/client/sound_maker.h | 14 | move this together with the game.cpp interact code to its own file. | Open |
| TODO | 3 | src/collision.cpp | 117 | Extend API to support acceleration for precise collision timing. | Open |
| TODO | 3 | src/constants.h | 92 | Use case-insensitive player names instead of this hack. | Open |
| TODO | 3 | src/content/subgames.cpp | 228 | Optimize such that `getAvailableGamePaths()` is not run N times. | Open |
| TODO | 3 | src/gamedef.h | 45 | these should be made const-safe so that a const IGameDef* is | Open |
| TODO | 3 | src/gameparams.h | 28 | unify with MainMenuData | Open |
| TODO | 3 | src/gettime.h | 25 | we should check if the function returns NULL, which would mean error | Open |
| TODO | 3 | src/gui/drawItemStack.cpp | 135 | could be moved to a shader | Open |
| TODO | 3 | src/gui/guiEngine.cpp | 661 | directly stream the response data into the file instead of first | Open |
| TODO | 3 | src/gui/guiFormSpecMenu.cpp | 3362 | getSpecByID is a linear search. It should made O(1), or cached here. | Open |
| TODO | 3 | src/gui/guiHyperText.cpp | 66 | find a way to check font validity | Open |
| TODO | 3 | src/gui/guiMainMenu.h | 19 | unify with GameStartData | Open |
| TODO | 3 | src/inventory.cpp | 548 | Implement this: | Open |
| TODO | 3 | src/irrlicht_changes/static_text.cpp | 106 | Implement colorization | Open |
| TODO | 3 | src/log.h | 196 | Search/replace these with verbose/tracestream | Open |
| TODO | 3 | src/main.cpp | 69 | luanti.conf with migration | Open |
| TODO | 3 | src/mapgen/cavegen.cpp | 14 | Remove this. Cave liquids are now defined and located using biome definitions | Open |
| TODO | 3 | src/mapgen/cavegen.cpp | 530 | 'np_caveliquids' is deprecated and should eventually be removed. | Open |
| TODO | 3 | src/mapgen/cavegen.h | 113 | 'np_caveliquids' is deprecated and should eventually be removed. | Open |
| TODO | 3 | src/mapgen/dungeongen.cpp | 417 | fix stairs code so it works 100% | Open |
| TODO | 3 | src/mapgen/mapgen_carpathian.cpp | 405 | '<=' fix from generateTerrain() | Open |
| TODO | 3 | src/mapgen/mapgen_carpathian.cpp | 543 | '<=' | Open |
| TODO | 3 | src/mapgen/mg_decoration.cpp | 134 | this is a stupid restriction, which we should lift | Open |
| TODO | 3 | src/network/mtp/impl.cpp | 32 | Clean this up. | Open |
| TODO | 3 | src/network/mtp/threads.cpp | 31 | Clean this up. | Open |
| TODO | 3 | src/nodedef.h | 839 | replace or remove | Open |
| TODO | 3 | src/objdef.h | 56 | const correctness for getter methods | Open |
| TODO | 3 | src/player.cpp | 93 | Make this generic | Open |
| TODO | 3 | src/porting.cpp | 501 | Luanti with migration | Open |
| TODO | 3 | src/porting.cpp | 566 | luanti with migration | Open |
| TODO | 3 | src/porting.cpp | 595 | luanti with migration | Open |
| TODO | 3 | src/porting.cpp | 613 | luanti with migration | Open |
| TODO | 3 | src/porting.cpp | 721 | luanti with migration | Open |
| TODO | 3 | src/porting.cpp | 725 | luanti with migration | Open |
| TODO | 3 | src/script/common/c_content.cpp | 702 | should be an error | Open |
| TODO | 3 | src/script/common/c_content.cpp | 1050 | should be an error | Open |
| TODO | 3 | src/script/common/c_content.h | 126 | rename to "read_enum_field" and replace with type-safe template | Open |
| TODO | 3 | src/script/common/c_converter.cpp | 32 | this should be turned into an error in 2026. | Open |
| TODO | 3 | src/script/common/c_converter.cpp | 66 | someone find out if it's faster to have the type check in Lua too | Open |
| TODO | 3 | src/script/common/c_converter.h | 77 | some day we should figure out the type-checking situation so it's done | Open |
| TODO | 3 | src/script/cpp_api/s_base.cpp | 555 | still needs work | Open |
| TODO | 3 | src/script/cpp_api/s_entity.cpp | 203 | this should be changed to not read the legacy place | Open |
| TODO | 3 | src/script/lua_api/l_base.cpp | 89 | Check presence first! | Open |
| TODO | 3 | src/script/lua_api/l_env.cpp | 450 | custom PointedThing (requires a converter function) | Open |
| TODO | 3 | src/script/lua_api/l_env.cpp | 954 | A similar but generalized (and therefore slower) version of this | Open |
| TODO | 3 | src/script/lua_api/l_localplayer.cpp | 483 | figure our if these are useful in any way | Open |
| TODO | 3 | src/script/lua_api/l_mainmenu.cpp | 397 | inspect call sites and make sure this is handled, then we can | Open |
| TODO | 3 | src/script/lua_api/l_metadata.cpp | 166 | this silently produces 0.0 if conversion fails, which is a footgun | Open |
| TODO | 3 | src/script/lua_api/l_object.cpp | 503 | if possible: improve the camera collision detection to allow Y <= -1.5) | Open |
| TODO | 3 | src/script/lua_api/l_object.cpp | 1874 | Track previous HUD values and only send network update on actual change | Open |
| TODO | 3 | src/script/scripting_emerge.cpp | 79 | ^ these should also be renamed to InitializeRO or such | Open |
| TODO | 3 | src/server/clientiface.cpp | 842 | this should be done by client destructor!!! | Open |
| TODO | 3 | src/server/luaentity_sao.cpp | 225 | force send when acceleration changes enough? | Open |
| TODO | 3 | src/server/luaentity_sao.cpp | 371 | give Lua control over wear | Open |
| TODO | 3 | src/serverenvironment.cpp | 948 | The blocks removed here will only be picked up again | Open |
| TODO | 3 | src/serverenvironment.cpp | 1022 | reinitializing this state every time is probably not efficient? | Open |
| TODO | 3 | src/serverenvironment.h | 124 | find way to remove this fct! | Open |
| TODO | 3 | src/servermap.cpp | 752 | Block should be marked as invalid in memory so that it is | Open |
| TODO | 3 | src/servermap.h | 22 | this could wrap all calls to MapDatabase, including locking | Open |
| TODO | 3 | src/settings.cpp | 125 | Avoid copying Settings objects. Make this private. | Open |
| TODO | 3 | src/settings.cpp | 862 | Remove this function | Open |
| TODO | 3 | src/test/test_k_d_tree.cpp | 45 | check | Open |
| TODO | 3 | src/unittest/test.cpp | 446 | this method should probably be removed | Open |
| TODO | 3 | src/unittest/test.cpp | 526 | Update to new system | Open |
| TODO | 3 | src/unittest/test.cpp | 682 | Check for AlreadyExistsException | Open |
| TODO | 3 | src/unittest/test_collision.cpp | 151 | Y-, Y+, Z-, Z+ | Open |
| TODO | 3 | src/unittest/test_collision.cpp | 374 | things to test: | Open |
| TODO | 3 | src/unittest/test_keycode.cpp | 18 | Re-introduce unittests after fully switching to SDL. | Open |
| TODO | 3 | src/unittest/test_sao.cpp | 74 | best to factor entity mgmt out of ServerEnvironment, also | Open |
| TODO | 3 | src/unittest/test_servermodmanager.cpp | 74 | test LUANTI_GAME_PATH | Open |
| TODO | 3 | src/util/areastore.cpp | 59 | Compression? | Open |
| TODO | 3 | util/wireshark/minetest.lua | 1402 | check if other parts of | Open |
