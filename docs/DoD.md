## Definition of Done (DoD)

Use this checklist before merging or releasing.

### Functional
- [ ] Multi‑source active: ≥2 Stratum pools + 1 GBT local node; all tested per iteration with same nonce.
 - [x] Normalizer: coinbase (with witness commitment), merkle, endianness, targets (share + block), midstate precompute; unit‑tested. (partial: coinbase/witness pending)
 - [x] WorkSourceRegistry: in‑place updates; `gen` bump last; `active` flags; device slot updates async; absorbs within ≤1 batch.
 - [ ] CUDA kernel: cross‑job per‑nonce; dual compares per job; ring buffer for hits. (stub kernel present)
 - [x] SubmitRouter: CPU re‑verifies header and SHA‑256d; correct adapter routing; idempotent.
- [ ] VRAM CacheManager: ~85% target, respects `min_free_mib`; low/high watermarks; page‑wise promote/evict.
- [ ] PredictabilityWorker: low‑priority stream; double‑buffer; epoch‑safe swap; throttles on util/watermarks.
- [ ] Ledger/Outbox: duplicates dropped; pending hits replay on restart.

### Non‑Functional
- [ ] Stability: recovers from disconnects/clean_jobs; no crashes; no multi‑submit of identical work.
- [ ] Performance: ≤5% regression vs single‑source baseline; micro‑batch ≤3 ms typical; hot‑swap ≤1 batch.
- [ ] Security: secrets never logged; correct header math and targets; NTP/time clamps.
 - [x] Observability: metrics exposed; structured JSON logs; trace per hit.

### Testing & CI
 - [x] Unit tests pass: header build, endianness, midstates, targets, rolling clamps, extranonce packing.
- [ ] Integration: regtest Stratum + GBT; submit success; invalidation on tip mismatch; clean_jobs handling.
- [ ] Fault injection: disconnects, clean_jobs storms; VRAM pressure; expected behavior.
- [ ] Bench: autotune sweep results recorded; within performance envelope.
- [ ] CI green on Windows & Linux; CUDA toolchain validated; license checks.

### Docs
 - [x] README updated; config example provided.
- [ ] PRD/Architecture/Testing‑Ops current with implementation.
- [ ] Roadmap and CHANGELOG updated for milestone.


