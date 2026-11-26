# MATERIAL-01A — Role and Profile Contract

**Status:** role names remain candidates. No final public enum is approved in S0.

## Candidate roles and evidence limits

| Candidate | Native evidence that may help | Contract assessment |
|---|---|---|
| Face | Face material feature / Face prepass descriptor; author Shape declaration | **INFERRED:** strong eligibility clue but not durable author intent; require explicit declaration for NPR. Shinano `head` is `BSLighting:4` and not `skinTinted`; this is sample-specific, not a universal Face rule. |
| Skin | SkinTint feature/descriptor, loaded lighting material, authored declaration | SSS treats Face and SkinTint as skin-relevant, so descriptor alone does not distinguish semantic coverage. Shinano `head_rear` is `BSLighting:5` with `skinTinted=true`; require declaration where model behavior differs. |
| Eye Iris | Eye feature, texture/material and Shape structure | Shinano has a combined `Eye` Shape; inspected NIF did not demonstrate a distinct Iris Shape. Require explicit role declaration or a validated submesh/region contract. |
| Eye Sclera | Same coarse Eye evidence | Shinano separately supplies `EyeWhite` (`BSLighting:2`), which is useful corroboration for sclera but still not a durable author contract. |
| Eye Cornea | Eye/env/refraction flags and alpha state may help | Shinano's `Eye` Shape has alpha test (`GREATER`, 128); this does not prove a distinct Cornea role. Declaration plus alpha/queue validation required. |
| Eye Pupil | Eye feature and Shape structure | No separate pupil Shape established in the inspected Shinano NIFs; require explicit declaration or validated submesh semantics. |
| Hair | Hair/HairTint feature, hair tint flag/material fields, alpha-tested hair geometry | Shinano separates `Hair_base` but it shares `BSLighting:6` with its `EyeBrow` and `EyeLash` samples. Shader type alone cannot identify Hair; explicit declaration is required. |
| Eyelash | Often shares hair/face-like shader/alpha paths | Shinano `EyeLash` is a separate Shape but uses `BSLighting:6`, shared with sampled hair/brow Shapes. Require declaration and separate alpha-path eligibility. |
| Eyebrow | May use face, hair or alpha-tested geometry depending on asset | Shinano `EyeBrow` is separate but also uses `BSLighting:6`; require declaration. |
| Generic Stylized Material | Explicit opt-in declaration | Author-only role; never inferred from actor ownership or texture names. |
| Unknown | No valid declaration or unresolved evidence | Always Native fallback; not an opt-in style. |

**VERIFIED:** CommonLib’s `BSShaderMaterial::Feature` includes Face, SkinTint, HairTint and Eye (`include/RE/B/BSShaderMaterial.h:17-41`); shader property flags include eye reflection, hair tint, FaceGen RGB tint and alpha-test (`BSShaderProperty.h:44-110`). These flags can corroborate the authored claim and reject an incompatible pairing. They do not encode the complete list of requested subroles.

**VERIFIED — bounded Shinano sample:** Outfit Studio MCP inspected the main head, rear head, eye, eye-white, brow, lash, highlight, face-part, hair, teeth and tongue NIFs in `Avatars_Race - Main.ba2`. This sample shows explicit Shape separation for several roles, overlapping shader types across brow/lash/hair, and a combined Eye Shape. **UNKNOWN:** whether other Shinano variants, FaceGen outputs, or runtime-loaded overrides preserve the same layout, and whether the BGSM contents change any of these conclusions. The role taxonomy must remain extensible and versioned; each role needs explicit declaration plus tested compatibility/fallback semantics.

## Role versus shading model

Role answers “what authored surface is this?” Shading model answers “which algorithm evaluates it?” A Face role may use Native, NPR SDF, or a future supported hybrid model; an arbitrary Role must not silently imply a specific model. A global Style Profile may select a model for valid declarations, while each Material Profile defines parameters/resources for that role-model combination.

## Material Profile responsibilities

**PROPOSED profile fields:**

- `profileId`, schema version, supported roles and required pass/material compatibility evidence.
- Shading model identifier and algorithm version.
- Static color/roughness/specular/SSS/ramp/SDF parameters, with typed units/ranges and explicit defaults.
- Required and optional texture/resource references, color-space/sampler requirements, UV channel and transform convention.
- Opaque/alpha-mask/transparent requirement, expected sorting/depth/write behavior and compatible engine paths.
- Explicit feature compatibility (for example whether underlying upstream SSS or PBR contribution remains active for this model).
- Fallback policy, constrained to Native or a prevalidated compatible profile; never an implicit guessed role.

Profile excludes Actor FormID, actor ownership, Head Transform, animation pose, live engine pointers, command-buffer-record pointers, instance index/generation and all frame-varying values. Multiple actors and multiple NIF instances may share one Profile and its immutable resources.

## Alpha and render queue

**VERIFIED:** native geometry has an alpha property, and descriptors distinguish alpha test (`NiAlphaProperty.h:16-117`; `ShaderFamilyDescriptor.cpp:70-73`). **PROPOSED:** S0/S1 first declare only opaque and alpha-mask paths that are proven to enter compatible deferred passes. Transparent/refractive roles require a separately designed route with sorting, depth, coverage, and composition guarantees. Do not classify transparent draws as opaque Actor Coverage or opt them into a deferred opaque material model based only on a declaration.

## Preserving Community Shaders

Every style override is opt-in and scoped to a valid resolved declaration. Unmarked content follows the existing Community Shaders path. When an NPR profile is active, integration must identify which upstream PBR/SSS/IBL/shadow terms remain authoritative and prevent duplicate or conflicting lighting decisions. S0 does not settle that per-model equation and does not authorize global Toon takeover, lane/RT changes, t70 changes, or a GPU instance table.
