# FACE-TRANSPORT-02B-R1 — Review Corrections

## Identity and scope

- Reviewed base: `476fb0b54e7db9dde31fb67a885ee8c1cafb0098`.
- Production base: `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`.
- Branch: `feature/face-transport-diagnostic`.
- Scope: default-off isolation, bounded publication telemetry, diagnostic-only RT6 checks, representative shader validation, and exact dependency identity. No Face SDF, new engine Hook, RT7/t70/PrepassFeatureLane ABI change, deployment, or game launch.

## Default-off isolation

`face_transport::GetRegistrationPlan(false)` returns no shader-binding registration and no shared-target contributor. `FaceTransportDiagnostic::Load()` completes the no-op registration phase without calling either registration API and records `registration_required=false` / `registration_state=not_required_disabled`. `HasShaderDefine()` uses the same plan, so the disabled feature contributes no runtime define. Enabling the setting remains a next-launch choice. Extended Materials still registers the shared Parallax target independently and requests its ordinary R8 path.

The registration-plan contract is exercised by `FaceTransportPublicationContractTests`; source-to-test linkage is the exact helper consumed by both `Load()` and `HasShaderDefine()`. The offline test does not claim runtime shader compilation or game behavior.

## Publication telemetry and state

The shared RT6 publication callback exports a value-only snapshot. It contains configured/operational and registration state, requested/actual format, resource generation, frame serial, publication ID, resource readiness, binding attempted/confirmed, SRV binding confirmation, publication status, fallback status, and veto reason. The callback stores the snapshot under a mutex; telemetry takes a locked copy. No SRV, RTV, context, or resource pointer is retained by telemetry. Consumer eligibility remains hard-coded false.

Snapshot publication occurs on the prepass begin path, normal publication path, and each early-return path in `BeginPrepass()` / `PublishTargets()`. `PublicationState::BeginFrame()` resets per-frame readiness, binding, publication, and consumer fields while retaining the monotonic publication ID. Generation matching rejects zero and mismatched generations. Tests cover default-off registration, per-frame reset, generation mismatch, binding attempt versus confirmation, invalid publication, RGBA8 fallback, and fail-closed consumer eligibility.

## D3D11 check boundaries

New diagnostic-only `GetDesc`, `GetStageBindings`, and RT6 `OMGetRenderTargets` checks run only for the shared Parallax/Face-transport entry when its diagnostic contributor is active and a ready resource transaction exists. Existing generic target rebinds and normal `publishSlot` binding remain shared infrastructure. No new diagnostic state query is applied to ordinary entries or the RT7/SSS target.

`binding_attempted` is set when the existing deferred-prepass bind callback reaches the diagnostic RT6 entry. `binding_confirmed` is separately set only after the target is observed at the expected output slot on the matching immediate context. The existing thread-local frame-binding epoch and global dedupe cache are not treated as proof; actual output binding is checked at publication. Resource recreation invalidates the shared bind epoch and advances resource generation. Temporary COM references are released after pointer comparison.

Publication telemetry establishes only the checked RT6 allocation/binding transition. It does not prove same-light association, View identity, writer coverage, payload visibility after Terrain, or consumer validity.

## Shader checks and warning comparison

The focused shader harness uses `D3DCompileFromFile`, `ps_5_0`, optimization level 3, standard file includes, and the pinned Windows SDK `d3dcompiler_47.dll` (10.0.26100.9457). Eight representative prepass variants passed reflection and bytecode checks:

1. Parallax off, Face diagnostic off.
2. Parallax on, Face diagnostic off.
3. Parallax off, Face diagnostic on.
4. Parallax on, Face diagnostic on.
5. SSS off.
6. FACE+EYE diagnostic variant.
7. Non-Face BLEND negative control.
8. TEXTURE=1 Parallax material computation path.

Checks cover RT6/RT7 output presence and the synthetic marker instruction. The marker is present only in the intended Face opaque variants among this set; it is absent in the non-Face/BLEND control. This is offline shader evidence, not runtime descriptor or material-identity proof. The existing directional DFLight t66 reflection contract also passes.

Using the same DFLight source, profile, defines, include stage, and compiler, warning X4000 appears with both the production-base `ExtendedMaterialsConsumer.hlsli` and the candidate RGBA8-compatible consumer. The warning is inherited; the line moves from 13 to 14 because of the candidate file's added declaration. No warning suppression or Parallax behavior change was introduced. Full stdout is in `build/face-transport-review/r1/dflight-baseline.log` and `dflight-current.log`.

## Dependency identity

Verified recursive submodule gitlinks; none was changed:

| Path | Commit |
| --- | --- |
| `extern/CommonLibF4` | `9c3e9bf7ae99394a9c41b087f66afa0356b9a09c` |
| `extern/CommonLibF4/lib/commonlib-shared` | `5408d22adf937cc00e0ccd4bdeef66021172884d` |
| `extern/CommonLibF4/lib/dearmoddingui-api` | `acabd633c9d3b9c774247c635b6fe2a22f9807ea` |
| `extern/Streamline` | `a310480cec2ebe488e4989f9df6bffefdc34aba0` |
| `extern/bethesda-devbench` | `1d804c5241019ecc456a4c7d385109c90f56ce1f` |
| `extern/community-shaders-shared` | `ac739f9aff1a222a644033cb937ab10b960a3986` |

The two `commonlib-shared` entries are separate repositories and are recorded at their respective paths. This correction changes documentation only; gitlinks stay pinned.

## Build, package, and runtime status

The maintained full ReleaseDbg x64 plugin build completed after correcting one compile-time namespace qualification in telemetry. The build was run serially with the workspace lock: xmake `3.1.1+HEAD.3ba37a0d4`, VS 2022, MSVC `14.44.35207`, Windows SDK `10.0.26100.0`, `-j 1`. The first failed attempt is retained separately from the successful build log. The final DLL, PE/export inspection, candidate package file list, DLL SHA-256, and review-bundle manifest are recorded in `build/face-transport-review/r1/`.

No in-game validation was performed. Runtime acceptance remains pending and follows the revised [runtime validation plan](FACE-TRANSPORT-02B-RUNTIME-VALIDATION-PLAN.md). The candidate is not installed in MO2 or the game.
