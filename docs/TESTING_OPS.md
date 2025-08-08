## Testing & Operations Guide

### Environments
- Regtest: fast feedback for end‑to‑end; inject clean_jobs, disconnects, and latency.
- Signet/Testnet: realistic network behavior without mainnet costs.
- Mainnet: optional; treat as research/education.

### Tests
- Unit
  - Header build (80B): version/prevhash/merkle_root/ntime/nbits/nonce composition; endian tests.
  - SHA‑256d vectors; target comparison (LE u32[8]).
  - Normalizer: coinbase assembly with segwit witness commitment; extranonce packing; rolling clamps.
  - Midstate precompute correctness.
- Integration
  - Stratum adapters: varDiff handling, rolling/version/ntime caps, clean_jobs, submit schema.
  - GBT adapter: BIP22/23 with `rules=["segwit"]`; submitblock success; tip mismatch invalidation.
  - WorkSourceRegistry hot‑swap behavior; device updates ≤1 batch.
  - SubmitRouter idempotency and CPU verification before submit.
- Fault Injection
  - Disconnect storms, stale jobs, adapter restarts.
  - VRAM near‑OOM and watermark thrash; verify cache controller behavior.
- Benchmarks
  - Sweep `workitems_per_launch` and `batch_nonces` per GPU; record kernel time, occupancy, PCIe BW, hit ring throughput.
  - Verify ≤5% regression vs single‑source baseline.

### CI/CD
- Matrix: Windows + Linux; CUDA toolchain versions; optional CPU‑only fast tests.
- Jobs: build, unit tests, integration (regtest), benchmark smoke, license compliance (CGMiner/GPL).

### Operations
- Configuration via INI; see `docs/config-example.ini`.
- Metrics export (Prometheus/JSON): per‑source hashrate, accept/reject/stale, kernel time, occupancy, PCIe BW, VRAM usage, predict worker activity.
- Logs: structured JSON; include `{jobKey, source, nonce, type}` traces; redact secrets.
- Persistence: ensure writable `store/` for ledger/outbox; monitor replay on process start.

### Runbooks
- Adapter flapping
  - Check network reachability; inspect logs for backoff reasons; verify credentials.
  - Reduce source weight or disable temporarily; confirm scheduler backpressure engaged.
- High VRAM pressure
  - Verify `min_free_mib`, `watermark_high`, and `page_mib` settings; consider enabling RAM spill (if supported) or reduce precompute horizon.
- Stale spikes
  - Inspect time sync (NTP); ensure `ntime` within bounds; check pool latency; consider smaller micro‑batches.


