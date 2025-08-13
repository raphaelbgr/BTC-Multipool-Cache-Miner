## Product Requirements Document (PRD)

### Title
Multi‑Pool BTC CUDA Miner (CGMiner‑based)

### Summary
Research/education‑grade Bitcoin SHA‑256d GPU miner that maximizes effective coverage and first‑block discovery probability by testing the same nonce candidate across all concurrently active work sources (multiple Stratum pools and a local full node via GBT), with a mandatory per‑GPU VRAM cache, safe predictability precompute, simple SOLID architecture, and in‑place job hot‑swapping.

### Goals
- Solo‑first discovery mindset while running multiple concurrent work sources.
- Cross‑job, per‑nonce CUDA testing with strict per‑job routing.
- Mandatory per‑GPU VRAM cache (~85% utilization) for midstates/templates/dedup; adaptive watermarks and safety margins.
- Clean, simple, SOLID architecture; DI‑friendly; adapters per pool; in‑place job hot‑swapping.

### Non‑Goals
- Predicting winning nonces on SHA‑256d.
- Multi‑submit abuse or violating pool ToS.

### Users and Use Cases
- Miners and researchers wishing to experiment with GPU mining strategies on regtest/signet/testnet or mainnet (educational/research focus). 
- Developers extending pool adapters or experimenting with multi‑source scheduling, header normalization, and CUDA kernel strategies.

### Assumptions & Constraints
- GPU SHA‑256d is noncompetitive vs ASICs; treat as research/education.
- Windows and Linux supported; CUDA‑capable GPUs with sufficient VRAM.
- Based on CGMiner heritage; GPL compliance required.
- Strict adherence to BTC protocol: BIP22/23 GBT with segwit rules, BIP141 witness commitment in coinbase, correct 80‑byte header construction, SHA‑256d test vectors, little‑endian target compare on u32[8].

### Scope (Functional Requirements)
1) Multi‑source work ingestion
   - N Stratum pools (varDiff; extranonce1/2; version/ntime caps; submit schema differences) and one local node (GBT).
   - Adapters: `AdapterBase → StratumAdapter (generic) → PoolAdapters/*` and `GbtAdapter`.
   - Stratum specifics implemented:
     - varDiff updates applied via `mining.set_difficulty` → `share_target` distinct from `block_target`.
     - `mining.configure` negotiation for `version-rolling.mask`; mask propagated to normalization (`vmask`).
     - `mining.notify` parsing honors `clean_jobs`, extranonce sizes, and sets minimal `ntime` rolling caps. Coinbase is assembled from `coinb1` + `extranonce1` + `extranonce2` + `coinb2` (with a zeroed `extranonce2` for midstate path) and Merkle root is recomputed from the branch.
   - Automatic recovery on disconnects; respects pool `clean_jobs`; honors rolling/version/ntime caps.

2) Normalization (mandatory before GPU)
   - Convert RawJob to uniform `WorkItem` and `GpuJobConst`.
   - Endianness normalization on `prevhash` and `merkle_root` to kernel‑ready LE.
   - Targets: share_target (varDiff) and block_target (nbits) as LE u32[8].
   - Rolling clamps for version/ntime per source policy; extranonce sizes/packing; full coinbase assembly and merkle root.
   - Midstate precompute; `header_first64` populated for accurate tail hashing on device.

3) WorkSourceRegistry (continuous testing + hot‑swap)
   - Fixed slots `work[SOURCES]`, `gpuJobs[SOURCES]`; in‑place updates; increment `gen` last; `active=1`; `found_submitted=0`.
   - Copy only updated slot to device asynchronously; absorb updates within ≤1 kernel batch. 
   - Invalidate on Stratum `clean_jobs` or GBT tip mismatch; keep solved jobs until superseded.

4) CUDA Engine (same nonce across all jobs)
   - Grid mapping: y‑dimension = job index; x‑dimension = nonce offsets; micro‑batches (~0.5–3 ms).
   - Per thread: assemble header (job constants + nonce) → double SHA‑256 → compare to share and block targets; record hits in a ring buffer.
    - Current status: device SHA‑256d implemented (including midstate tail path); `DeviceJob` uploaded per job; constant‑memory fast path for up to 64 jobs; device hit ring with host drain; per‑block shared‑memory hit buffers reduce global atomic contention; host reconstructs header to CPU‑verify before submit. Auto‑tune adjusts `nonces_per_thread` and desired threads/job and is now occupancy‑aware via a kernel occupancy query; further unrolling optimization TBD.
   - Constants in __constant__/read‑only memory; minimize branching; round unrolling.

5) Scheduler
- All sources active; weighted fairness (configurable per source; optional solo bias).
- Backpressure reduces slices for laggy/rejecting sources; micro‑batch duration auto‑tuned for throughput vs responsiveness.
- Current status: multi‑source runners per pool, per‑slot registry, routing by `work_id`; per‑source backpressure (rejects/latency) integrated in runner with decay; basic auto‑tuning active.

6) CacheManager (VRAM mandatory)
   - Per‑GPU dynamic target ≈ 85% available VRAM; respect `min_free_mib`; watermarks trigger async promote/evict by page size (MiB).
   - Contents: midstates, normalized template/variant pages, per‑job Bloom/Cuckoo dedup, header‑variant staging.
   - Mempool cap and OOM avoidance; optional RAM spill OFF by default; multi‑GPU shards per device.

7) PredictabilityWorker (safe precompute)
   - Low‑priority CUDA stream; double‑buffer shadow pages; epoch swap at batch boundaries if `gen` unchanged; discard on change.
   - Adaptive throttling via GPU util and VRAM watermarks; CPU fallback for partial tasks; cadence ~60 s; lookahead ~120 s.

8) SubmitRouter, Ledger & Outbox
   - CPU rebuild and verify header pre‑submit; idempotent routing to the originating adapter.
- Persistent O(1) job map (ledger) with periodic JSONL snapshots and rotation policy (size and/or time‑based); dedupe jobs; crash‑safe outbox with replay; rotation by size, optional time‑based interval, and optional rotate‑on‑start; acceptance‑based pruning wired via runner callbacks.

9) Observability
- Metrics: per‑source hashrate (EWMA), share accept/reject/stale, kernel time, desired threads and nonces per thread, CUDA mem free/total, PCIe/VRAM usage.
   - Structured JSON logs; per‑hit traces; backoff/retry reasons.

10) Configuration
   - INI‑style config with sources, CUDA, cache, predict worker, scheduler, submit, ledger, outbox.

### Non‑Functional Requirements
- Stability: no crashes on disconnects; automatic recovery; no multi‑submit of identical work.
- Performance: ≤5% throughput regression vs single‑source baseline; micro‑batch ≤3 ms typical; ≤1 batch to absorb job hot‑swap.
- Security: do not log secrets; correct witness commitment and header math; target comparisons correct; NTP time discipline.
- Portability: Windows + Linux builds; CUDA toolchain; CI for both OSes.
- Compliance: CGMiner/GPL license compliance.

### Acceptance Criteria
- Multi‑source mandatory: ≥2 pools + local node; all tested per iteration with the same nonce.
- Normalization validated by unit tests: coinbase/merkle, targets, rolling, endianness, midstate.
- Job hot‑swap: in‑place updates; `gen` bump; kernel absorbs within ≤1 batch.
- VRAM cache: ~85% utilization per GPU without breaching `min_free_mib`; watermarks respected.
- PredictabilityWorker: safe double‑buffer; throttles appropriately; no interference with main kernels.
- Kernel: dual‑compare per job; correct routing and CPU verification before submit. Device hit ring feeds `SubmitRouter`.
- Ledger/Outbox: duplicates dropped; pending hits replay after restart.
- Stability: no crashes on disconnects; automatic recovery; no multi‑submit.

### Release Plan (Milestones)
- M1 Bootstrap: Fork CGMiner; toolchain/CI; scaffolding; config loader; logging/metrics skeleton.
- M2 Adapters: `AdapterBase`, `StratumAdapter`, initial pool profiles; `GbtAdapter`; connectivity and reconnection logic.
- M3 Normalizer & Registry: full normalization pipeline; `WorkSourceRegistry` in‑place hot‑swap; unit tests.
- M4 CUDA Engine & Submit: multi‑job kernel; `SubmitRouter`; CPU verification; end‑to‑end regtest shares.
- M5 Cache & PredictabilityWorker: VRAM cache; adaptive controller; double‑buffer precompute; safety and throttling.
- M6 Persistence & Observability: ledger/outbox; metrics/logging; fault injection; docs and examples.

### Risks & Mitigations
- ASIC dominance → Set expectations (education/testing); target regtest/signet; emphasize research value.
- VRAM pressure/OOM → Watermarks, safety margin, mempool cap; optional RAM spill (off by default).
- Pool policy variance → Adapter overrides; schema tests per pool; strict routing and idempotency.
- Concurrency bugs → In‑place updates with `gen` last; minimal pointer churn; ring buffers; robust unit/integration tests.

### Metrics (north‑star & health)
- Hashrate per source; share acceptance/reject/stale; kernel time; kernel occupancy and active blocks/SM; per‑SM flush counters (summaries); kernel attributes (regs/shared/TPB); VRAM residency/util; PCIe bandwidth; predict worker duty cycle; recovery MTTR.

### Open Questions
- Minimum GPU VRAM to officially support? (e.g., 6 GiB+)
- Default scheduler weights and solo bias—conservative or configurable presets?
- Which bloom/cuckoo default parameters for dedup FPR and memory footprint?

### Configuration Overview
- File: `config/pools.json`
  - Each pool entry defines:
    - `profile`: viabtc | f2pool | ckpool | nicehash | gbt
    - `cred_mode`: `wallet_as_user` or `account_worker`
    - `endpoints`: `[ { host, port, use_tls } ]`
    - For GBT: `rpc` (url, use_tls, auth=cookie|userpass, username, password, cookie_path) and `gbt` (poll_ms, rules)
  - Global sections:
    - `cuda`: `hit_ring_capacity`, `desired_threads_per_job`, `nonces_per_thread`
    - `metrics`: `enable_file`, `file_path`, `dump_interval_ms`, `enable_http`, `http_host`, `http_port`
    - `outbox`: `path`, `max_bytes`, `rotate_on_start`, `rotate_interval_sec`
    - `ledger`: `path`, `max_bytes`, `rotate_interval_sec`

### Example Configuration
See `docs/config-example.ini` for a complete example.


