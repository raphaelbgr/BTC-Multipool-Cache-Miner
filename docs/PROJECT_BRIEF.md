## Project Brief

### Project
Multi‑Pool BTC CUDA Miner (CGMiner‑based)

### Purpose
Improve effective mining coverage and first‑block discovery odds by concurrently testing the same nonce across multiple active jobs from Stratum pools and a local node, backed by a mandatory VRAM cache and safe predictability precompute.

### Outcomes
- Working multi‑source GPU miner with cross‑job nonce testing.
- Deterministic normalization, in‑place hot‑swaps, and VRAM cache controller.
- Research‑grade observability and robust fault handling.

### Success Criteria
- Meets PRD acceptance criteria for multi‑source, normalization, caching, predict worker, kernel correctness, persistence, and stability.
- ≤5% throughput regression vs single‑source baseline; quick hot‑swap absorption.

### Deliverables
- Source code (Windows + Linux), configuration, and CUDA kernel.
- Documentation: PRD, Architecture, Testing & Ops, README, example config.
- CI pipeline and license compliance.

### Milestones
- M1–M6 as defined in PRD.

### Risks
- ASIC dominance (scope: research/education), VRAM pressure, pool policy variance, concurrency bugs.


