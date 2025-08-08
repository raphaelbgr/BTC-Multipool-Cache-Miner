## Multi‑Pool BTC CUDA Miner (CGMiner‑based)

Research/education‑grade GPU miner that tests the same nonce across all active jobs per CUDA iteration, spanning multiple Stratum pools and a local node (GBT). Emphasizes deterministic normalization, in‑place hot‑swaps, mandatory per‑GPU VRAM cache (~85% utilization), and a safe predictability precompute worker.

### Status
Initial bootstrap complete with unit-tested core utilities.

- Build: CMake + GoogleTest; optional CUDA (disabled by default)
- Core utils: structured JSON logging, metrics, endianness helpers
- Normalization: compact nBits→LE u32[8], SHA-256 midstate, Merkle root
- Registry: fixed-slot `WorkSourceRegistry` with in-place `gen` bump
- Submit: CPU sha256/sha256d and `SubmitRouter` with verify-then-route
- Tests: 15/15 passing on Windows (VS 2022)

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

### Build & Test
```powershell
mkdir build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DENABLE_CUDA=OFF
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### Quick Start (regtest/signet)
1) CUDA optional for now; CPU path compiles without CUDA.
2) Prepare config based on `docs/config-example.ini`.
3) Use local pools or `bitcoind` GBT for integration when adapters are added.

### Licensing
Planned: CGMiner/GPL compliance. Ensure all derivatives respect GPL requirements.

### Risks & Disclaimer
GPU SHA‑256d is noncompetitive vs ASICs; this project targets research/education and test networks. Respect pool ToS; avoid multi‑submit and any abusive behavior.


