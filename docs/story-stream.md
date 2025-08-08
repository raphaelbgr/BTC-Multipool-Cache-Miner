## Story Stream (Backlog) — Multi‑Pool BTC CUDA Miner

This document is the streamlined backlog of epics and stories derived from `PRD.md`, `ARCHITECTURE.md`, `ROADMAP.md`, `DoD.md`, `TESTING_OPS.md`, and `CI_PLAN.md`. It is organized by milestones (M1–M6) and ordered for execution. Each story includes crisp acceptance criteria (AC) aligned with DoD, and dependencies to ensure sequencing.

### Conventions
- P0 = critical path for next milestone; P1 = important; P2 = nice‑to‑have
- AC references the DoD sections where applicable
- Tests reference `TESTING_OPS.md` suites (Unit, Integration, Fault, Bench) and `CI_PLAN.md`

---

## Epic M1 — Bootstrap & Tooling

Goal: Establish repo, configuration, logging/metrics scaffold, CPU‑only test harness, and CI matrix.

1) P0 — Establish repository and licensing foundation
   - As a maintainer, I need the project seeded with license compliance so we can safely ship and contribute.
   - AC:
     - SPDX headers in new source files; `THIRD_PARTY.md` present and populated
     - CGMiner/GPL compliance notes added in `README.md` (Licensing section) and CI license check wired
     - Links to external `external/cgminer-*` preserved; no license violations in build outputs
   - Tests/CI: License compliance job green (see `CI_PLAN.md`)

2) P0 — Repository skeleton and configuration loader
   - As a developer, I need an INI loader that maps to sections in `docs/config-example.ini` so I can run locally.
   - AC:
     - Loads `[sources]`, `[cuda]`, `[cache]`, `[cache.vram]`, `[predict_worker]`, `[scheduler]`, `[submit]`, `[ledger]`, `[outbox]`
     - Validates required fields; fail‑fast with actionable errors
     - Example config from `docs/config-example.ini` parses successfully
   - Tests: Unit for config parsing and validation error cases

3) P0 — Logging and minimal metrics scaffold
   - As an operator, I need structured JSON logs and a metrics interface, so I can observe behavior.
   - AC:
     - Structured logs with redactable secrets; log levels; correlation of events by source
     - Metrics interface with stubs for: hashrate, accepts/rejects/stales, kernel time, occupancy, PCIe BW, VRAM usage, predict worker duty cycle
   - Tests: Unit smoke for log redaction and metrics counters

4) P0 — CI: Windows + Linux, CPU‑only unit test harness
   - As a contributor, I need fast feedback across platforms.
   - AC:
     - Matrix builds (Windows Server 2022, Ubuntu 22.04)
     - CPU‑only unit tests run without CUDA
     - Artifacts archived on build job
   - Tests/CI: All jobs green per `CI_PLAN.md`

5) P1 — Developer docs pass
   - As a developer, I need README Quick Start and contribution notes refined to match current state.
   - AC: README Quick Start validated on a clean machine using `docs/config-example.ini`

Dependencies: None external; sets foundation for all subsequent epics.

---

## Epic M2 — Adapters (Stratum + GBT)

Goal: Implement adapter layer with robust connectivity and policy handling.

6) P0 — Core adapter abstractions (`AdapterBase`, `StratumAdapter`)
   - As a developer, I need a common adapter contract and a generic Stratum implementation.
   - AC:
     - `AdapterBase` lifecycle: connect, receive job/update, submit, backoff/retry
     - `StratumAdapter`: varDiff support, clean_jobs handling, rolling/version/ntime caps parsed
     - Reconnect logic with exponential backoff; jittered retries
   - Tests: Unit for schema parsing and policy flags; Integration mock Stratum server

7) P0 — Pool adapter profiles (at least 2 real pools)
   - As an operator, I need working profiles for two pools to validate multi‑source.
   - AC: Profiles tested against mock or test endpoints; submit schema variances handled
   - Tests: Integration per pool profile (mock)

8) P0 — `GbtAdapter` (BIP22/23 with segwit rules)
   - As a solo‑bias miner, I need GBT support with witness commitment correctness.
   - AC: `rules=["segwit"]` honored; submitblock success path; tip mismatch invalidation hooks
   - Tests: Integration with regtest `bitcoind`

9) P1 — Connection health and observability
   - AC: Connection metrics and state logs; backoff reasons visible

Dependencies: Requires M1 logging/metrics and config loader.

---

## Epic M3 — Normalizer & Work Registry

Goal: Deterministic normalization and in‑place hot‑swap registry.

10) P0 — Normalizer: coinbase/witness, merkle, endianness, targets, midstate
    - As a kernel consumer, I need normalized `WorkItem` + `GpuJobConst` for correctness and speed.
    - AC:
      - Coinbase assembly with BIP141 witness commitment
      - Endianness normalization for `prevhash` and `merkle_root` to LE
      - `share_target` (varDiff) and `block_target` (nbits) computed as LE `u32[8]`
      - Midstate precompute validated against vectors
    - Tests: Unit (header build 80B, SHA‑256d vectors, targets, rolling clamps, extranonce packing)

11) P0 — `WorkSourceRegistry` in‑place hot‑swap
    - As a scheduler, I need fixed slots updated in place with `gen` bump last and async device copy of only changed slots.
    - AC: Absorbs updates within ≤1 kernel batch; `active`/`found_submitted` flags correct; invalidation on clean_jobs or tip mismatch
    - Tests: Integration hot‑swap behavior; device copy scheduling hooks

12) P1 — Schema/property validation & bounds checks
    - AC: Robust input validation with precise errors; fuzz/sanity tests for bounds

Dependencies: M2 adapters producing `RawJob`; M1 tests/CI.

---

## Epic M4 — CUDA Engine & Submit Routing

Goal: Cross‑job per‑nonce kernel with CPU verification and adapter routing; end‑to‑end share submission on regtest.

13) P0 — CUDA kernel: per‑nonce across jobs + hit ring buffer
    - As a miner, I need the kernel to test the same nonce across all active jobs each micro‑batch.
    - AC:
      - Grid mapping: y = job index, x = nonce offsets; micro‑batches ~0.5–3 ms
      - Per‑thread double SHA‑256; compare to share+block targets; write hits to lock‑free ring
      - Constants in constant/read‑only memory; minimal branch divergence
    - Tests: Bench smoke (CPU shim or GPU runner), correctness checks using known vectors

14) P0 — `SubmitRouter` with CPU verification
    - As a safety gate, I need CPU to rebuild header and verify SHA‑256d before submission, routing to the originating adapter.
    - AC: Idempotent routing; rejects duplicates; correct source mapping
    - Tests: Integration with adapters; duplicate suppression

15) P0 — Regtest end‑to‑end share submission
    - As a system tester, I need a full flow that finds and submits valid shares in regtest.
    - AC: End‑to‑end run submits valid shares; acceptance/reject metrics recorded
    - Tests: Integration on regtest; CI job optional GPU or CPU shim path

Dependencies: M3 normalizer/registry to supply kernel‑ready data.

---

## Epic M5 — VRAM Cache & Predictability Worker

Goal: Mandatory VRAM cache with watermarks and a safe, low‑priority precompute worker.

16) P0 — VRAM `CacheManager`
    - As a performance owner, I need ~85% VRAM utilization with safety margins and page‑wise promote/evict.
    - AC:
      - Respects `min_free_mib`; low/high watermarks trigger background actions
      - Holds midstates, normalized template/variant pages, dedup filters, header‑variant staging
      - Optional RAM spill OFF by default
    - Tests: Fault injection for VRAM pressure; behavior per watermarks

17) P0 — `PredictabilityWorker` (safe precompute)
    - As a stability owner, I need a low‑priority stream that double‑buffers and swaps epochs only when `gen` stable.
    - AC: Throttles by GPU util and watermarks; CPU fallback for partial work; cadence ~60s; horizon ~120s
    - Tests: Integration with kernel cadence; verify no interference and safe abort on `gen` change

18) P1 — Dedup filter tuning
    - AC: Configurable Bloom/Cuckoo parameters; targeted FPR and memory budget verified

Dependencies: M4 kernel interfaces and metrics.

---

## Epic M6 — Persistence, Observability, Hardening

Goal: Durable ledger/outbox, metrics/logging, and system hardening with benchmarks and docs.

19) P0 — Ledger (mmap snapshot) and Outbox (crash‑safe replay)
    - As an operator, I need crash‑safe replay of pending hits and duplicate suppression.
    - AC: O(1) job map; dedupe guarantees; replay on start; configurable paths from config
    - Tests: Integration restart scenarios; duplicates dropped; pending hits replayed

20) P0 — Observability completeness
    - As an operator, I need metrics and structured logs covering kernel and adapters.
    - AC: Metrics for hashrate/source, accept/reject/stale, kernel timing/occupancy, PCIe BW, VRAM, predict worker duty cycle
    - Tests: Metrics and logs validated; dashboards/docs examples captured

21) P0 — Fault injection + performance envelope
    - As a quality owner, I need controlled tests to validate stability and performance goals.
    - AC: Disconnect storms, clean_jobs storms, VRAM pressure, scheduler backpressure; ≤5% regression vs single‑source baseline
    - Tests: Fault and Bench suites per `TESTING_OPS.md`

22) P1 — Docs and examples finalized
    - AC: README, PRD, Architecture, Testing‑Ops updated; example configs verified; Roadmap/CHANGELOG updated

Dependencies: Prior epics completed; feeds release readiness.

---

## Cross‑Cutting Definition of Done (applies to all stories)
- Functional and non‑functional criteria from `DoD.md` satisfied where relevant
- Unit tests added/updated; Integration where the surface crosses process boundaries
- Logging is structured and secrets are never logged
- CI green on Windows and Linux for touched components

---

## Initial Sprint Cut (suggested)
- Sprint 1 (M1 focus): Stories 1–4, plus story 6 (adapters skeleton stubs) to unblock M2
- Sprint 2 (M2 focus): Stories 6–9, plus story 10 (normalizer start)

Re‑plan each sprint based on risk and findings; M3–M4 carry more technical uncertainty and may need spikes.

---

## Traceability Matrix (high‑level)
- PRD Goals → Epics M2–M5 (multi‑source, normalization, cache, predictability worker) and M4 (kernel/submit)
- PRD Acceptance Criteria → ACs in stories 10–21
- ROADMAP M1–M6 → Epics M1–M6 mapping 1:1
- TESTING_OPS suites → Tests listed under each story
- CI_PLAN → Stories 4, 15, 21, and license checks in story 1


