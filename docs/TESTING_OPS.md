## Testing and Ops Notes

### Current State
- Config-driven multi-pool runner operational (no CLI args required) with endpoint rotation and TLS.
- GBT path implemented:
  - Polls `getblocktemplate` with `rules=["segwit"]` (cookie auth by default).
  - Normalizes to `WorkItem` and feeds registry.
  - `GbtSubmitter` caches template txs and submits via `submitblock`.
- All unit tests currently green: 42/42.

### How to Run (Windows)
- Use VS 2022 Developer Command Prompt.
- Build: `cmd /c build_ninja_vs2022.bat`
- Run runner: `build\src\stratum_registry_runner.exe`
  - Optional: `set BMAD_POOL=viabtc|f2pool|ckpool-solo|nicehash|mynode-gbt`
  - Optional: `set BMAD_LOG_FILE=runner.jsonl`

### Local Node RPC (PowerShell)
- Named params, cookie auth auto:
  - `bitcoin-cli --% -named getblocktemplate template_request="{\"rules\":[\"segwit\"]}"`

### Manual GBT Submit (for debugging)
- In runner console when `BMAD_POOL=mynode-gbt`:
  - `submit_header <header80_hex>`
  - Header is 80 bytes, hex-encoded. The runner verifies on CPU and routes to `submitblock` if valid.

### CI Notes
- `BUILD_TOOLS` is OFF by default to avoid building the `stratum_probe` diagnostic tool during tests.
- POSIX plain socket backend added for Linux builds; TLS via OpenSSL when available.

### Next Tests to Add
- Coinbase assembly + witness commitment vector tests.
- Block-hex construction (varint and ordering) unit tests.
- Regtest E2E: `getblocktemplate` → easy header → `submitblock` accept path.

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


