## Roadmap

### M1 — Bootstrap & Tooling
- Fork CGMiner baseline; license review
- Repo layout, config loader, logging/metrics scaffold
- CI matrix (Win/Linux), unit test harness

### M2 — Adapters
- `AdapterBase`, `StratumAdapter`, initial pool profiles
- `GbtAdapter` with BIP22/23 and segwit rules; `GbtRunner` polling via JSON-RPC; `GbtSubmitter` calling `submitblock`
- Robust reconnect, clean_jobs handling

### M3 — Normalizer & Registry
- Coinbase/witness commitment, merkle, endianness, targets
- Midstate precompute; `WorkItem` + `GpuJobConst`
- `WorkSourceRegistry` in‑place hot‑swap; async device updates

### M4 — CUDA Engine & Submit
- Cross‑job per‑nonce kernel; ring buffer for hits
- CPU verification; `SubmitRouter` to adapters
- Regtest end‑to‑end share submission (GBT path first)

### M5 — Cache & PredictabilityWorker
- VRAM CacheManager with watermarks and safety margin
- PredictabilityWorker (double‑buffer, low‑priority stream)
- Adaptive throttling and CPU fallback

### M6 — Persistence, Observability, Hardening
- Ledger (mmap snapshot), Outbox (crash‑safe replay)
- Metrics and structured logs
- Fault injection, benchmarks, tuning; docs finalization


