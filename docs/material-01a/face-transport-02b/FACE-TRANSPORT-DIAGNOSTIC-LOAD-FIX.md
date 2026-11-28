# Face Transport Diagnostic Load Fix

## Finding

The paired ON-attempt log telemetry reports the process-effective values `configured=false` and `registration_required=false`. The setting is next-launch-only, so a selected menu value can be enabled while the current process still runs the OFF registration plan. The screenshot's missing-route error is consistent with that state: `ShaderInjectionFeature::ValidateShaderInjections()` unconditionally demanded a registered route even though `FaceTransportDiagnostic::Load()` intentionally registered none for that plan.

This was a real lifecycle bug: a valid no-op configuration was marked failed, which also obscured the diagnostic's settings page. It did not prove that RGBA8 transport was active. A configured ON process must still register and validate its routes; a missing ON route remains a hard failure.

## Change

`ShaderInjectionFeature` now allows a feature to declare that its current configuration requires no shader routes. `FaceTransportDiagnostic` derives that decision from the same registration plan used by `Load()`, and the shared route-requirement predicate is covered by `FaceTransportPublicationContractTests`.

When disabled, validation succeeds as a no-op while the injection-operational gate remains false. When enabled, the existing prerequisite, route, and variant validation runs unchanged. No RT6, shader, or transport behavior changed.

## Verification and remaining runtime check

- Full `FO4CommunityShaders` ReleaseDbg x64 build: passed with `xmake build -P . -j 1 FO4CommunityShaders` (xmake 3.1.1+HEAD.3ba37a0d4; serialized).
- `FaceTransportPublicationContractTests`: built and passed.
- Pinned clang-format 22.1.4 check: passed for all changed C++ files.
- `git diff --check`: passed.
- In-game validation: not run; no deployment or game launch was performed.

On the next candidate run, the menu should no longer show a missing-route error for the OFF process and should expose the setting. After starting a fresh process with the setting effective, telemetry must report `configured=true` and `registration_required=true`; the registered ON routes must then pass validation. If that fresh-process telemetry remains false, the next investigation is limited to the setting's persisted next-launch value and its startup readback. No game, MO2, or user configuration state was inspected or changed here.
