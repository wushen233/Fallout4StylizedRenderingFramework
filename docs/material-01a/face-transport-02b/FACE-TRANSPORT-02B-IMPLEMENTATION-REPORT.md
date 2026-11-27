# FACE-TRANSPORT-02B — Implementation Report

> Historical report for the pre-R1 candidate at `476fb0b54e7db9dde31fb67a885ee8c1cafb0098`. Its DLL hash and installation output are not the R1 candidate. R1 corrections, final build identity, and review artifacts are recorded in [FACE-TRANSPORT-02B-R1-REVIEW-FIXES.md](FACE-TRANSPORT-02B-R1-REVIEW-FIXES.md) and the generated `build/face-transport-review/r1/` bundle.

## Result

**Candidate implementation and targeted offline validation: PASS. Runtime acceptance: NOT RUN.**

The worktree is `feature/face-transport-diagnostic`, based directly on production baseline `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`. FACE-TRANSPORT-02A evidence remains on its separate worktree at `d5faa07b6553cb9b5ddc3030a49870f37dd27020`; it was used as evidence and was not merged into this source branch. No production runtime was started, and no candidate was deployed.

## Shared RT6 owner

`src/Render/PrepassExtraTargets.*` now exposes a dedicated shared Parallax/Face-transport registration path. Extended Materials and the diagnostic contributor share RT6 slot 6 and the existing pixel SRV slot t66. The shared resource checks device identity, dimensions, and DXGI format; recreations advance a resource generation. RGBA8 allocation failure falls back to R8, which preserves the existing scalar Parallax red channel and leaves the diagnostic consumer ineligible.

The entry tracks allocation, per-prepass readiness, active writing, whether the registered target binding path was observed, publication identity, frame serial, format, and consumer eligibility separately. Publication checks the resource description, SRV binding, and that RT6 is no longer bound as an output. These checks establish a bounded publication record; they do not establish which View produced the resource or that all intervening writers preserve its payload.

`GetPrepassExtraTargetSRV` retains its prior readiness semantics for existing users. The new publication query is separate. Diagnostic `consumerEligible` remains deliberately false because View identity, same-light association, and supported-writer coverage are unverified. The veto reports either unavailable diagnostic format or unverified View/light association. No diagnostic G/B sampling was added to the lighting result.

## Diagnostic producer and shader contract

`FaceTransportDiagnostic` is a separate feature with `EnableRGBA8Payload=false` by default. The setting applies on next launch, fixing the resource format and shader variant for the session. It does not require Extended Materials. With the diagnostic disabled, the shared target stays on the R8 path unless Parallax itself requires it. With it enabled, RGBA8 is requested, but R8 fallback remains safe for Parallax and disables diagnostic eligibility.

For supported opaque Face prepass variants only, the diagnostic writes synthetic RT6 G/B values `(0.5, 1.0)`. Blend, Landscape, and tessellated displacement variants do not receive the synthetic Face payload. RT6.R and output alpha retain the existing behavior; Terrain Color Replay continues to write its blend factor to alpha. This payload is a transport marker only. It is not SDF, proof of Face material identity, or NPR opt-in.

`ExtendedMaterialsConsumer.hlsli` uses one `Texture2D<float4>` declaration at the existing t66 resource name and explicitly reads `.r`, preserving the scalar Parallax contract for R8 and RGBA8 resources. RT7, t70, `PrepassFeatureLane`, Terrain blend state, and lighting equations are unchanged.

## Terrain and visibility limits

The TerrainBlending success order was corrected to **Opaque Replay → Color Replay → Data Replay** (`features/TerrainBlending/src/TerrainBlending.cpp:282–317`). CopyObjectDepth failure takes Stock Replay and returns. The diagnostic does not change these paths or the Terrain blend state.

The shared target cannot claim exact final G/B validity: Terrain RT6 writes do not guarantee G/B, alpha blending can leave stale values, and UNORM8 quantization can turn a small blended marker into 255. No View-level Terrain veto or diagnostic consumer was implemented. Consumer remains fail-closed until the View/publication and writer coverage are proven.

## Verification

- Full ReleaseDbg x64 plugin build succeeded with the locked submodule gitlinks. Log: `build/face-transport-review/releasedbg-build.log`.
- `FaceTransportPublicationContractTests.exe`: exit 0.
- Targeted SM5 compile/reflection checks: five contracts PASS (four Face/Parallax combinations and one directional DFLight binding). The DFLight compiler emitted warning X4000 for potentially uninitialized `ExtendedMaterials::GetParallaxShadow`; compilation and reflection succeeded. See `build/face-transport-review/shader-evidence/shader-reflection.txt` and `targeted-shader-check.log`.
- The Face diagnostic variant DXBC writes `o6.xyzw = (1.0, 0.5, 1.0, 1.0)`; its non-diagnostic counterpart has no `o6` output. SSS RT7 remains reflected in the four Prepass variants.
- The directional shader reflects `ExtendedMaterials::ParallaxShadowTexture` at t66; the source name and `.r` consumption are retained.
- Pinned clang-format 22.1.4 checks and `git diff --check` passed.
- FACE-TRANSPORT-02A RTX 4060 and WARP fixture results (10/10 each) are accepted prior evidence, not rerun in this task.
- Full shader regression matrix, Runtime variants, in-game RT6 inspection, same-light association, Terrain visibility, and GPU payload consumption: **NOT RUN / NOT VERIFIED**.

## Candidate artifact

- DLL: `build/windows/x64/releasedbg/FO4CommunityShaders.dll`
- Size: 4,759,040 bytes
- SHA-256: `8FB7D39E2525EFE1CAF3CC8B167E61E0341D2A2D60DAAC8098CB907242B2B74F`
- PE: AMD64 / PE32+
- F4SE exports: `F4SEPlugin_Query`, `F4SEPlugin_Load`
- Build: ReleaseDbg x64; xmake `3.1.1+HEAD.3ba37a0d4`; MSVC toolset directory `14.52.37025`
- Dependency gitlinks exactly as pinned by the production baseline: CommonLibF4 `9c3e9bf7ae99394a9c41b087f66afa0356b9a09c`; community-shaders-shared `ac739f9aff1a222a644033cb937ab10b960a3986`; Streamline `a310480cec2ebe488e4989f9df6bffefdc34aba0`; DevBench `1d804c5241019ecc456a4c7d385109c90f56ce1f`. The earlier handoff note's `5408d22...` value does not match this branch's gitlink; no gitlink was changed.

The pre-R1 candidate was not installed in MO2 or the game. Runtime compatibility is not established by that OG-targeted build.
