# MATERIAL-01A — Material Identity Contract

**Status:** design proposal, not an implementation approval.

## Identity is a tuple of independent domains

| Domain | Proposed identity | Lifetime / meaning | Must not be substituted with |
|---|---|---|---|
| Persistent asset | Mod/author namespace + canonical game-relative model path + content digest; Shape declaration UUID within that file | Identifies authored bytes and one authored Shape declaration; digest changes when the asset changes | Loaded pointer, Actor FormID, pass descriptor, texture path alone |
| Runtime geometry | Engine object reference plus an engine-lifecycle generation/lease, if and when a safe lifecycle source is proven | Identifies one currently loaded Geometry instance | NIF path alone or raw pointer retained across frames |
| Shader pass | Runtime descriptor/technique + shader family/stage + pass kind, scoped to current render event | Describes one rendering route/variant | Material Role/Profile or durable Geometry identity |
| Material declaration | Validated declaration UUID and schema version, resolved from the authored asset | Author’s opt-in semantic assignment for a Shape | Actor ownership or guessed texture convention |
| Material Profile | Namespaced `Profile ID` plus immutable catalog snapshot generation | Reusable static model/resources/parameters; many Shapes and Actors can share it | Actor FormID, Head pose, command record pointer |
| Actor instance | Runtime Actor identity plus separately proven 3D generation | Scope for per-actor/per-frame data, only if needed by a later feature | Material identity or Shape role |

## Persistent resource key

**PROPOSED:** a resource key is `sourceNamespace + normalized game-relative path + SHA-256(content)`. Normalize separators and case according to the actual Windows game filesystem rules, reject traversal/absolute paths, and retain the effective winning provider as a diagnostic input. Provider precedence can change without the resource path changing, so the digest is required to distinguish replaced bytes. The key is for offline catalogs and cache invalidation; it does not assert that the runtime engine exposes the winning provider.

Within an NIF, attach a declaration to the Shape object and give it a declaration UUID. The Shape display name may be retained for diagnostics but is not required to be unique. Exporter reorder or shape rename must not silently bind an old declaration to a different Shape; offline validation should report stale or duplicated declaration IDs. A new asset digest creates a new persistent asset revision even if the author retains the declaration UUID.

**VERIFIED:** CommonLibF4 models named `NiExtraData`/`NiStringExtraData` and NIF objects with `NiObjectNET` extra data (`NiStringExtraData.h:7-48`, `NiObjectNET.h:31-51`). **UNKNOWN:** how every FO4 authoring tool and asset-generation path handles a new declaration entry. Its exact name, encoding, size limit, and tool round-trip must be tested before freezing.

## Runtime identity and lifetime

**PROPOSED:** runtime handles are `{index, generation}` in an owned registry; no raw engine pointer enters a Material Profile or survives an unproven lifecycle boundary. A runtime entry may temporarily hold a guarded engine reference only while executing inside a documented engine phase and after validating its generation. If no callback or ownership primitive proves a reference remains valid, the operation returns unavailable and uses Native shading.

Geometry, shader property, material object, texture set, Actor and scene nodes are separate native objects. Their pointer equality may support one synchronous lookup but is not a stable cross-frame identity. Command-buffer bake identity is evidence about a bake event; it is not proof that every later replay draw belongs to the same live Actor/Geometry.

## Role and pass separation

**VERIFIED:** the engine exposes material features (Face/SkinTint/HairTint/Eye) and render descriptors (including Face, SkinTint, Hair, Eye and AlphaTest) as distinct classification evidence (`BSShaderMaterial.h:17-69`; `ShaderFamilyDescriptor.cpp:62-119`). **PROPOSED:** resolver output includes both `MaterialRole` and compatible pass requirements. It must not promote a pass enum into a role declaration; a shader descriptor can authorize which shader variant is eligible to consume a profile, but an author declaration establishes semantic intent.

No rule maps Actor ownership to Role. No rule maps Role to Actor ownership. A Face role can be present on static preview geometry or non-Actor content; an Actor may own skin, eyes, hair, gear and effects simultaneously.

**VERIFIED — bounded Shinano NIF sample:** Outfit Studio exposed separate NIF Shapes for `head`, `head_rear`, `Eye`, `EyeWhite`, `EyeBrow`, `EyeLash`, `Hair_base`, and other face parts. The `EyeBrow`, `EyeLash`, and `Hair_base` samples all use `BSLighting:6`, while the `Eye` NIF includes one combined Shape rather than proven Iris/Pupil/Cornea Shapes. Thus Shape/material/pass evidence can corroborate a declaration, but this sample does not justify inferring every semantic role from shader type or file/texture names. The evidence source and limits are recorded in `MATERIAL-01A-S0-SOURCE-EVIDENCE.md`.

## Material Profile boundary

A Profile contains immutable, shareable shading-model selection, resource references, static parameter defaults, UV interpretation, alpha/queue requirement, and compatibility requirements. It excludes FormID, Head transform, animation pose, live object pointers, command-buffer records and any per-frame values. Those belong to explicitly separate runtime systems and remain out of scope until their identities and lifetimes are independently proven.
