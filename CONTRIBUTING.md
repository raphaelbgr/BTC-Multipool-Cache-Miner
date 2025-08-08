## Contributing

Thanks for your interest! This project targets research/education for BTC GPU mining strategies.

### Build Prerequisites
- CUDA toolkit 12.x (for GPU build); CPU‑only path for unit tests
- Windows or Linux; C/C++ toolchain; Python (for helpers/tests)

### Development Workflow
1) Fork and create a feature branch.
2) Add/modify unit and integration tests alongside code.
3) Ensure docs updated: PRD/Architecture/Testing‑Ops if behavior changes.
4) Run CI locally if possible; ensure Windows + Linux compatibility.
5) Open a PR with a clear description and checklist against `docs/DoD.md`.

### Code Style
- Clear, readable code; explicit types on public APIs
- Guard clauses; error handling first; avoid deep nesting
- No secrets in logs; structured JSON logging

### Licensing
- Maintain CGMiner/GPL compliance; include SPDX headers where applicable

### Security
- Do not add third‑party dependencies without review
- Never log RPC passwords or private data


