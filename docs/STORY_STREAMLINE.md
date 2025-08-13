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
- [x] Milestone 0: Bootstrap + test harness (CMake, GoogleTest)
- [x] Milestone 1: Observability (structured logger) and endianness helpers
- [x] Milestone 2: Targets (compact nBits → LE u32[8]) with vectors
- [x] Milestone 3: Work Source Registry skeleton with `gen` bump
- [x] Milestone 4: CUDA engine stubs link cleanly when CUDA off
- [x] Milestone 5: Config skeleton and defaults
- [x] Normalizer pieces: midstate utilities and Merkle root computation
- [x] Submit path: CPU SHA‑256/sha256d utilities and `SubmitRouter` that CPU‑verifies before routing
- [x] Persistence scaffolding: in‑memory Ledger + Outbox; SubmitRouter hooks
- [x] Tests green: endianness, targets, registry, logging, metrics, CPU verify, midstate, merkle, submit router
- [x] Adapters skeleton: `AdapterBase`, `StratumAdapter` façade with basic state machine and ingest/poll
- [x] GBT path: `GbtAdapter` ingest/poll queue and `GbtRunner` that polls `getblocktemplate` with `rules=["segwit"]` using cookie auth by default; `GbtSubmitter` assembles and calls `submitblock`.
- [x] Normalizer completion scaffolds: coinbase assembly + witness commitment placeholder; rolling/ntime caps and extranonce packing tests
- [x] Registry integration: adapter → normalizer → registry path covered by unit test
 - [x] Adapter policies: varDiff share target, version mask, ntime caps, `clean_jobs` handling (applied in `ingestJobWithPolicy`)
 - [x] Multipool strategy router (failover, round-robin) with tests
 - [x] Persistence helpers: Ledger JSONL save/load, Outbox binary append/load with tests
 - [x] CUDA engine planning: multi-job launch plan helpers + tests
 - [x] Cache: `CacheManager` and `VramPages` scaffolds with tests
 - [x] PredictabilityWorker: decision engine with tests

### Next Up
- Multi-source concurrency is implemented (one runner per pool + optional GBT). Continue refining scheduler backpressure and ensure no starvation.
- Submit pipeline: correlate accepted submits to `(work_id, nonce)` to prune Outbox and rotate files; finalize Ledger snapshot format.
- CUDA engine: occupancy tuning, per-SM local hit buffers to reduce global atomic contention; extend constant-memory usage; auto-tune `threads_per_block` and `nonces_per_thread`.
- GBT: complete witness commitment path and full block assembly for `submitblock` on hits.


