## Architecture Specification

### Principles
- SOLID, DI‑friendly modules with single responsibility.
- In‑place hot‑swaps; avoid pointer churn; generation counters for visibility.
- Deterministic normalization prior to CUDA; kernel consumes aligned constants.

### Component Overview
- CacheManager (VRAM): dynamic per‑GPU cache targeting ~85% VRAM; maintains midstates, normalized templates/variants, per‑job dedup filters, and header‑variant staging. Watermarks and `min_free_mib` guard rails.
- PredictabilityWorker (CUDA): low‑priority stream precomputing/warming cache pages with double‑buffer shadow pages and epoch swap on stable `gen`. Adaptive throttling based on VRAM and GPU util.
- Adapters: `AdapterBase`, `StratumAdapter`, concrete `PoolAdapters/*`, and `GbtAdapter` + `GbtRunner`. Convert external schemas to `RawJob` with policy metadata (rolling caps, extranonce constraints, submit schema). GBT path polls `getblocktemplate` via JSON‑RPC (Bitcoin Core), using cookie auth by default. `GbtSubmitter` assembles block hex (header + varint(tx_count) + txs) and calls `submitblock`.
- Normalizer: converts `RawJob` → `WorkItem` + `GpuJobConst`; computes share and block targets; endian normalization; coinbase assembly with extranonces; midstate precompute; Merkle root. For GBT, miner uses `merkleroot` for header while witness commitment is handled inside coinbase.
- WorkSourceRegistry: fixed‑size arrays for N sources tracking `WorkItem` and `GpuJobConst`. In‑place writes; `gen` bump last; `active` flags; `found_submitted` sticky until superseded.
- CudaEngine: per‑nonce, cross‑job kernel. Grid y‑dim = job index; x‑dim = nonce offsets. Short micro‑batches (~0.5–3 ms). Emits hits into a device‑side ring buffer per device, drained by host into a lock‑protected host ring. Per‑block shared‑memory hit buffers coalesce writes to reduce global atomic contention. Device SHA‑256d implemented (midstate path), with `DeviceJob` table uploaded each hot‑swap; constant‑memory fast path for small job sets; specialized unrolled kernels for micro‑batches (2/4/8).
- Scheduler: fair, weighted policy. Backpressure against sources with rejects/latency (decaying penalty). Tunes micro‑batch duration for responsiveness to hot‑swaps (simple auto‑tune). Occupancy‑aware tuning adjusts threads per job.
- SubmitRouter: CPU verifies double SHA‑256 on headers before routing shares/blocks to their originating adapter. Idempotent per job.
- Ledger & Outbox: O(1) in‑RAM job map with periodic JSONL snapshots and rotation policy (size/time); crash‑safe append‑only outbox that replays pending hits on startup; size‑based rotation, optional time‑based rotation, and optional rotate‑on‑start; acceptance cleanup wired from runner to prune persisted entries.

### Data Models (Kernel‑ready)

`WorkItem`
```
{ source_id, job_id, version, prevhash[32 LE], merkle_root[32 LE], ntime, nbits,
  share_target_le[8], block_target_le[8], vmask, ntime_min, ntime_max,
  extranonce2_size, clean_jobs, gen, active, found_submitted }
```

`GpuJobConst`
```
Aligned constants and precomputed midstates required for kernel header assembly.
```

### Normalization Pipeline
1) Parse adapter `RawJob` → validate input bounds and policies.
2) Coinbase assembly (address, extranonce sizes/packing) and Merkle root with segwit commitment.
3) Endianness normalization (LE for `prevhash`, `merkle_root`).
4) Targets: compute `share_target` (varDiff) and `block_target` (from `nbits`) as LE `u32[8]`.
5) Precompute midstates; produce compact `GpuJobConst`.
6) Write into `WorkSourceRegistry` slot; bump `gen` last; mark `active=1`.
7) Enqueue async copy of the updated slot to device; update device job table for the kernel.

### CUDA Execution Model
- Threads compute `nonce = base + offset` and loop over job indices (y‑dim). For each indexed job j:
  - Assemble 80‑byte header (constants + nonce)
  - Compute SHA‑256d
  - Compare `h` with `share_target[j]` and `block_target[j]`
- On match, record hit `{job_index, nonce, type}` into a device ring with atomic write index; host drains between launches.
- Job constants reside in `__constant__` or read‑only memory; job loop strives to minimize branch divergence; SHA rounds unrolled for throughput.

### VRAM Cache Strategy
- Target utilization: ~85% of available VRAM; never drop below `min_free_mib` free memory.
- Watermarks (`low`, `high`) trigger background promote/evict in `page_mib` chunks.
- Sharded per device; optional RAM spill is off by default; RAM holds authoritative indices only.

### PredictabilityWorker Safety
- Uses a low‑priority CUDA stream to precompute shadow pages.
- Double‑buffer per job; atomically swaps at kernel batch boundaries when `gen` unchanged.
- Pauses on high GPU utilization or near `high` watermark; CPU fallback for partial tasks.

### Scheduler & Backpressure
- Weighted fairness across sources; optional bias for local node.
- Backpressure: reduce time slices for laggy/rejecting sources; dynamic micro‑batch sizing.
- Current: runner iterates fairly over active jobs with a common nonce base; scheduler module provides per‑source weight replication (unit‑tested). Backpressure based on rejects/latency to be wired next.

### Submission & Persistence
- CPU verifies reconstructed header matches GPU result prior to submission (also used by `SubmitRouter`).
- Routes to correct adapter by slot index.
- Ledger stores job key and timestamps; outbox replays pending hits post‑restart; dedup guarantees idempotency.

### Observability
- Metrics: hashrate per source, accept/reject/stale, kernel timing, occupancy (active blocks/SM, %), per‑SM flush summaries, kernel attributes (regs/shared/TPB), PCIe BW, VRAM usage, predict worker duty cycle.
- Logs: structured JSON; per‑hit traces; connection state; retry/backoff reasons.

### Testing Strategy
- Unit tests: header build, endianness, midstate computation, target math, rolling clamps, extranonce packing.
- Integration: Stratum and GBT adapters on regtest/signet; network fault injection; clean_jobs storms.
- Benchmarks: sweep `workitems_per_launch` and `batch_nonces`; auto‑tune defaults; ensure ≤5% regression vs single‑source baseline.

### Security & Correctness
- BIP22/23 GBT with `rules=["segwit"]`; BIP141 witness commitment in coinbase.
- Header math and SHA‑256d verified against test vectors.
- Time discipline via NTP; clamp `ntime` within bounds; monotonic scheduling.
- Secrets redaction in logs; never persist credentials in plaintext logs.

### Milestones & Ownership
- Mirrors PRD milestones (M1–M6). Each milestone includes code, tests, and doc updates.

### Implemented So Far (Current Status)
- Config-driven multi-pool source loader (`config/pools.json`) with per-pool profiles (ViaBTC, F2Pool, CKPool, NiceHash) and a local-node GBT entry (cookie auth).
- Endpoint rotation: advances to next endpoint after repeated connect failures or quick disconnects; per-connection backoff with structured logs.
- Stratum V1 runner: connect → subscribe → authorize → notify handling; varDiff via `mining.set_difficulty`, `mining.configure` version‑rolling mask negotiation; `clean_jobs` honored; submit_result logging.
- TLS support via OpenSSL backend; plaintext sockets on Windows and POSIX. Auto-enable TLS on ports like 443/3334 when configured.
- Structured JSON logging with optional file sink (`BMAD_LOG_FILE`).
- Normalizer path and registry integration are in place; CUDA engine has multi‑job launch stub and a demo device path that writes one hit per job for host drain tests.
- GBT: node connectivity validated (cookie auth + getblocktemplate) and config schema defined (supports `cookie_path` override). Template transactions cached and block submission via `submitblock` implemented. A helper builds full block hex (header + varint(tx_count) + coinbase + txs) pre-submit for diagnostics. If only `default_witness_commitment` is present, a minimal coinbase carrying an OP_RETURN witness commitment is synthesized for diagnostics.

### Config & Run
- File: `config/pools.json`
  - Each pool entry defines:
    - `profile`: viabtc | f2pool | ckpool | nicehash | gbt
    - `cred_mode`: `wallet_as_user` (wallet.worker) or `account_worker` (account.worker)
    - `endpoints`: `[ { host, port, use_tls } ]` (rotation order)
    - For GBT: `rpc` (url, use_tls, auth=cookie|userpass, username, password, cookie_path) and `gbt` (poll_ms, rules)
- Environment (optional):
  - `BMAD_POOL`: select pool by name; default is first in config
  - `BMAD_LOG_FILE`: write JSONL logs to file
  - `BMAD_AUTO_SUBMIT`: `en2:ntime:nonce` (hex) test submit after first job
- Run (no CLI args required):
  - `build\\src\\stratum_registry_runner.exe`
  - Example pool-specific (optional): `set BMAD_POOL=viabtc`

### Next Up
- Implement `GbtRunner`: poll `getblocktemplate` (cookie), assemble coinbase to configured wallet, compute witness commitment and Merkle, feed jobs to registry, and submit solutions via `submitblock`.
- Linux validation for OpenSSL backend and config-driven runner.
- Stratum V2 adapter (Braiins) or V2→V1 proxy integration.
- Submit pipeline persistence (Ledger/Outbox replay) and CUDA engine auto-submit hook.


