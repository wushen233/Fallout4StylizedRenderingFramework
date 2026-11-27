# FACE-ELIGIBILITY-01 — Next Implementation Proposal

Baseline reviewed: production source `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`. This is a research proposal only; it does not authorize implementation.

## Decision

**INFERRED:** Community Shaders already exposes enough data to define a conservative *variant eligibility candidate* without adding an engine Hook: the Prepass classifier receives `BSRenderPass` at Immediate SetupGeometry or CommandBuffer Bake and can inspect the pass descriptor and Geometry. Existing code demonstrates a fail-closed Actor lookup for its own SSS classification.

**BLOCKER:** Existing evidence is insufficient to perform Face NPR. The lane is fully owned, and accepted runtime evidence is CommandBuffer Bake only. There is no proof that a profile/instance payload is attached to the actual replayed GPU draw, that the GPU consumed it, or that resource/UV compatibility and dynamic data lifetime are valid. `FACE` alone is insufficient.

Do not revive `NativeBipedAttachmentProbe` or infer its Hook installation safety from the fact that existing renderer callbacks exist. No new Hook is recommended for the first slice.

## Minimal next validation slice (proposal)

### Slice 1 — Read-only runtime variant correlation

Use existing accepted capture/log evidence and a future targeted RenderDoc capture to correlate, for a single opaque player Face sample and one vanilla Face sample:

- exact Prepass event and actual Draw/DrawIndexed;
- Pixel Shader identity/defines or reflected variant;
- descriptor Face/SkinTint/Blend/AlphaTest state;
- actual material/texture and UV inputs visible to that draw;
- whether draw is Immediate or replayed CommandBuffer;
- lane constant bytes and consumer execution for that draw.

Keep Bake event, Replay Draw event, and GPU pixel use as separate records. This is observation, not a new Hook. If the existing capture cannot reveal a field, report it UNKNOWN rather than adding a new callback in this phase.

### Slice 2 — Opt-in and immutable material lookup proof

Before shader implementation, demonstrate one selected asset can be explicitly opted in through a validated declaration or Compatibility Profile, and that runtime resolution binds it to the exact live geometry/material use without filename inference. Shape-scoped carrier validation is still outstanding; if it remains unavailable, use only a separately approved, validated Compatibility Profile path. Do not fall back to Type 4, segment labels, or texture paths as identity.

### Slice 3 — Minimal production vertical slice (future approval required)

Only after Slices 1-2 and transport/lifetime review: implement one opaque Face-only opt-in profile on an isolated shader variant/path, preserving stock rendering when disabled or unresolved. Do not modify existing lane component meanings. Define a separate reviewed transport if instance data is required; defer SDF itself until texture/UV contract, head-pose source, and replay association are established. Keep SkinTint, eyes, hair, transparent/blended parts, mixed Geometry, and all unknown cases on native Community Shaders behavior.

## Dependencies and gates

| Gate | Existing evidence | Required closure |
|---|---|---|
| Variant candidate | Source descriptor maps Face to `FACE`; accepted Bake descriptors exist | Capture exact target draws; check Face/SkinTint combinations |
| Actor ownership | SSS source uses `FindReferenceFor3D(geometry)` and fails closed for missing actor/race | Validate at actual Face draw/replay, including CommandBuffer cases |
| Profile opt-in | Architecture keeps explicit declaration and External Compatibility Profile | One exact, validated asset-to-profile association; no guessed identity |
| Material/UV/resources | Static NIF and BGSM evidence accepted, but no full runtime bridge | Confirm resolved shader/material, resource content/channel, UV set and transform |
| Alpha/pass | Descriptor has Blend/AlphaTest bits; HLSL has separate branches | Select and verify a supported opaque pass; establish native queue/blend behavior |
| Instance/GPU transport | Lane bake evidence only; lane already owned by four features | Separate transport proposal with generation, replay association and GPU retirement rules |
| Head transform | ARCH-01C/01D still lacks a synchronized production pose source | Close independently before any view-dependent/head-relative SDF data |
| Runtime support | OG evidence is limited | Validate each advertised runtime independently; no version gate added here |

## Acceptance for the next research review

1. The two positive assets correlate to actual runtime Face variants; negative controls do not qualify merely by actor ownership or asset names.
2. `FACE` and `SKIN_TINT` combination behavior is documented from actual variants; any unknown combination stays native.
3. Bake-to-replay identity and per-draw data consumption are either proven or explicitly left as a transport blocker.
4. An exact opt-in/profile association and all required texture/UV contracts are validated.
5. Any later code proposal names the existing supported seam and its installation/lifecycle risks. If it modifies Hook installation, that is a separate safety gate.
6. Existing PBR, SSS, IBL, shadows, HDR, and all non-opted-in assets remain on their current Community Shaders path.

No production implementation, tests, build, deployment, game start, push, or merge is authorized by this proposal.
