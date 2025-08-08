## Development Story Streamline

### Milestone 0 — Bootstrap and Test Harness
- Goal: Project compiles and runs unit tests on Windows/Linux without CUDA dependency.
- Deliverables:
  - CMake-based build with optional CUDA.
  - GoogleTest wired in; `ctest` runs.
  - Header-only CUDA stubs compile even if CUDA is missing.
- Tests:
  - Header build tests compile.

### Milestone 1 — Observability and Utilities
- Goal: Structured JSON logging and byte endianness helpers.
- Deliverables:
  - `src/obs/log`: minimal structured logger using `nlohmann::json`.
  - `src/normalize/endianness`: endian swap helpers for 32/64-bit values and 32-byte arrays.
- Tests:
  - Endianness round-trip correctness.
  - Log formatting smoke test (JSON shape only, no file I/O).

### Milestone 2 — Targets and Compact Encoding
- Goal: Correct translation between Bitcoin compact difficulty (`nBits`) and 256-bit targets normalized to LE `u32[8]`.
- Deliverables:
  - `src/normalize/targets`: `compactToTargetU32LE` and helpers.
- Tests:
  - Genesis vector: `0x1d00ffff` target equals `00000000ffff0000...` (big-endian hex).
  - Edge cases: underflow/overflow clamps and zero mantissa handling.

### Milestone 3 — Work Source Registry (Skeleton)
- Goal: Fixed-slot registry with in-place hot-swap and generation bump (`gen`).
- Deliverables:
  - `src/registry/work_source_registry`: types + registry with set/get snapshot API.
- Tests:
  - Set/update bumps `gen` and preserves slot constraints.

### Milestone 4 — CUDA Engine Stubs
- Goal: CUDA entry points and compile-time stubs for environments without CUDA.
- Deliverables:
  - `src/cuda/sha256d.cuh` and `src/cuda/engine.h/.cu` (stubbed ops).
- Tests:
  - Header build test; no GPU math assertions yet.

### Milestone 5 — Config Skeleton
- Goal: Types for config and a placeholder INI reader (actual parsing TBD).
- Deliverables:
  - `src/config/config.h/.cc` with minimal `AppConfig` and loader stub.
- Tests:
  - Type presence and basic defaults.

### Definition of Done for this phase
- All unit tests green for the milestones above.
- Code adheres to repo’s coding rules; no secret leakage in logs.

### Progress Update (current)
- Completed Milestone 0: Bootstrap + test harness (CMake, GoogleTest)
- Completed Milestone 1: Observability (structured logger) and endianness helpers
- Completed Milestone 2: Targets (compact nBits → LE u32[8]) with vectors
- Completed Milestone 3: Work Source Registry skeleton with `gen` bump
- Completed Milestone 4: CUDA engine stubs link cleanly when CUDA off
- Completed Milestone 5: Config skeleton and defaults
- Added Normalizer pieces: midstate utilities and Merkle root computation
- Added Submit path: CPU SHA‑256/sha256d utilities and `SubmitRouter` that CPU‑verifies before routing
- Tests: 15/15 green across endianness, targets, registry, logging shape, metrics, CPU verify, midstate, merkle, and submit router

### Next Up
- Adapters skeleton
  - `AdapterBase` interface and `StratumAdapter` façade (no network yet)
  - Basic state machine unit tests (connect → subscribe → authorize flow simulation)
- Normalizer completion
  - Coinbase assembly scaffold and witness commitment placeholder
  - Rolling/ntime caps tests and extranonce packing tests
- Registry integration
  - Adapter → Normalizer → Registry update path with `gen` bump ordering tests


