## Multi‑Pool BTC CUDA Miner (CGMiner‑based)

Research/education‑grade GPU miner that tests the same nonce across all active jobs per CUDA iteration, spanning multiple Stratum pools and a local node (GBT). Emphasizes deterministic normalization, in‑place hot‑swaps, mandatory per‑GPU VRAM cache (~85% utilization), and a safe predictability precompute worker.

### Status
Design and documentation scaffold. See PRD and Architecture docs.

### Key Features
- Multi‑source: ≥2 pools + local node concurrently
- Normalizer to `WorkItem` + `GpuJobConst` with midstate precompute
- Cross‑job per‑nonce CUDA kernel; micro‑batches
- VRAM CacheManager with adaptive watermarks
- PredictabilityWorker (low‑priority stream, double‑buffer)
- SubmitRouter, persistent Ledger, crash‑safe Outbox
- Observability: metrics and structured logs

### Docs
- `docs/PRD.md` — Product Requirements
- `docs/ARCHITECTURE.md` — Architecture
- `docs/PROJECT_BRIEF.md` — Project Brief
- `docs/TESTING_OPS.md` — Testing & Operations
- `docs/config-example.ini` — Example configuration

### Quick Start (regtest/signet)
1) Ensure CUDA toolkit and a compatible GPU.
2) Prepare config based on `docs/config-example.ini`.
3) Run against regtest/signet pools and/or a local `bitcoind` with GBT enabled (`-rpcallowip`, `-rpcuser`, `-rpcpassword`, `-deprecatedrpc=submitblock` when applicable) and `-txindex` for convenience.

### Licensing
Planned: CGMiner/GPL compliance. Ensure all derivatives respect GPL requirements.

### Risks & Disclaimer
GPU SHA‑256d is noncompetitive vs ASICs; this project targets research/education and test networks. Respect pool ToS; avoid multi‑submit and any abusive behavior.


