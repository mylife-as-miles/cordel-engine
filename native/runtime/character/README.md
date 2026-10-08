# CORDEL Phase 2.2 capsule motor

Reusable C++20 query motor, selected Jolt adapter, no Jolt gameplay types or
CharacterVirtual. See [the report](../../../docs/cordel/PHASE_2_2_CHARACTER_MOTOR.md)
for movement rules, limits and executed evidence. New code/greybox data is MIT;
existing engine/library notices are preserved.

From the repository root, after the documented BOOTSTRAP environment setup:

```sh
export PATH="$PWD/.venv/bin:$PATH"
cmake -S native/phase1_host -B build/phase1-host -DCORDEL_OFFSCREEN_ONLY=ON
cmake --build build/phase1-host -j 4
ctest --test-dir build/phase1-host --output-on-failure
bash native/runtime/character/tools/run_phase2_2.sh tmp/new-phase2_2
```

The complete gate uses the selected Jolt host and separately reruns both Phase
2.1 query adapters, Phase 1 smoke, Ren'Py viewport and baseline. Output must be new
or empty. Every child stage has a timeout with process-group cleanup. It requires
no extra Python imaging package, privileged installation or new physics library.

Desktop development build: omit `CORDEL_OFFSCREEN_ONLY`, provide SDL's documented
platform development prerequisites, and run:

```sh
build/phase1-host/cordel-native-host --motor --output tmp/motor-desktop
build/phase1-host/cordel-native-host --motor-zone ramp35 --output tmp/ramp-desktop
```

W/S move along -Z/+Z; A/D along -X/+X. Left Shift selects 7 m/s instead of
4.5 m/s. Movement is world-relative. Left click requests relative mouse look for
the **stationary inspection camera**; Escape releases it. Space/Ctrl do not move
the motor. There is no jump or follow/orbit camera. F6 stalls for 250 ms.

Zones: `flat`, `long_flat`, `ramp10/25/35/45/55`, `step10/20/30/45/60`,
`ceiling`, `door`, `wide_door`, `ledge`, `sensor`. Each invocation creates one world
and motor; zones select debug initial poses, never collision rules. The query
fixture has deliberately separated areas; use a new launch to inspect another.

`--render-hz 30`, `60`, `120` control the sleep fallback (60 also tries verified
VSync); `--uncapped` removes pacing. The world always ticks at 60 Hz, max three
ticks per render iteration. `--seconds N` bounds diagnostic runs.

Reproducible software runs:

```sh
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
build/phase1-host/cordel-native-host --motor-test --output tmp/new-motor-runtime
build/phase1-host/cordel-native-host --motor --narrative-test --output tmp/new-motor-story
build/phase1-host/character/cordel-motor-tests --output tmp/new-motor-logic
```

`--motor --narrative` uses the native console presentation: Enter acknowledges,
1/2 chooses, WASD still moves the motor. Story waits never pause simulation.
The beacon event uses native motor proximity in interactive mode, deterministic
world-owned triggering in the test. The original host modes remain available
without `--motor`. Offscreen success does not verify desktop input or hardware.
