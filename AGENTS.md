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
- [src/VEB/node16.hpp](src/VEB/node16.hpp): VEB node implementation. Uses a packed FAM to store subnodes (node8) instead of a Map for perf.
- [src/VEB/node32.hpp](src/VEB/node32.hpp): VEB node implementation. Ues a Set instead of a Map for perf. Key is inlined into unused padding in the subnode's (node16) object representation.
- [Makefile](Makefile): Build and test targets used in CI and local flows.
- [tests/flow](tests/flow): python test suite (rltest) exercising module behavior and persistence.

**How to Build & Test (quick reference)**
- Build locally (using WSL or docker is required on Windows): `make`
- Run full tests: `make test`
- Run a single test file: `make test TEST=test_basic_mutations`
- Run a single flow test: `make test TEST=test_toarray_order:test_toarray_order_large`
- Example Windows/WSL invocation used in this workspace:
  `wsl -e bash -lc "make clean && make test QUICK=1"`

**Agent (You) — Working Rules and Instructions**
- ALWAYS start with a short todo entry via the project's todo tool to track the change.
- Before any tool call that modifies files, post a 1–2 sentence preamble describing what you'll do next.
- Use `apply_patch` to make edits. Keep changes minimal and focused on the requested task.
- After edits, run the smallest set of tests that should validate the change (targeted `make test QUICK=1 TEST=...`).
- If tests fail, attempt to fix the root cause. Limit to 3 quick edit-test cycles for the same file; escalate if still failing.
- If creating new files, include concise documentation and update relevant READMEs if necessary.
- Prefer running tests inside WSL or Docker if on Windows. Redis is not Windows-compatible.
- Emit logs using `RedisModule_Log` only; do not use `fprintf`, `std::cerr`, `std::cout`, or other stdio/iostream channels in module or VEB code. This ensures logs are captured consistently by Redis and test harnesses.
- VEB nodes (`Node16`, `Node32`, `Node64`) **MUST** be manually destroyed using their `.destroy(alloc)` method before they go out of scope or are reassigned. Failure to do so will cause memory leaks and, in debug builds, trigger an assertion.

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

**VEB Node Invariants**
- **Min/Max ownership:** The `min` and `max` values must be stored in the node itself and must never be stored inside cluster containers. If an operation would remove a node's `min` or `max`, the node must promote a replacement value from its clusters (for example, pull the next `min`/`max` from the appropriate cluster) so the node's `min`/`max` remain authoritative. These in-node `min`/`max` values are the single source of truth and are not copies.
- **Nodes are never empty:** A node is never considered empty except during the fleeting moment between an operation that removed its last elements and the call site that destroys the node. Assume nodes always contain at least the stored `min`/`max`.
- **Summary is the source of truth:** The `summary` index is authoritative for which clusters exist. If the `summary` indicates a cluster exists at a key, callers do not need to re-validate that by checking `find()` return values or similar — rely on the summary's membership operations (e.g., `contains`, `min`, `max`, `successor`, `predecessor`) to reason about clusters.
- **Summary is itself non-empty:** The `summary` is a proper node and therefore must contain at least one element whenever `cluster_data_` exists. Treat `summary` as non-empty — its `min`/`max` and membership ops are valid.
- **Clusterless does not mean empty:** A node with no clusters still contains valid `min` and `max` values and therefore must be treated as non-empty. Do not infer emptiness solely from the absence of clusters.
- **Summary implies clusters exist and are non-empty:** Because `summary` is authoritative for cluster membership, and `summary` is non-empty, it can be assumed that clusters are also non-empty rather than defensively re-checking cluster containers.

**Node-Specific Behavior**
- **Node8:** Node8 de facto is a PoD 256-bit bitset, conceptually `alignas(32) uint64_t[4]`. The implementation is de jure based on the API provided by `xsimd` for handling 256-bit data in batches of 64 bits. Operations on it should be optimized using SIMD instructions provided by the `xsimd` library.
- **Node16:** A bitset that can store up to 2^16 = 6.5e4 bits. Is used by the Tree as its underlying storage iff the range of values stored within exceeds the max value storable in a `Node8`. Uses `Node8` as subnodes. Clusters are stored in a packed dynamic array to minimize memory overhead. It avoids a Map by using the `summary` to compute the physical index of a cluster via `summary.count_range`. This keeps the structure at exactly 16 bytes.
- **Node32:** A bitset that can store up to 2^32 = 4.3e9 bits. Is used by the Tree as its underlying storage iff the range of values stored within exceeds the max value storable in a `Node16`. Uses `Node16` as subnodes. For a sparse 32-bit universe, it uses a HashSet of `Node16` clusters. Each `Node16` cluster inlines its own `key` to serve as the hash/equality key, avoiding the overhead of separate key-value pairs. Total size is 16 bytes.
- **Node64:** A bitset that can store up to 2^64 = 1.8e19 bits. Is used by the Tree as its underlying storage iff the range of values stored within exceeds the max value storable in a `Node32`. Uses `Node32` as subnodes. It utilizes a HashMap to manage `Node32` clusters. A `Node32` summary tracks which clusters are non-empty. Total size is 24 bytes. Remains largely unused, as Redis's Bitmap data structure - that this bitset module is competing with - can itself store only up to 2^32 bits.

**Coding & PR Guidelines for the Agent**
- Keep patches minimal and narrowly scoped.
- Use C++23 and C11: Ensure all new code adheres to the C++23 standard for the core library and C11 for the Redis module wrapper.
 - C Interface: Follows standard Redis Module conventions using snake_case and explicit error handling via Redis return codes.
 - C++ Core: Utilizes modern C++23 features (std::variant, std::visit, templates) while maintaining low-level control over memory via custom allocators and manual object destruction.
- Maintain Invariants: The min and max values must be stored in the node itself and never inside cluster containers. The summary is the authoritative source for cluster existence.
- Optimization: Optimizing memory consumption is mission-critical! This bitset is designed to reside in-memory. It should go out of its way to ensure that users are not paying for more memory than they are using. Optimizing for time is very important, but not if the gains are low at the expense of increased memory usage.
 - Examples of specific optimizations used include: Extensive use of SIMD (via xsimd), Flexible Array Members (FAM), and bit manipulation to achieve $O(\log \log U)$ performance.
 - Performance First: Ensure operations maintain top speed. Avoid unnecessary allocations or scans that could degrade performance.
 - Manual Memory Management: All vEB nodes (Node16, Node32, Node64) must be manually destroyed using their .destroy(alloc) method before their lifetime ends.
 - SIMD & Bit Manipulation: When working with leaf nodes (Node8), use xsimd and bitwise intrinsics (like std::popcount) to optimize performance.
- Preserve existing style and APIs unless explicitly requested by the user.
 - Follow Naming Conventions: Use snake_case for all functions, methods, and variables (e.g., insert_value, cluster_data_).
 - No Unnecessary Comments: Code should be self-documenting. Avoid adding comments unless they explain complex algorithmic logic or non-obvious optimizations.
- Test-Driven Development: All new features or bug fixes must include functional tests in ./tests/flow/ using the RLTest framework.
 - Run only the tests relevant to your change before proposing broader test runs.
- Build System Integrity: Maintain the Makefile and CMake configurations. Ensure the project builds with `make` and passes `make test QUICK=1` before submission.

**Notes**
- Logs and previous failing runs are available in `tests/flow/logs` — consult them when debugging flakiness or persistence issues.
- If a change requires environment-specific steps, document them in this file briefly.

File created to help future agent-driven edits and to centralize workflow expectations.
