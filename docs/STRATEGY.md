## Strategy: Solo‑Mining Hit with Cached Multipool

### Goal
Maximize coverage and responsiveness for a true solo block find while maintaining continuous hashing across multiple external pools. Do this by caching normalized jobs from many sources and testing the same nonce across all active jobs per CUDA iteration.

### Core Ideas
- **Same‑nonce, cross‑job testing**: Each kernel batch tests a common nonce range against all active jobs (Stratum pools + local GBT). This yields block‑hit detection across all sources without re‑scheduling.
- **VRAM cache as first‑class**: Keep aligned, kernel‑ready constants (midstates, targets, endianness‑normalized headers) resident in VRAM. Target ~85% VRAM utilization with safety watermarks and make hot‑swaps absorb within ≤1 batch.
- **Deterministic normalization**: Adapters convert pool/job semantics into a single `WorkItem` + `GpuJobConst` representation. Trust the normalizer—not pool idiosyncrasies.
- **In‑place hot‑swap**: `WorkSourceRegistry` updates a fixed slot per source; bump `gen` last. Kernel sees stable snapshots; device copies are small and targeted.
- **Submit routing with CPU verify**: `SubmitRouter` double‑hashes the 80‑byte header on CPU and compares to the right target before routing.

### Submission Policy
- **Share hits (< share_target)**: Submit back to the originating pool adapter (normal revenue path). Ledger + Outbox dedupe ensure idempotence.
- **Block hits (< block_target)**: Prefer the configured solo endpoint (local `bitcoind` via GBT) for true solo claim. If policy demands pool claim, route to originating adapter instead.

| Hit type | Compare against | Primary route | Fallback |
| --- | --- | --- | --- |
| Share | `share_target` | Pool adapter | Requeue via Outbox |
| Block | `block_target` | Solo (GBT/local node) | Pool adapter (configurable) |

### Why Cached Multipool Helps
- Reduces end‑to‑end time to detect and route a block hit by eliminating per‑source rescheduling and CPU normalization jitter.
- Absorbs frequent pool template changes (e.g., `clean_jobs`, varDiff tweaks) without thrashing the device; only dirty slots are copied.
- Keeps multiple opportunities live (pools + solo), increasing the chance that a randomly explored nonce produces a block‑valid header for at least one active template.

### Adapter Semantics (Stratum today)
- Track varDiff (`share_target`) independently of `block_target (nbits)`.
- Respect pool policies: version/ntime rolling masks, `clean_jobs` invalidation.
- Map Stratum notify → `RawJob` → `normalize::RawJobInputs` → `WorkItem`/`GpuJobConst`.

### Safety & Observability
- Strict CPU verification before submit; no blind routing.
- `Ledger` and `Outbox` provide crash‑safe dedupe and replay upon restart.
- Structured JSON logs and metrics for per‑hit traceability and pool health.

### Configuration Notes
- Define solo endpoint (GBT) and pool endpoints concurrently.
- Tie submission preference for block hits to a config flag (solo‑first vs pool).
- Ensure `min_free_mib` is respected to keep the driver stable under pressure.



