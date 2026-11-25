# Legacy ENMS-B4 final audit and F4SRF design constraints

2026-11-26. Disposition: STATIC FAIL / LEGACY FROZEN RECOMMENDED. No further legacy algorithm implementation. Historical runtime attribution remains unverified.
Review: https://github.com/wushen233/Fallout4CommunityShaders-Endfield/pull/3#pullrequestreview-6066125044
Exact mirror f99822ded1f21587ec12bfd35c1578b6e808cf22; tree 521e8d078acca68f1d01762f7b39a85618e889cf matches reported local snapshot c7cd5a2f02b10745a3b79fa7574b63470586847c. Mirror has different ancestry. Actual reported runtime was 4a5598d77097b098aa4c29425e3dee1a376b1184-dirty. The disk DLL hash does not establish winning provider or loaded bytes. No new build/RenderDoc capture executed for this audit.

## Proven defects in reviewed source
1. DFLight.hlsl:790–813 has the only Face SDF production read, under BSDFLIGHT_PS_DEFERRED (69) / LIGHT_TYPE == LIGHT_TYPE_DIRECTIONAL (361). ShaderFamilyDescriptor.cpp:282–308,326–343,419–422 defines LIGHT_TYPE=3 for native kDeferred; the block is excluded under that mapping. The actual DIRSPLITS1/2/3 sun families at 2866–2891,3975–4000,5164–5189 use legacy NdotL/second-shadow/refinement with no Face SDF. UNSHADOWED also lacks the consumer. This is a production family reachability defect, not merely a bad SDF texture. No claim that this exact tree was the historical loaded binary.
2. DFComposite.hlsl:1835–1848 displays RT7 blue in DebugMode9; normal Toon branch computes legacy bands (2002–2068) and early returns albedo × legacy tone (2107–2153). It does not consume Face SDF. Hybrid uses stock composite, so do not claim Toon early return overwrites Hybrid.
3. Even in a hypothetical reachable consumer, finalDiffuse is attenuated by lerp(1,faceLit,shadowStrength*hybridNprStrength), defaults 1 and .35. This cannot independently replace the native NdotL response or guarantee visible contrast; ambient/specular remain. SecondShadowStyle.z is hybrid strength, not second-shadow strength (.y). Shared shadowBorder/softness also control SDF (DFPrepass:1919–1922). No independent artistic face response.
4. Global hardcoded Shinano DDS (Endfield.cpp:516–541) is not a per-material resource contract. Head yaw captures current head forward but loses pitch/roll; command replay freshness unproven. RT7 blue shares red SSS/green owner; existing green-only blend masking is not evidence of correct blue lifecycle. No automatic migration.
5. RuntimeShaderTests.cpp:200–245 tests Prepass compile and sampler reflection, not normal lighting consumer reachability or pixel output. Debug success and compile success are not visual acceptance.

## New implementation constraints
- Map actual runtime descriptor -> compile recipe -> reflected shader -> producer -> consumer -> final image on exact clean upstream. Never choose a branch from comments or name alone; every claimed supported light family needs proof. Include shadowed directional split variants and unshadowed directional; local/tiled/ambient/transparent handling explicitly defined; don't modify shadow-only pass as if it were diffuse.
- Separate material identity, per-material resource binding, head transform, per-pixel validity and artistic light response. No old production marker, old global Toon Takeover, old RT7/t70 protocol or hardcoded asset default inherited. Native fallback for unsupported/invalid input.
- Single authority for artistic diffuse response per material. Preserve upstream radiance, real cast-shadow visibility, indirect/ambient, reflections and SSS where intended; measure attenuation/energy and color separately. Face SDF is not just a multiplier of an already dark normal-based term; design for authored lit/shadow tone within physical scene lighting. Avoid global legacy bands or second shadow overriding the selected model.
- Dedicated Face controls; angle encoding and UV resource contract, per-head coordinates and dynamic freshness; prove runtime binding, render-target lifecycle/clearing/writes, native fallback, shader cache/version, actual output. No diagnostic signature as production identity.
- Minimal production vertical slice before adding next function. Exact DLL/Shader/winning provider + normal-image Off/On + target pixel evidence + reference light directions; use DevBench/RenderDoc as supported. Upstream reuse audit and dual-style product goals remain unchanged.
- Old local Git commits/LFS/artifact/author resources must remain backed up; remote mirror alone does not preserve full local ancestry. Archive old repository read-only after preservation, do not merge B4. New repo retains only lessons, not old production code.

## Source anchors
https://github.com/wushen233/Fallout4CommunityShaders-Endfield/blob/f99822ded1f21587ec12bfd35c1578b6e808cf22/package/Shaders/DFLight.hlsl#L790-L813
https://github.com/wushen233/Fallout4CommunityShaders-Endfield/blob/f99822ded1f21587ec12bfd35c1578b6e808cf22/src/Render/ShaderFamilyDescriptor.cpp#L282-L343
https://github.com/wushen233/Fallout4CommunityShaders-Endfield/blob/f99822ded1f21587ec12bfd35c1578b6e808cf22/src/Render/ShaderFamilyDescriptor.cpp#L419-L422
https://github.com/wushen233/Fallout4CommunityShaders-Endfield/blob/f99822ded1f21587ec12bfd35c1578b6e808cf22/package/Shaders/DFComposite.hlsl#L2107-L2153
https://github.com/wushen233/Fallout4CommunityShaders-Endfield/blob/f99822ded1f21587ec12bfd35c1578b6e808cf22/package/Shaders/DFPrepass.hlsl#L1886-L1930
