## Testing & Operations Guide

### Environments
- Regtest: fast feedback for end‑to‑end; inject clean_jobs, disconnects, and latency.
- Signet/Testnet: realistic network behavior without mainnet costs.
- Mainnet: optional; treat as research/education.

### Tests
- Unit
  - Endianness helpers (swap, array round-trip)
  - Compact target to LE `u32[8]` (genesis vector, zero mantissa)
  - Registry `gen` bump semantics
  - Logging JSON shape smoke test
  - Metrics counters/gauges snapshot
  - CPU `sha256`/`sha256d` vector and hash≤target compare
  - Midstate (64-byte) finishing equivalence to full SHA-256 first round
  - Merkle root computation (empty, two-leaf duplication rule)
  - SubmitRouter verify-then-dispatch
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


