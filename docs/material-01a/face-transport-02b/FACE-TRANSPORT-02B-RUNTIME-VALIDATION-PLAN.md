# FACE-TRANSPORT-02B — Runtime Validation Plan

## Status and candidate identity

This is a plan only. The revised candidate has not been deployed and the game has not been started. Runtime validation requires the complete isolated MO2 package produced from one reviewed commit; copying only the DLL is insufficient because the diagnostic shader define and RGBA8-compatible HLSL/include must match the binary.

Record the candidate commit, DLL SHA-256, and a manifest of every installed file. Verify the exact DLL path loaded by F4SE and the winning VFS source for each changed shader/include/config file. The runtime setting is next-launch state, so OFF and ON require two independent process launches.

## Session 1 — Diagnostic OFF

1. Install the complete isolated candidate package, with `EnableRGBA8Payload=false`.
2. Before launch, invalidate the Community Shaders shader cache using the project's normal cache invalidation mechanism. Record the cache path and confirm the session compiles/loads shaders from this candidate's HLSL and include files; do not mix cached DXBC from another candidate.
3. Launch Fallout 4 once. Record runtime, loaded DLL path/hash, shader VFS winners, and telemetry.
4. Confirm `configured=false`, `registration_required=false`, `registration_state=not_required_disabled`, `operational=false`, and `consumer_eligible=false` for FaceTransportDiagnostic. There should be no diagnostic shader contribution or diagnostic RT6 contributor. When Extended Materials Parallax is active, its shared RT6 remains R8 and its t66 consumer remains the existing red-channel path. The diagnostic must not allocate RGBA8 or emit a marker.
5. Capture only a bounded opaque Face Prepass range with RenderDoc MCP. Compare the output against the same scene/settings without the candidate diagnostic feature. Record any difference as a regression candidate; telemetry absence in OFF is not proof of GPU behavior.

Exit normally before proceeding.

## Session 2 — Diagnostic ON

1. Enable `FaceTransportDiagnostic.EnableRGBA8Payload`, restart the game, and repeat cache invalidation for the ON shader variant.
2. Confirm the same complete package identity and VFS winners. Telemetry must report configured/operational/registration state, requested and actual format, resource generation, frame serial, publication ID, resource readiness, binding attempted/confirmed, SRV binding confirmation, publication, fallback, and veto. `consumer_eligible` must remain false.
3. In a controllable scene, capture one opaque Face Prepass event and inspect RT6 as RGBA8. Verify the synthetic G/B marker only for the intended Face opaque shader variant; record RT6.R and alpha separately. A fallback to R8 must report `rgba8_allocation_fallback=true` and veto the diagnostic format.
4. With Parallax and SSS enabled, verify RT6 and RT7 coexist. Exercise a Terrain-covered view and retain the diagnostic veto; do not claim G/B validity through Terrain writers.
5. Inspect the matching directional DFLight event through RenderDoc MCP. Verify t66's reflected resource name/register and bound resource identity, and verify the existing Parallax read still consumes `.r`.
6. Compare ON against the OFF session with the same scene, camera, weather, lighting, resolution, and settings. Separate ordinary scene variation from reproducible differences. Any change to native direct diffuse, specular, SSS, GI, IBL, shadow, or attenuation is a failure to investigate; do not proceed to Face SDF.

## Evidence and limits

For each event, preserve runtime version, candidate and package hashes, event/shader identity, Face/BLEND/alpha variant evidence, RT6/RT7 formats and bindings, t66 slot/resource, telemetry snapshot identifiers, and selected pixel values. Use RenderDoc MCP, with one RDC loaded at a time.

This candidate can test allocation, binding/publication diagnostics, the synthetic marker, and preservation of the RT6.R/t66 contract. It does not prove CPU Bake-to-GPU Draw identity, same-light pairing, exact View association, complete writer coverage, Terrain-safe G/B visibility, or an eligible lighting consumer. `consumer_eligible=false` is intentional. No result authorizes Face SDF or a lighting change.
