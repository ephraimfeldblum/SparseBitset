**Purpose**
- **Context**: Guidance for agent contributors working on the SparseBitset repository. This file explains high-level repo structure, build/test commands, and explicit instructions for the agent (you) to follow when making changes.

**Repository Context**
- **Language**: C/C++ with Redis module API and a C++ van Emde Boas (VEB) implementation.
- **Primary locations**: [src/bitset_module.c](src/bitset_module.c) and [src/VEB](src/VEB)
- **Tests**: Integration/flow tests are under [tests/flow](tests/flow) and are executed with the project's `Makefile` and rltest harness.

**Key Files**
- [src/bitset_module.c](src/bitset_module.c): Redis module entry points and command implementations.
- [src/VEB/VebTree.hpp](src/VEB/VebTree.hpp): Core VEB tree template and behavior.
- [src/VEB/node8.hpp](src/VEB/node8.hpp): VEB leaf implementation. Naive packed bitsets. Should be trivial to validate correctness/perf.
- [src/VEB/node16.hpp](src/VEB/node16.hpp): One of the VEB node implementations. Uses a packed FAM to store subnodes instead of a Map for perf.
- [src/VEB/node32.hpp](src/VEB/node32.hpp): One of the VEB node implementations. Ues a Set instead of a Map for perf. Key is inlined into unused padding in the value's object representation.
- [Makefile](Makefile): Build and test targets used in CI and local flows.
- [tests/flow](tests/flow): python test suite (rltest) exercising module behavior and persistence.

**How to Build & Test (quick reference)**
- Build locally (using WSL or docker is required on Windows): `make`
- Run full tests: `make test`
- Run a single flow test quickly: `make test QUICK=1 TEST=test_set_operations`
- Example Windows/WSL invocation used in this workspace:
  `wsl -d Ubuntu -e bash -lc "make clean && make test QUICK=1 TEST=test_set_operations"`

**Agent (You) — Working Rules and Instructions**
- ALWAYS start with a short todo entry via the project's todo tool to track the change.
- Before any tool call that modifies files, post a 1–2 sentence preamble describing what you'll do next.
- Use `apply_patch` to make edits. Keep changes minimal and focused on the requested task.
- After edits, run the smallest set of tests that should validate the change (targeted `make test QUICK=1 TEST=...`).
- If tests fail, attempt to fix the root cause. Limit to 3 quick edit-test cycles for the same file; escalate if still failing.
- If creating new files, include concise documentation and update relevant READMEs if necessary.
- Prefer running tests inside WSL or Docker if on Windows. Redis is not Windows-compatible.
 - Emit logs using `RedisModule_Log` only; do not use `fprintf`, `std::cerr`, `std::cout`, or other stdio/iostream channels in module or VEB code. This ensures logs are captured consistently by Redis and test harnesses.

**Testing Strategy**
- Reproduce failing tests locally using QUICK mode and using the specific test with TEST to limit runtime.
- Use tests/flow logs at `tests/flow/logs` and `tests/flow/rltest_logs` for historical failures and RDB artifacts.
- For performance changes in `src/VEB`, add small benchmarks under `tests/benchmarks` or reuse existing harness.

**Useful Commands**
- Build: `make`
- Clean: `make clean`
- Run a single flow test: `make test QUICK=1 TEST=...`
- Run all flow tests: `make test QUICK=1`
- Run all flow tests in all configurations: `make test`
- Run in Docker: `./run_in_docker.sh`

**Coding & PR Guidelines for the Agent**
- Keep patches minimal and narrowly scoped.
- C++ standard used is C++23.
- Preserve existing style and APIs unless explicitly requested by the user.
- Run only the tests relevant to your change before proposing broader test runs.

**Notes**
- Logs and previous failing runs are available in `tests/flow/logs` and `tests/flow/rltest_logs` — consult them when debugging flakiness or persistence issues.
- If a change requires environment-specific steps, document them in this file briefly.

File created to help future agent-driven edits and to centralize workflow expectations.
