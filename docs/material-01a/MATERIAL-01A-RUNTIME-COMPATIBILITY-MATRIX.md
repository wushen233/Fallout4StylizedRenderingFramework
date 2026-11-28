# MATERIAL-01A — Runtime Compatibility Matrix

**Status:** source-supported project targets are not equivalent to S0 runtime validation. OG 1.10.163 has inherited ARCH-01D diagnostic observations; they do not establish architecture compatibility across runtimes.

| Runtime | Project target evidence | Shared contracts likely reusable | Runtime-specific evidence required | S0 status |
|---|---|---|---|---|
| OG 1.10.163 | README names supported target; xmake defines OG shader identity and feature compilation (`README.md:31-35`; `xmake.lua:473-480`) | Declaration schema, canonical Profile IDs, resource semantics, role vocabulary and offline validator can be runtime-independent | Address Library IDs, CommonLib layouts, shader descriptor/constant-table interpretation, hook/ABI and GPU resource lifecycle | **INFERRED supported by project; diagnostic bake observations reported; material contract not run-tested** |
| NG 1.10.984 | README names target; AE shader identity recipe covers `FeatureShaderCompileAE` (`README.md:31-35`; `xmake.lua:470-472`) | Same serialized declaration/profile schema and validation rules | Runtime-specific plugin ABI, Address Library/CommonLib resolution, descriptor variants, shader cache, material load/provider behavior | **SUPPORTED target declared; independent runtime execution evidence UNKNOWN** |
| AE 1.11.240 | README names target; AE shader identity/feature recipe in CI (`README.md:31-35`; `xmake.lua:470-472`, `.github/workflows/pr.yml:156-160`) | Same serialized declaration/profile schema and validator | Runtime-specific addresses/ABI, shader reflection/layout, alpha/pass coverage, resource and cache invalidation | **SUPPORTED target declared; independent runtime execution evidence UNKNOWN** |
| Later Fallout 4 runtime | Existing architecture target language asks to follow upstream support (`docs/rebuild/README.md:23`) | Schema can remain portable if explicitly versioned | Must wait for Community Shaders/CommonLib/F4SE support and validate every native ABI and shader assumption | **UNKNOWN / not presumed supported** |

## Common contract versus runtime ABI

**PROPOSED common contract:** canonical asset/profile identifiers, declaration schema, enum values and parameter/resource meanings are serialized independently of Fallout runtime addresses. Runtime-specific loading translates those values into engine objects only after verifying exact runtime support.

**Runtime-specific ABI:** F4SE/CommonLib types/layouts, Address Library relocations, engine object traversal/lifetime, `BSGeometry`/shader-property layouts, native descriptor constants, per-call constant buffers, hook signatures and D3D resources. None of these may be cached in a cross-runtime Profile file.

**VERIFIED:** the project requires Address Library and lists OG/NG/AE versions in `README.md:31-35`; CI recipes include AE and OG shader identity/compile paths (`xmake.lua:470-480`, `.github/workflows/pr.yml:126-160`). **UNKNOWN:** these build/compile checks do not prove correct live object mapping, loaded asset winner, material resolution, or visual output on each runtime.

## Required compatibility gate per runtime

Before runtime material resolution is enabled for a target runtime, require: exact supported executable/F4SE/Address Library combination; compile and tests using its pinned or project-required toolchain; shader identity/reflection for the relevant descriptors; smoke evidence for declaration loading, missing/invalid fallback, resource replacement and cache invalidation; and explicit evidence that upstream PBR, SSS, IBL and shadow consumers remain intact for both opted-in and unmarked materials. Label build, ABI/static, in-game, and visual results separately.

OG results are one row of evidence. They do not waive NG or AE validation. If one runtime fails a check, keep that runtime’s stylized resolver disabled while the shared offline authoring contract remains usable.
