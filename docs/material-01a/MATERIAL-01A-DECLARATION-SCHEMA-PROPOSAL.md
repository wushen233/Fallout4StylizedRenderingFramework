# MATERIAL-01A — Declaration Schema Proposal

**Status:** all names, encoding and validation behavior below are **PROPOSED**. The CommonLib API for named NIF string extra data is **VERIFIED**, but no proposed F4SRF key or serialized form has been round-trip tested.

## Authoring model

Prefer a Shape-scoped NIF declaration for opt-in identity and an external profile catalog for reusable parameters/resources. This prevents duplicated profile settings across many NIFs while keeping the exact role binding next to the Shape that needs it.

**PROPOSED NIF carrier:** one `NiStringExtraData` named `F4SRF.MaterialDecl`; its string value is canonical UTF-8 JSON with duplicate-key rejection and bounded size. Example:

```json
{"schemaVersion":1,"declarationId":"7e7ecdbc-713e-4d0b-8f17-60429098b979","profileId":"author.package:characters.face.default","role":"Face"}
```

This is only a candidate. Before adoption, test write/read round-tripping in the supported NIF editor(s), FO4 NIF version variants, FaceGen/export tools, loose files and BA2 delivery. If JSON strings are awkward or unstable in the toolchain, retain the conceptual fields but use a documented line-oriented encoding or a typed extra-data representation instead. Do not ship a parser until one representation passes these tests.

## Schema v1 conceptual fields

| Field | Required | Contract |
|---|---:|---|
| `schemaVersion` | yes | Positive integer; unknown major version fails closed. Minor evolution must be defined before use. |
| `declarationId` | yes | UUID unique within the asset and stable for that authored Shape declaration. Duplicate IDs within one asset invalidate both declarations. |
| `profileId` | yes | Case-sensitive, namespaced logical ID (`author.package:profile.name`); no filesystem path semantics. |
| `role` | yes | Closed-on-read vocabulary for the schema version; unknown value is invalid, not coerced to Generic. |
| `modelScope` | no | Omit when the declaration is embedded on the Shape; sidecar declarations must specify a canonical model resource key. |
| `shapeSelector` | no | Sidecar-only selector; require an explicit Shape-local declaration ID or an unambiguous stable selector. Shape name alone is not guaranteed unique. |
| `priority` | no | Reserved for profile catalog selection only; do not permit arbitrary per-NIF priority to override conflicts. |
| `extensions` | no | Namespaced extension bag; unknown keys ignored only if the schema specifies forward-compatible semantics, otherwise reject. |

## External profile catalog

**PROPOSED:** `profiles/<author-or-pack>/<profile>.json` (or TOML after validator review) carries its own `profileSchemaVersion`, `profileId`, supported material roles, shading model, required/optional resources, parameters, UV contract, alpha/queue requirements, and compatibility declarations. Catalog IDs are globally unique within a loaded snapshot. Catalog files reference relative assets using canonical, traversal-free paths and may not override declaration identity.

Profile-level precedence is resolved at catalog assembly time, not by selecting the “highest-looking” file in a shader callback. If two equally prioritized definitions have the same ID and different content, the entire ID is conflicted and unresolved. An explicit authorized override must name the overridden profile ID and publish a new validated catalog snapshot.

## Validation constraints

- Reject malformed JSON, duplicate keys, unknown required enum values, non-finite/out-of-range numeric parameters, invalid UUIDs, path traversal, missing profile IDs, duplicated declaration IDs, incompatible schema major versions, and unsupported resource encodings.
- Require all profile-declared required textures; missing optional textures use a declared neutral/default resource only when that default is mathematically valid. Otherwise the profile is invalid.
- Validate the declared UV channel/transform and texture color space per resource. Scalar/data maps (for example SDF/mask) are linear; color maps state whether they are sRGB. No implicit filename convention establishes role or encoding.
- Validate the expected native shader/material evidence and descriptor compatibility as separate gates. A declaration cannot make an unsupported pass variant safe.
- Treat a fully valid declaration/profile pair as opt-in. Missing, malformed, duplicated, ambiguous, incompatible or resource-incomplete entries resolve to Native.

## Scope and revalidation

The validator should run offline against the effective asset package before runtime. Runtime loading may recheck file/catalog digests and schema version, but must not perform unbounded directory scans or file I/O in a draw hook. Any profile catalog update produces a new immutable snapshot generation; existing handles from the old generation are rejected or explicitly translated after validation.
