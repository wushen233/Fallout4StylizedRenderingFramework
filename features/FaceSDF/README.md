# Directional Face SDF (FACE-01A)

This feature is experimental, disabled by default, and has no bundled profiles or textures. Enabling the feature alone does not opt any material into NPR.

## Profile contract

Each profile is an explicit compatibility opt-in. The feature currently requires:

- `id`: namespaced logical identifier, for example `avatars.shinano:face.default`.
- `race_plugin` and `race_local_form_id`: resolve to a `TESRace` after data load. The local ID is a hexadecimal string scoped to that plugin.
- `shape`: exact runtime geometry name. This selector is only used together with the explicit profile, resolved Race, Face material feature, and opaque skinned pass gates.
- `sdf_texture`: a relative `Data\\...` DDS path. The supported source formats are non-sRGB `R8G8_UNORM` or `R16G16_FLOAT`, one 2D texture, one array slice. The two channels encode the author-defined directional signed-distance pair.
- `forward`, `right`, `up`: author-verified, normalized, right-handed model-space axes. No universal head forward axis is assumed.

Optional fields are `uv_scale_bias` (default `[0, 0, 1, 1]`), `softness` (default `0.03`, accepted range `(0.001, 0.25]`), and `strength` (default `1`, range `[0, 1]`).

The plugin configuration uses the ordinary feature table. This example is a template only: replace the Race, exact runtime Shape name, model-space basis, and DDS path with values verified for the target asset. No profile or texture is shipped, so the feature remains visually inactive until an author supplies a valid profile and resource.

```toml
[features.FaceSDF]
load = true
enabled = true

[[features.FaceSDF.profiles]]
id = "avatars.example:face.default"
race_plugin = "ExampleRace.esp"
race_local_form_id = "000800"
shape = "VerifiedFaceShape"
sdf_texture = "Data\\Textures\\F4SRF\\ExampleFaceSdf.dds"
forward = [0.0, 0.0, 1.0]
right = [1.0, 0.0, 0.0]
up = [0.0, 1.0, 0.0]
uv_scale_bias = [0.0, 0.0, 1.0, 1.0]
softness = 0.03
strength = 1.0
```

Malformed, ambiguous, unresolved, missing, unsupported, or incompatible profiles do not affect rendering. Invalid SDF samples, basis vectors, transport publication, view association, or directional-light association use the native diffuse factor. SkinTint, blended, alpha-tested, landscape, instanced, combined, tessellated, and command-buffer prepass draws are not eligible in this slice.

## Transport behavior

The immediate prepass transforms the profile's model-space basis through the current draw's b10 skinning palette, interpolates it in view space, and validates/orthonormalizes it per pixel. The prepass writes the SDF factor to shared RT6.G and a per-pixel opt-in marker to RT6.B. RT6.R remains the existing Parallax payload; alpha remains owned by the existing prepass behavior. DFLight consumes the factor only when the CPU-side publication/view checks pass and the shader's current directional light matches the shared world-space sun direction after transformation to view space.

The runtime test evidence accepted for the RT6 producer predates FACE-01A and does not validate this SDF consumer. This feature still needs shader compilation, candidate package inspection, and owner-run in-game visual acceptance before use.
