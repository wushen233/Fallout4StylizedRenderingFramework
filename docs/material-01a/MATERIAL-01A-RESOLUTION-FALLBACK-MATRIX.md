# MATERIAL-01A — Resolution and Fallback Matrix

**Status:** deterministic behavior is proposed. Unresolved/conflicted input must fail closed to Native.

## Resolver stages

| Order | Input | Required result |
|---:|---|---|
| 1 | Asset resource key: namespace, canonical game-relative path and content digest | Must identify the exact loaded/validated asset revision. If the runtime provider/digest cannot be tied safely to the authored asset, no declaration match is claimed. |
| 2 | Shape-scoped explicit declaration or validated sidecar selector | Exactly one valid declaration is required. Duplicate IDs, ambiguous Shape matches or conflicts invalidate the match. |
| 3 | Schema, declaration UUID, Profile ID, Role | Validate schema and role vocabulary; unknown version/role or missing profile is invalid. |
| 4 | Profile catalog snapshot | Resolve one Profile ID in one immutable snapshot. Multiple differing definitions at equal priority are a conflict; no arbitrary winner. |
| 5 | Native shader/material evidence | Check expected property/material feature/type, texture availability and required evidence. Mismatch rejects the profile; evidence corroborates the declaration and gates supported variants. |
| 6 | Pass/descriptor/alpha compatibility | Check shader family/stage/descriptor and opaque/masked/transparent route. Unsupported or unknown queue/pass rejects the profile. |
| 7 | Resources/UV/parameters | Verify all required resources, sampler/UV/color-space expectations, ranges and compatibility. Any required failure rejects the Profile. |
| 8 | Publish resolved immutable binding | Return Material Handle + Profile/Role and required consumption contract, or Native. No filesystem or unbounded traversal here. |

## Conflict and fallback cases

| Condition | Outcome | Diagnostic |
|---|---|---|
| No F4SRF declaration | **Native** | `DECLARATION_MISSING` (optional/quiet aggregate) |
| Malformed, unsupported major schema, unknown required role | **Native** | `DECLARATION_INVALID` with bounded reason |
| Duplicate declaration UUID or multiple sidecar matches | **Native for every conflicting match** | `DECLARATION_AMBIGUOUS` |
| Same Profile ID, same priority, different content | **Native for that Profile ID** | `PROFILE_ID_CONFLICT` including catalog/source IDs |
| Profile absent or invalid | **Native** | `PROFILE_MISSING` / `PROFILE_INVALID` |
| Shader material/property evidence contradicts declaration | **Native** | `MATERIAL_EVIDENCE_MISMATCH` |
| Descriptor, alpha mode or queue unsupported | **Native** | `PASS_INCOMPATIBLE` |
| Required texture/UV contract unavailable | **Native** | `RESOURCE_REQUIRED_MISSING` / `UV_CONTRACT_UNAVAILABLE` |
| Optional resource absent with declared safe default | Resolve with that default | `OPTIONAL_RESOURCE_DEFAULTED` (aggregate) |
| Asset override provider or content revision cannot be verified | **Native** | `ASSET_REVISION_UNVERIFIED` |
| Stale handle/snapshot generation or exhausted generation | **Native** | `HANDLE_STALE` / `HANDLE_RETIRED` |
| Transparent material offered to opaque coverage path | **Reject coverage; Native route** | `QUEUE_NOT_OPAQUE` |
| Actor/Head/Replay identity unavailable but static profile is otherwise valid | Resolve only the static profile; dynamic feature is unavailable and must remain Native for that feature | `DYNAMIC_IDENTITY_UNAVAILABLE` |

## Priority policy

Priority is allowed only for explicit, catalog-level override resolution when the override chain is named and validated. It does not break ties between two different declarations attached to one Shape, and it does not silently choose among conflicting providers at equal priority. No “last file loaded wins” or filesystem enumeration order behavior is permitted. If users intend a higher priority, the override must name its target Profile ID and produce a single valid effective definition before snapshot publication.

## Upstream coexistence

Native fallback means Community Shaders continues its existing PBR/SSS/IBL/shadow and deferred behavior. For an opted-in model, the future consumer must explicitly state whether each upstream term is preserved, replaced, or composed. Unknown or double-applied contributions reject that style route; do not globally disable upstream features to make a material appear stylized.
