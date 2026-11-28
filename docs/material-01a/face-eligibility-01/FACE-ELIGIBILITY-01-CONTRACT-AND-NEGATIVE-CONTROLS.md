# FACE-ELIGIBILITY-01 — Contract and Negative Controls

Baseline: `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`. Status labels: VERIFIED, INFERRED, PROPOSED, UNKNOWN, BLOCKED.

## Eligibility is a separate domain

**PROPOSED:** `Face Rendering Eligibility` is a per-rendering-use decision, not a persistent material identity. It does not replace or imply Material Role, Native HeadPart Identity, Geometry Ownership, Actor Instance, Material Profile, Style Profile, or Shading Model.

A Face NPR operation may run only if every independent gate below is valid for the same supported draw/use:

1. **Native variant gate:** the effective Deferred Prepass descriptor has `FACE`; `SKIN_TINT` is absent. A combined or unknown feature combination fails closed.
2. **Pass/material compatibility gate:** the actual supported pass and resolved shader/material behavior are known. Blend, alpha-test, transparency, and queue handling must be explicitly supported by the feature; a serialized Alpha Property alone is insufficient.
3. **Ownership gate:** Actor ownership is resolved for that Geometry at the relevant draw/bake and is valid for the same replayed use. Unknown ownership remains native.
4. **Opt-in gate:** a valid explicit NIF declaration or validated External Compatibility Profile opts this material/asset into the selected Style/Profile. Native Face feature alone is not NPR opt-in.
5. **Resource gate:** the selected Profile's required textures/resources exist and their channel meaning, color space, UV set/transform, and sampling convention are validated for the target material. No filename-only inference.
6. **Instance/transport gate:** the required profile/material handle and generation are valid for the target GPU use; lifetime and update ordering are proven. If GPU data or replay association is unknown, remain native.
7. **Feature gate:** the chosen Shading Model and requested feature are supported for this runtime and render path.

Only `all gates = valid` may produce eligible. Any UNKNOWN, conflict, incompatibility, unsupported pass, missing resource, missing opt-in, or invalid generation yields Native Fallback. Do not silently downgrade an invalid explicit declaration to heuristic classification.

## Feature and evidence are not interchangeable

- A native `FACE` descriptor bit is **render-variant evidence**, not an exact HDPT identity, author-intended Material Role, or NPR opt-in.
- `SKIN_TINT` is not a synonym for Face. The SSS classifier intentionally accepts either flag plus a base-race check (`SkinProfile.cpp:49-65`); that establishes an SSS-oriented skin candidate only.
- `BSRenderPass`/Geometry Actor ownership does not establish which Shape/segment role or profile is intended.
- Serialized NIF Shader Type and Alpha Property are input evidence only. They do not prove the resolved runtime property/material, descriptor, queue, or actual draw.
- The G-buffer `auxA.w` value `0.019608` is shared by non-Blend Face and SkinTint variants in `DFPrepass.hlsl:1646-1653`; do not use it as a Face discriminator.
- The accepted ARCH-01D lane values prove CPU Bake writes for sampled records, not replay ownership or shader consumption.

## Asset controls (serialized NIF evidence only)

Source: accepted `NATIVE-03A-SEGMENT-ASSET-MATRIX.md` in the NATIVE-03A R1 worktree. The values below are not runtime eligibility results.

| Control | Serialized evidence | Use in this study | Runtime Face eligibility |
|---|---|---|---|
| Shinano `head` | `BSLightingShaderProperty`, raw Shader Type 4, no Alpha Property; multi-segment labels and nonzero ranges | Positive candidate for later runtime correlation | UNKNOWN; do not promote Type 4 to a contract |
| Vanilla `MaleHeadHuman` | `BSLightingShaderProperty`, raw Type 4, no Alpha Property; segment data includes face-related candidate labels | Independent positive candidate | UNKNOWN; static record/path does not prove live draw |
| Shinano `head_rear` | `BSLightingShaderProperty`, raw Type 5, no Alpha Property, nonzero segment ranges | Negative/ambiguous rear geometry control | UNKNOWN; type difference is suggestive, not a runtime rule |
| Vanilla `MaleHeadHumanRearTEMP` | raw Type 5; nonzero ranges | Negative/ambiguous rear control | UNKNOWN |
| Vanilla `BaseFemaleBody:0` | raw Type 5; seven segments and eighteen subsegments, many labels | Mixed Geometry/body control | UNKNOWN; segment labels cannot assign whole-shape Face role |
| Shinano `Eye_` | raw Type 16; NiAlphaProperty; multiple ranges | Eye control | UNKNOWN |
| Shinano `EyeWhite`, `HighLight` | raw Type 2; one serialized range is zero | Eye-part controls; zero range is not proof absent at runtime | UNKNOWN |
| Shinano `EyeBrow`, `EyeLash`, `Hair_base` | raw Type 6; Hair sample lacks Segment Data | Hair/auxiliary controls | UNKNOWN |
| Vanilla `MouthShadowMale` | `BSEffectShaderProperty`, NiAlphaProperty | Effect/auxiliary negative control | UNKNOWN |
| Vanilla FaceGen eyes/AO/lashes and `MaleNeckGore` | mixed shader property types, Alpha Property presence and segment labels vary | Confounder controls; do not infer Face from head ownership or labels | UNKNOWN |

**VERIFIED (asset report):** selected face/hair/eye files can share segment component labels, Shapes can have multiple segments, and serialized ranges include zero and nonzero entries. **INFERRED:** these facts are sufficient to reject any rule that maps a segment label or actor ownership directly to a single whole-Geometry Material Role. **UNKNOWN:** exact segment submission and runtime material behavior.

## Feature combinations and alpha policy

**VERIFIED:** descriptor bits are separate for Face, SkinTint, Blend, and AlphaTest (`src/Render/PrepassTechnique.h:16,23,26,39`). The examined source does not enforce Face/SkinTint exclusivity.

**PROPOSED:** first opaque Face slice supports only a narrowly validated Face variant with no SkinTint, no Blend, no unsupported AlphaTest/queue mode, validated material and UV/resources, exact opt-in, and valid ownership/instance/GPU data. All other variants keep native CS rendering. This is a proposal, not a claim about the current set of runtime variants.

**UNKNOWN:** the exact supported matrix across OG, NG, AE, and later runtimes, plus runtime shader/material resolution for these asset controls. An OG capture cannot close other runtimes.
