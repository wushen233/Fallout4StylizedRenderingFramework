# FACE-TRANSPORT-02B — Runtime Validation Plan

## Status and prerequisites

This is a plan only. The candidate is not deployed and the game was not started. Use the reviewed DLL only after the normal owner-controlled installation step. The diagnostic format and shader define are fixed at startup; changing `FaceTransportDiagnostic.EnableRGBA8Payload` requires a fresh game start.

The candidate currently has **no eligible diagnostic light consumer**. Same-light producer/consumer, exact View identity, terrain visibility, and payload readback are still unknown. Runtime observations must not be presented as Face SDF or lighting validation.

## One bounded acceptance session

Keep one RenderDoc capture loaded at a time and capture only the relevant frame/event ranges.

1. **Diagnostic OFF launch:** leave `EnableRGBA8Payload=false`. Verify the feature loads with its default settings and the ordinary Community Shaders output is unchanged. If Parallax is enabled, verify its RT6 allocation remains R8 and its t66 consumer remains bound; with Parallax inactive, the diagnostic contributor should not allocate/activate the target. Record feature state and candidate log identity.
2. Exit normally, set `EnableRGBA8Payload=true`, then start a fresh session. This second start is required because resource format and shader define are startup-fixed.
3. **Diagnostic ON launch:** capture an opaque Face Prepass draw. Inspect RT6 as RGBA8, verify the synthetic G/B marker on the draw's supported pixels, and record RT6.R and alpha. Confirm the matching directional DFLight t66 binding retains the reflected name/register and that the existing Parallax read remains `.r`.
4. In that same ON session, verify Parallax and SSS are enabled and observe RT6/RT7 coexistence. Exercise a Terrain-covered view and record that diagnostic consumer eligibility remains vetoed/false; do not infer final G/B validity from the marker alone.
5. Compare the ON session's native lighting output against the OFF baseline. There must be no diagnostic change to direct diffuse, specular, SSS, GI, IBL, shadow, or attenuation. Any visual difference is a failure requiring investigation; do not proceed to Face SDF.

The OFF and ON phases are two launches within one validation task. A single process cannot verify both formats because the setting explicitly applies next launch.

## Required evidence per event

- Runtime version and candidate DLL SHA-256.
- Event ID, shader identity, Face feature/variant evidence, RT6/RT7 formats, and render-target bindings.
- RT6 resource ID and resource generation where available; t66 slot, reflected resource name, and bound resource ID at the consumer event.
- Pixel History or direct readback for R/G/B/A at selected pixels, with blend/replay context.
- Diagnostic enable state and publication/veto state, including frame and View/publication association if such association becomes observable.
- Parallax and SSS enable state.

## Acceptance limits

This candidate can prove allocation, target binding, synthetic producer writes, and preservation of t66's R-channel contract if those are observed. It cannot currently prove same-light pairing, complete producer-to-consumer publication identity, Terrain-safe final payload visibility, or that the GPU consumes G/B. `consumerEligible=false` is intentional. No runtime result authorizes enabling Face SDF or changing the lighting response.
