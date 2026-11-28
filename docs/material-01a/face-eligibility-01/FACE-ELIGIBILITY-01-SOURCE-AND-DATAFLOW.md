# FACE-ELIGIBILITY-01 — Source and Data Flow

Baseline: production source `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`. Research branch `docs/face-eligibility-01`. Status labels: VERIFIED, INFERRED, PROPOSED, UNKNOWN, BLOCKED.

## Findings

### Descriptor and shader-variant data flow

- **VERIFIED** — `src/Render/PrepassTechnique.h:16,23,26,39` assigns distinct Deferred Prepass pass-enum bits to AlphaTest, Blend, SkinTint, and Face. These are F4SRF's source-level names for native pass flags; the shared spelling is not proof of the serialized NIF or BGSM field that originally caused each flag.
- **VERIFIED** — `src/Render/ShaderFamilyDescriptor.cpp:55-118` maps the descriptor bits to shader compilation defines, including `ALPHA_TEST`, `BLEND`, `SKIN_TINT`, and `FACE`. It consumes an already formed descriptor. The repository does not establish a complete NIF Shader Type/BGSM-to-descriptor conversion chain.
- **UNKNOWN** — The exact engine decision path from serialized NIF Shader Type, resolved `BSLightingShaderProperty`/`BSLightingShaderMaterial`, BGSM/BGEM state, and draw state to each Face/SkinTint/Blend pass bit. Static asset values therefore cannot be treated as runtime eligibility.
- **VERIFIED** — `features/SubsurfaceScattering/src/SkinProfile.cpp:49-65` implements `IsHumanSkinDraw`: it requires resolved base-race keywords, a pass, and either `kSkinTint` or `kFace`; then it resolves the Geometry's reference and Actor and accepts only a race matching the configured base-race keywords. Missing ownership/Actor/race leaves the conservative default and returns false.
- **VERIFIED** — `features/SubsurfaceScattering/src/SubsurfaceScattering.cpp:68-84` registers that bool classifier on lane W and observes Deferred Prepass techniques. `:241-248` treats Face OR SkinTint as a frame-level “skin drawn” signal. This is a broad skin/SSS selection, not a Face-only material classifier.
- **VERIFIED** — `src/Render/ShaderSubclassHooks.cpp:243-288,323-335,386-410` resolves the per-variant lane offset from the native pixel shader constant table and evaluates registered bool classifiers at Immediate SetupGeometry or CommandBuffer creation/build. Immediate classification sees the `BSRenderPass`; CommandBuffer classification sees the pass when baking constants. A baked record is not evidence of each later Replay Draw's Geometry identity.
- **VERIFIED** — the same registration path at `src/Render/ShaderSubclassHooks.cpp:443-466` installs the existing shared lane hooks once. This gives a current extension seam, but it has no unused lane component: the four existing owners retain X/Y/Z/W. Reusing it for a new face identity payload would conflict with current ABI ownership.
- **VERIFIED** — `SubsurfaceScattering.cpp:241-248` receives a shader and technique ID at technique bind and records frame-level activity, but no pass Geometry/Actor is passed to this callback. It is useful for variant observation, not ownership proof.

### Shader behavior and material class

- **VERIFIED** — `package/Shaders/DFPrepass.hlsl:240-268` lays out the SkinTint constants and `PrepassFeatureLane`; the lane is not a new free channel. Face behavior is guarded separately at `:367` and throughout the Face-specific normal/albedo code.
- **VERIFIED** — `package/Shaders/DFPrepass.hlsl:1503-1653` shows different alpha handling branches for Blend and non-Blend variants. In the non-Blend path, SkinTint and Face both write `0.019608` to `output.auxA.w` (`:1646-1653`). That encoded G-buffer class cannot distinguish those two features. Blend variants follow other alpha-specific branches.
- **VERIFIED** — `src/Render/PrepassTechnique.h` defines Face and SkinTint as separate bits, while SSS tests their OR. There is no source-level mutual-exclusion assertion in the examined classifier/descriptor mapping. **UNKNOWN** — whether both bits occur together in any supported runtime draw. A proposed Face-only consumer must reject the combined state until that case is characterized.
- **VERIFIED** — AlphaTest and Blend are represented as separate descriptor bits and HLSL branches. **UNKNOWN** — this source review does not establish complete engine render-queue ordering, native blend-state selection, nor all material-property interactions. Those remain independent compatibility gates.

### ARCH-01D runtime evidence boundary

- **REPORTED (accepted ARCH-01D R1 evidence)** — six CPU CommandBuffer Lane Bake records reported `lane_written=true`; Face descriptor `0x8000105E` had lane offset 12 and SkinTint descriptor `0x0004101E` had offset 16. This proves the CPU bake wrote the lane for those recorded variants.
- **REPORTED (accepted ARCH-01D R1 evidence)** — 11 CommandBuffer Create records were collected. CommandBuffer creation/bake is distinct from replay submission.
- **UNKNOWN** — Immediate Bake was not observed; actual Replay Draw ownership, per-GPU-draw Lane consumption, and synchronized Head pose were not proven. RenderDoc was not enabled in that run.
- **PROPOSED** — Reuse the accepted CPU Bake facts as source-specific evidence only; do not label them runtime material identity or GPU payload-consumption proof.

## End-to-end chain status

| Stage | Evidence | Status |
|---|---|---|
| Serialized NIF shader/property fields | Read-only NIF samples in NATIVE-03A asset matrix | VERIFIED as serialized fields only |
| Runtime resolved shader material | Engine object exists in the rendering path; field-to-meaning bridge not closed | UNKNOWN |
| Deferred Prepass descriptor | `BSRenderPass::passEnum` is consumed by existing code and mapped to defines | VERIFIED for source handling; runtime provenance limited to accepted samples |
| CPU lane classifier/write | Existing immediate and bake paths in source; accepted samples are bake-only | VERIFIED in source; runtime Immediate UNKNOWN |
| CommandBuffer Replay Draw association | No per-record ownership at replay established by this task | UNKNOWN |
| Pixel/GPU lane consumer | Shader source has consumers; accepted capture did not prove execution for each target draw | Source VERIFIED; runtime per-draw consumption UNKNOWN |

## Scope and isolation

No production C++, HLSL, tests, build, game launch, deployment, hook installation, push, or merge was performed for this research. The ARCH-01D runtime Hook installation remains frozen and is not reused or modified here.
