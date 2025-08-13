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
- [x] Persistence scaffolding: in‑memory Ledger + Outbox; SubmitRouter hooks; outbox size‑based rotation; periodic ledger snapshots
- [x] Outbox pruning on acceptance wired from runner; persistent file rewrite on accept
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

- [x] TLS connectivity on Windows: OpenSSL backend enabled via vcpkg; SNI and CRLF in Stratum; CA bundle wired (`certs/cacert.pem`) with hostname verification; config ordered to prefer non‑TLS first with TLS as optional fallback; verified: NiceHash TLS stable, F2Pool plain 1314 stable, ViaBTC plain 3333 stable (TLS 443 connectivity dependent on network route)

### Next Up
- Scheduler/backpressure refinement: stronger EWMA penalties, anti‑starvation guarantees, and health‑based pool weighting.
- HTTP metrics endpoint (read‑only JSON): expose pool health/backpressure, kernel attrs, occupancy, device mem.
- GBT regtest e2e: mine trivial target, build full block hex with witness commitment, `submitblock` acceptance; disable coinbase synth by default.
- CUDA auto‑tune profile: sweep harness + persist per‑GPU tuned params; measure unroll variants and occupancy; docs for tuning.
- Multi‑GPU: enumerate devices; one engine/scheduler per GPU; fair source allocation.
- TLS hardening: config flags for verify on/off and custom CA path; consider native Schannel backend on Windows as alternative to OpenSSL.
- Outbox/Ledger durability: CRC + fsync on append; gzip rotation; faster replay indexing.
- Submit idempotency: cross‑restart dedup and per‑source submit keys.
- Pool policy polish: version‑rolling mask negotiation, ntime rolling limits, varDiff tracking/metrics.
- Hot config reload: env overrides and live reload for pools and tuning knobs.
- Docs: CUDA tuning guide, ops runbook, pool compatibility notes, Windows build (vcpkg/CA) steps.
- Performance: reduce global atomics in hit paths; refine shared/local buffers and hit ring sizing.


