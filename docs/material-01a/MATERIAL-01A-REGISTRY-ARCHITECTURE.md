# MATERIAL-01A — Registry Architecture

**Status:** proposal only. No runtime registry or GPU table is implemented.

## Separate catalogs and handles

Use four independently owned data sets:

1. **Asset catalog** — canonical model resource key, digest, declaration UUIDs, and offline validation results.
2. **Material catalog** — stable logical Profile IDs and immutable, validated profile data/resource handles.
3. **Runtime material binding** — authored declaration resolved to a Material Handle for the current loaded Geometry/Property generation, only after safe lifetime rules are proven.
4. **Actor/instance registry** — explicitly separate from material data and deferred to independent runtime research. No Actor FormID or pose is stored in a Profile.

**PROPOSED handle:** `{index: u32, generation: u32, snapshotGeneration: u64}`. Index zero is invalid. A generation increments before slot reuse; exhaustion retires the slot rather than wrapping. Snapshot generation prevents a handle resolved against stale configuration from aliasing a newly loaded profile. Use a larger opaque handle if the supported shader transport cannot represent this safely; do not truncate it into PrepassFeatureLane.

## Operations and ownership

- `ValidateCatalog`: offline/file-loading phase, bounded inputs, deterministic diagnostics; owns parsed values and verified resource references.
- `PublishSnapshot`: build a complete immutable snapshot off-thread, validate every reference/conflict, then publish as one transaction at an established safe configuration boundary.
- `ResolveDeclaration`: deterministic lookup over the immutable snapshot; no disk access, directory traversal or dynamic allocation on render hot paths.
- `BindRuntimeObject`: only at a proven engine-safe callback with an explicit Geometry lifetime/owner contract. If unavailable, do not bind; report Native fallback.
- `InvalidateResource`: build/revalidate a replacement snapshot and atomically publish it. Do not mutate a snapshot while readers hold it.
- `RetireRuntimeBinding`: requires a verified unload/rebuild lifecycle event or a safely validated generation check. Until this exists, use a bounded frame-local/synchronous resolution that retains no object pointers.

## Concurrency

**PROPOSED:** immutable snapshots with reference-counted ownership, read-only lookup structures, and atomic snapshot publication. Render readers acquire a strong snapshot token, resolve stable value IDs, and release at the callback boundary. Resource objects are owned by that snapshot or by an explicit asset lifetime manager. No reader races with in-place profile edits. Config changes are transactions: parse → validate → resolve resources → check collisions → publish; any failure preserves the previous valid snapshot.

This solves catalog concurrency, not engine object synchronization. It does not make a `BSGeometry*`, `TESObjectREFR*`, `NiNode*`, texture pointer or CommandBuffer pointer safe across threads or frames. Those require their own lifecycle proof.

## Determinism, diagnostics and bounded behavior

Use sorted catalog IDs, deterministic path normalization, capped resource count/size/depth, bounded lookup work and fixed-size or deferred diagnostic events. Detect duplicate profile IDs, declaration IDs, asset keys and selector collisions before publication. For errors, record stable diagnostic codes plus asset/Profile IDs and cause; avoid logging raw object pointers as identity. Do not walk the scene graph, touch the filesystem or allocate unbounded memory in shader injection or draw callbacks.

## Resource reload and shutdown

On reload, create a new snapshot generation; do not mutate profile records in place. Existing frame readers finish on the old snapshot; new readers see the new snapshot. Retire old resources only after snapshot references drain and any GPU fence/resource lifetime has been satisfied. **UNKNOWN:** exact Community Shaders resource reload and GPU retirement boundary for a future material catalog; must be designed against the chosen resource-binding path before S3.

## Actor Instance Registry is a separate future system

Actor identity cannot be a bare pointer or material Profile. A future Actor Instance Handle needs an authoritative actor identity, 3D generation, unload/rebuild invalidation, generation-safe reuse, and a documented pose publication phase. CommandBuffer creation records must not be treated as live Replay ownership. If these contracts remain unavailable, the material resolver stays static and declines features needing dynamic per-Actor values. No production GPU Structured Buffer is justified by S0 evidence.
