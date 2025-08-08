## CI Plan (Windows + Linux, CUDA)

### Matrix
- OS: Windows Server 2022, Ubuntu 22.04 LTS
- CUDA: 12.x (build), CPU‑only fast path for unit tests
- Architectures: x86_64

### Pipelines
1) Lint & Unit (CPU‑only)
   - Install deps; build with CUDA stubs or CPU fallback
   - Run unit tests: normalization, header, targets, midstates, rolling clamps

2) Integration (Regtest)
   - Spin up `bitcoind` regtest
   - Mock Stratum servers (or containers)
   - Run adapters, registry, submit router tests

3) Build (CUDA)
   - Install CUDA toolkit
   - Build release binaries (Windows + Linux)
   - Archive artifacts

4) Bench Smoke
   - Short kernel run on CPU‑only shim to validate control flow (or GPU‑enabled runner if available)

5) License Compliance
   - Verify CGMiner/GPL compliance; SPDX headers; third‑party notices

### Caching
- Cache build artifacts and deps between runs

### Secrets
- Use CI secrets for RPC creds; never print to logs

### Artifacts
- Release bundles, example configs, test reports, coverage


