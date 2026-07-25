# Host test interfaces

## Purpose

Host tests compile production C modules with desktop compilers and replace only
the hardware boundary. Tests use public APIs and do not include private source
files or inspect `static` state.

## Current seams

| Production contract | Host replacement | Covered behavior |
|---|---|---|
| `HalGpio` | `fakes/fake_hal_gpio.*` | Level writes, reads, call counts, next-call failures |
| Event log sink | Test-local sink callback | Backpressure and retry without record loss |
| Parameter image | Byte buffer | CRC, schema, range, and transactional load failures |
| Task manifest | Pure application table | Period, priority, deadline, and stack budget |

## Fake design rules

- Implement the same public contract as the production adapter.
- Keep state in an explicit fake context; do not use hidden globals.
- Record observable calls and arguments.
- Fault injection must be deterministic, normally “fail the next call”.
- A fake must not reproduce the production algorithm.
- Tests must assert behavior at the public boundary.
- Hardware integration remains a separate no-prop bench test.

## Add a test

1. Extract or identify a hardware-independent public interface.
2. Add a `test_<module>.c` file.
3. If hardware is involved, add a focused fake under `fakes/`.
4. Register the target with `add_quadcopter_host_test()` in `CMakeLists.txt`.
5. Compile with warnings as errors.
6. Add the new behavior and failure cases to `docs/TESTING.md`.
7. Run the unified quality gate from the repository root.

The repository validator fails if a `test_*.c` file is not registered in the
host CMake file.

The CMake helper explicitly undefines `NDEBUG`, so `assert()` remains active in
Debug and Release builds. Do not override that option for individual tests.
