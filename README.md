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

### Build & Test (CUDA required)
This project has no CPU fallback; NVIDIA CUDA Toolkit is required.

If CUDA isn’t auto-detected, set it in `config/build.ini`:
```
cuda_root = C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v12.9
```
Linux example:
```
cuda_root = /usr/local/cuda
```

#### Windows (recommended: VS 2022 + Ninja + CUDA)
To avoid MSVC toolset mismatches in CLI builds, use the provided script which pins the VS 2022 environment for Ninja:

```
cmd /c build_ninja_vs2022.bat
```

What the script does:
- Activates the Visual Studio 2022 x64 Developer Command Prompt
- Sets `CC=CXX=cl`
- Configures CMake with `-G Ninja`, CUDA ON, and your `nvcc.exe`
- Builds and runs tests (ctest)

Manual alternative (from the VS 2022 x64 Developer Command Prompt):
```
cmake -S . -B build -G Ninja -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CUDA_COMPILER="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9/bin/nvcc.exe"
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

#### Linux
```
cmake -S . -B build -G Ninja -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 8
ctest --test-dir build --output-on-failure
```

### Quick Start (regtest/signet)
1) CUDA is required; ensure Toolkit is installed.
2) Prepare config based on `docs/config-example.ini`.
3) Use local pools or `bitcoind` GBT for integration when adapters are added.

### Licensing
Planned: CGMiner/GPL compliance. Ensure all derivatives respect GPL requirements.

### Risks & Disclaimer
GPU SHA‑256d is noncompetitive vs ASICs; this project targets research/education and test networks. Respect pool ToS; avoid multi‑submit and any abusive behavior.


