# MATERIAL-01A — Implementation Plan S1–S4

**Status:** proposed roadmap only. S0 authorizes no production implementation. Independent runtime research listed below must proceed on its own exact scope; no probe branch code or legacy ENMS production code is imported by this plan.

## Stage boundaries

| Stage | Scope | Dependencies | Main risks | Acceptance | Rollback |
|---|---|---|---|---|---|
| **S1 — Authoring contract + offline validator** | Freeze v1 names/encoding after testing authoring round-trip; define Profile schema; extend the initial Shinano NIF corpus with representative vanilla FaceGen/skin and BGEM/transparent samples; independently parse packed BGSM/BGEM payloads and texture semantics; implement offline validator only after separate authorization | S0 review; working NIF/BGSM tools; representative assets and known archive/provider provenance | Extra-data loss/rewrite, non-unique Shape names, exporter incompatibility, parser ambiguity, packed-resource/provider mismatch | Golden NIF round trips across target editor/export paths; deterministic schema validation; rejects duplicate/unknown/bad paths; BGSM/BGEM field and texture-reference checks; no game or shader runtime dependency | Remove validator/catalog from release package; declarations remain inert and native |
| **S2 — Catalog and deterministic static resolver** | Parse validated catalogs off-thread; immutable snapshots; Profile ID/resource resolution; conflict diagnostics; CPU-only tests and offline comparison | S1 schema and validator; agreed provider/digest source | Provider precedence drift, stale catalog, conflicting overrides, resource ownership | Deterministic results independent of enumeration order; bounded limits; snapshot transaction tests; missing/invalid/conflict always native; no draw Hook or GPU ABI changes | Disable catalog load and leave all assets on native renderer |
| **S3 — Runtime static material binding** | Integrate only a proven Shape declaration/profile to supported native material/pass context; bound caches/resources; no dynamic Head/Actor dependency; exact route coverage validated | S2; independent research for Geometry/Property lifetime, replay ownership and Immediate/CommandBuffer routes; shader consumer contract | Raw pointer ABA, unseen pass variants, cache reuse, resource-reload race, accidental change to existing features | Per-route evidence matrix; exact supported descriptors and asset revisions; concurrency/lifetime tests; GPU capture shows intended binding; PBR/SSS/IBL/shadows and unmarked native content regression checks | Feature kill switch and startup-off default; remove runtime binding, preserve catalog as unused data; no ABI migration in place |
| **S4 — First declared NPR material consumer** | Add one narrow model/role consumer (Face SDF only after its UV/resource/Head-space contract is separately proven); style/profile selection; explicit preserved/composed upstream terms | S3 accepted; independent Head Pose synchronization contract; authored validation assets; shader-cache/version plan | Stale pose, duplicate lighting, alpha/coverage errors, visual mismatch and cost | OG/NG/AE separately gated build/runtime; deterministic Face test matrix including head animation, camera orbit, shadowed/unshadowed lights, missing declaration/resource fallback, and unaffected native materials; GPU timing and cache behavior recorded | Default off; opt-in profiles removed/ignored to Native; retain upstream shader path and existing resource ownership |

S3 must not invent a replay hook to satisfy this schedule. If ownership/lifetime evidence is missing, S3 is blocked and S1/S2 can still deliver useful offline content; narrow the first consumer to a safe route only after review.

## Independent runtime research queue

These workstreams are prerequisites only where indicated; they are not material identity contracts and do not imply that a probe is production-ready:

1. Replay Draw Ownership correlation: relate bake-time Geometry/Property/declaration to actual replay draw, including multi-threaded command creation and stale-record invalidation.
2. Head Pose synchronization boundary: identify a documented/proven phase for coherent root/face/head transform snapshots; if absent, SDF consuming head space stays disabled.
3. Actor 3D generation/lifecycle: prove attach/detach, unload/rebuild and safe generation publication; do not persist naked pointers.
4. Instance Handle + generation: test nonwrapping reuse/ABA defense, frame validity and registry teardown; no GPU table until data transport is separately approved.
5. Immediate Lane Bake: observe exact route/variant coverage and preserved lane behavior; it is independent of static authoring and S3 must not change the lane ABI.

## Global compatibility invariants

- Unmarked, malformed, unresolved, incompatible and resource-incomplete materials remain on native Community Shaders behavior.
- Keep PBR, SSS, IBL, realtime shadows, HDR and existing non-stylized materials functioning. Per-model composition must be explicit; no global Toon takeover.
- No changes to PrepassFeatureLane physical/logical contract, RT7, `t70`, or existing feature consumers under this roadmap. Any future ABI proposal requires its own reviewed compatibility plan.
- No production owner parser changes, unreviewed hooks, transparent-to-opaque coverage, GPU instance table or old ENMS production-code migration.
- Runtime support is a per-runtime gate; OG evidence is never substituted for NG/AE.

## S0 handoff decisions requested

Review the proposed NIF extra-data carrier, schema format, candidate roles, provider/digest identity key, immutable snapshot model, and S1 evidence corpus. Do not start S1 automatically. If review changes a core identity or authoring assumption, revise the docs before implementation.
