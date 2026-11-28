# F4SRF Project Status

Snapshot date: 2026-11-28

## Project and integration baseline

Fallout 4 Stylized Rendering Framework (F4SRF) is an independent Community Shaders fork. It keeps the upstream rendering paths available and adds narrowly opted-in NPR material behavior. The production baseline remains `main` at `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`; PR #1 is merged. The local `main` worktree is stale at `6d768d1233d2471e8712685187f8931ed8c6d8d8` and is five commits behind `origin/main`; it was not changed.

`feature/face-transport-diagnostic` at `2d694c05a6626b7bc223125d5ad2b1d1c8d56f5a` is the most complete **unmerged** production-source candidate for the Face Transport producer and its gated diagnostic. It is three commits ahead of `origin/main` and based directly on the production baseline. It includes the reviewed diagnostic load correction from `b5ad648e3f5d7ec073e5cd77510260861b76210f` plus the follow-up `2d694c05a6626b7bc223125d5ad2b1d1c8d56f5a`. It does not contain Menu Localization, ARCH-01D, Native Biped Probe, Material Registry, or Face SDF consumer code.

FACE-01A is under Draft PR #8 on `feature/face-sdf-directional` at exact HEAD `05524887eac317d03c3e744dbf24dac6c6810d8f`, based on the accepted Face Transport head `2d694c05a6626b7bc223125d5ad2b1d1c8d56f5a` and linked in Stack #9 above PR #4. The feature is default-off and requires explicit Race/Shape/profile/resource compatibility. Focused shader fixes include a pixel-stage light transform, sampler-slot isolation, and non-skinned dismemberment row mapping. CI run `46117273141` passed Build, unit tests, runtime shader compilation, AE/OG feature shader checks, OG identity shaders, package provenance, artifact upload, and Lint. Candidate `v0.5.1-dev.2027` archive SHA-256 is `F0D303B0DE5F4D44497A79EBA8C4C201B9BC9A40164521A43E486001B57B588C`; DLL SHA-256 is `D9A33F44E43A6A04C7D2A3B4C7965A6187D71380A5F5373E1A21AB6D0F4A375A`. The archive contains 216 files and all 216 matched the uploaded payload SHA-256 manifest. In-game visual acceptance remains pending; the candidate was not deployed and the game was not launched.

## Development status

| Area | Research / architecture | Implementation | Validation | Current state |
|---|---|---|---|---|
| Upstream fork and production baseline | Upstream Community Shaders baseline imported with history; F4SRF main established | Fork and baseline CI policy are on main | PR #1 merged; candidate-build infrastructure is a separate remote branch | **Integrated baseline**; production features remain separate branches |
| CI / Candidate Build | Candidate package provenance and release policy documented | `infra/candidate-build` at `09fcbc74af7a9319b1638909b377bded2d175330` | Compare: 0 ahead / 1 behind `main`; merge base equals branch HEAD | **Integrated**; no unmerged work exists at the observed remote HEAD |
| Menu Localization | Localization design and coverage developed independently | `feature/menu-localization` at `b07a1015c0f66567cf230fe4ed0ae5cadbc749e0` | Branch has its own CI/review history | **Open Draft PR #2**; not in Face Transport candidate |
| Localization Coverage | Follow-on coverage work, based on PR #2 exact HEAD | Remote `feature/menu-localization-coverage` at `8d9f4d936cda282831e6fc906dfedda0070b880c`; 7 commits beyond PR #2, 30 files changed, including 631 added `zh_CN.toml` lines | Remote-only branch at checkpoint; no merge or PR state change performed | **Independent localization follow-up**, not in Face Transport candidate |
| MATERIAL-01A S0 | Conditional architecture approval; identity, role/profile, resolver and fallback contracts documented | No production resolver or registry | Static research accepted conditionally | **Architecture only** |
| MATERIAL-01A S1A / S1A-R1 | Asset corpus and BGSM/BGEM evidence accepted; shape-owned carrier and contract corrections remain under review | No production parser or validator | S1A partial; S1A-R1 worktree has nine modified and four untracked files | **Incomplete and dirty locally**; not published in this checkpoint |
| Native Identification | Native provenance research through NATIVE-02C; generic HDPT-to-Geometry mapping not established | No Native Part Resolver | Static evidence only; xEdit record inspection was blocked | **Research partial** |
| ARCH-01D Geometry Ownership | Immediate and CommandBuffer bake paths investigated | Bounded diagnostic probe exists | Remote PR #3 HEAD `1282871993113fac93a8cebc53082913a4f6cd5d`: 4 commits ahead of main, merge base `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`. Local HEAD `a9985c383916841b522fec144b349cd05a4b376b`: 3 commits ahead of main and 1 commit behind the remote PR head | **Diagnostic evidence accepted; not production ownership**; do not update PR #3 here |
| Native Biped Attachment | Attach/segment investigations documented in NATIVE-02A/B/C | `feature/material-01a-native-03a-probe` contains a frozen probe candidate | Runtime probe installation was blocked; no runtime validation | **Frozen / blocked**, not integrated |
| Head Pose / Skinning Palette | ARCH-POSE-01/02A/02B/02C and FACE-BASIS-01 investigate GPU palette, synchronization and draw-local basis | No production pose transport | Capture and static research accepted at their stated scope; CPU-to-actual-draw synchronization remains unproved | **First Face SDF: use a validated draw-local GPU skinning basis. General-purpose Head Pose: CPU bone pose or CPU palette-derived pose remain longer-term candidates. Neither is implemented.** |
| Face Rendering Eligibility | FACE-ELIGIBILITY-01/02A/02B research and capture review | No production eligibility resolver | Sampled Face-feature Draw and PS b2 evidence accepted; exact CPU Bake-to-GPU Draw identity is still unknown | **Research partial** |
| Shared RT6 / Face Transport | FACE-TRANSPORT-01/02A/02B establish resource and publication contracts | `feature/face-transport-diagnostic` adds shared RT6 producer and opt-in synthetic transport diagnostic | Producer accepted in OG runtime using RT6 payload readback and face outline; OFF/ON evidence exists. Strict same-binary A/B provenance and downstream lighting consumption are not established here | **Implemented and producer runtime-validated; consumer is absent** |
| FACE-01A Directional Face SDF | Draw-local GPU skinning basis, explicit compatibility/profile opt-in, per-feature eligibility, and native fallback approved | `feature/face-sdf-directional` at `05524887eac317d03c3e744dbf24dac6c6810d8f`; Draft PR #8 is in Stack #9 above PR #4 | CI run `46117273141` passed Build, unit tests, runtime/AE/OG shader checks, package provenance, artifact upload and Lint; 216/216 archive payload hashes match | **Implemented candidate under review**; default off, no profile/texture bundled, in-game visual acceptance pending; no deployment |
| Toon shading and other NPR roles | No approved implementation stage | None | Not run | **Not started** |

Status labels distinguish research and diagnostics from integrated product functionality. Runtime acceptance of the RT6 producer does not prove that a lighting pass consumes the payload or that Face SDF works.

## Local branch and worktree inventory

All local branches were inspected on 2026-11-28. `ahead` is the number of commits beyond the branch's merge base with `origin/main`; all listed branches have that merge base at `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48`, except `main`, whose local HEAD itself is the merge base. Remote divergence is stated separately.

| Branch | Exact HEAD | Parent | Ahead | Worktree / remote state | Purpose and disposition |
|---|---|---|---:|---|---|
| `main` | `6d768d1233d2471e8712685187f8931ed8c6d8d8` | `e2bf99f632664435d34dd6612cf1f191cadd1405` | 0 | Clean; `origin/main` is five commits ahead | Stale local checkout; production baseline is remote main, do not use this checkout as the current baseline |
| `feature/menu-localization` | `b07a1015c0f66567cf230fe4ed0ae5cadbc749e0` | `e04de4d638031c0865d876d66fdc821ecfecdb26` | 4 | Clean; matches remote | Active independent feature; PR #2 Draft |
| `feature/arch-01d-probe` | `a9985c383916841b522fec144b349cd05a4b376b` | `4db2d8e88e3c6a332b3162755b0f806e69c8282a` | 3 | Clean; remote PR #3 at `1282871993113fac93a8cebc53082913a4f6cd5d` is one commit ahead; merge base is production baseline | Bounded ownership/bake diagnostic; held separate from this sync |
| `feature/face-transport-diagnostic` | `2d694c05a6626b7bc223125d5ad2b1d1c8d56f5a` | `b5ad648e3f5d7ec073e5cd77510260861b76210f` | 3 | Matches remote PR #4 head; one local untracked runtime acceptance report | Accepted producer candidate; no Face SDF consumer |
| `feature/material-01a-native-03a-probe` | `1052ea0d5b59711201d0af9dc421e4c3717f0f2e` | `e74b1c6acf78b6dd4da111ddacdb06b8291deb29` | 9 | Clean; no remote branch | Frozen/blocked Native Biped probe and build evidence; do not publish as a production dependency |
| `docs/project-checkpoint` | `b3726b612e02bc868ee3bdea0879615c6008f742` | `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48` | 3 | PR #7 Draft; status update in progress | Canonical project status index |
| `feature/face-sdf-directional` | `05524887eac317d03c3e744dbf24dac6c6810d8f` | `8641de5a07bb16ac2e6ce7d829734880bdd15bf0` | 12 | Clean; matches remote PR #8 head; base is PR #4; CI run `46117273141` passed | FACE-01A directional Face SDF candidate; visual runtime acceptance pending |
| `docs/material-01a-s0-architecture` | `1b7969cdf259cbb37d7d7fda5895b4d624244908` | `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48` | 1 | Clean; no remote branch | S0 architecture reports; publish for review |
| `docs/material-01a-s1a-validation` | `516a56f1a13d5d494faffe85abb390acc367c826` | `1b7969cdf259cbb37d7d7fda5895b4d624244908` | 2 | 9 modified files and 4 untracked reports | S1A-R1 carrier/identity correction; preserve locally, exclude from this push |
| `docs/material-01a-native-01` | `18319765598fed340c1683ddad4dcb1eba186df7` | `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48` | 1 | Clean; no remote branch | Initial native identification research; superseded by later routes, not selected for publication |
| `docs/material-01a-native-02a` | `578b8bc4e75473a905b49157c8cd72d38dd2e363` | `18319765598fed340c1683ddad4dcb1eba186df7` | 2 | Clean; no remote branch | Native component provenance route; research history |
| `docs/material-01a-native-02b` | `d00ae56b3ba8a83d6870baa2592c9aafbed2fbb4` | `74fd4d855a714709afaede812666c53c68d2ac2e` | 5 | Clean; no remote branch | Route A/B targeted reverse engineering; source evidence retained locally |
| `docs/material-01a-native-02c` | `3dd45414e6b3eb74633018bd43bd2a6dbc980c5d` | `d00ae56b3ba8a83d6870baa2592c9aafbed2fbb4` | 6 | Clean; no remote branch | Segment and dynamic Face trace research; contains local absolute paths, withheld from public sync |
| `docs/face-eligibility-01` | `21abc985d3cf4396493763f00683de832290502a` | `a0aaceaa64a63854c2d7637ff0cc4888d40e9e48` | 1 | Clean; no remote branch | Face eligibility research; publish doc-only branch |
| `docs/face-eligibility-02a` | `ae5a04fa08562a993f1bb8297830d91a53093971` | `e8b78abe523e48e6538c84b3adf65030c02fa060` | 4 | Clean; no remote branch | GPU capture and pose feasibility research; not selected for this synchronization |
| `docs/face-transport-01` | `d5faa07b6553cb9b5ddc3030a49870f37dd27020` | `a4e924a00d1838daa763d04e7f5359d8db0c76bf` | 10 | Clean; no remote branch | Transport architecture evidence includes capture-derived binary data; withheld from public sync |
| `docs/arch-pose-02a` | `08aac95faf7242e213666c0aff623422fd92d569` | `7176eee7ee307a06a876b2a56da3deda0e3d2194` | 8 | Clean; no remote branch | Pose and draw-local basis evidence includes capture-derived binary data; withheld from public sync |

Remote-only branches observed: `docs/legacy-b4-audit` at `99eb6cbe2f7658538fac058077b879b02578281d`, `feature/menu-localization-coverage` at `8d9f4d936cda282831e6fc906dfedda0070b880c`, and `infra/candidate-build` at `09fcbc74af7a9319b1638909b377bded2d175330`. The latter is already represented in merged PR #1 ancestry but has later branch work; no changes were made to these refs.

`infra/candidate-build` compare evidence: 0 commits ahead and 1 behind `main`, with merge base `09fcbc74af7a9319b1638909b377bded2d175330`; its observed HEAD is fully integrated. `feature/menu-localization-coverage` has merge base `b07a1015c0f66567cf230fe4ed0ae5cadbc749e0` and contributes seven additional commits. It remains a separate follow-up; PR #2 was not changed.

## Source integration and architecture debt

- Face Transport is based on production `main`, not on Menu Localization or ARCH-01D. The diagnostic uses the accepted transport publication path and does not implement a general Material Resolver.
- Menu Localization remains a separate feature branch and will require deliberate integration after its review; this checkpoint does not combine it with Face Transport.
- ARCH-01D remains a separate diagnostic PR. Its bake-time ownership samples do not prove replay-draw ownership or provide a production Actor Instance Registry.
- MATERIAL-01A outputs are predominantly documents. S1A-R1 remains dirty and its Shape-owned declaration carrier is not yet accepted. No production Material Profile resolver or registry exists.
- The current approved material direction keeps Native Automatic Adaptation, External Compatibility Profiles, and Explicit NIF Declarations as coexisting identification inputs. Material Identification does not activate a Style; eligibility is feature-specific. These are architectural decisions, not completed resolver functionality.
- Native HeadPart-to-Geometry provenance is not generic or established. The Native Biped probe remains frozen and is not a dependency of Face Transport.
- For the first Face SDF, the approved direction is a validated draw-local GPU skinning basis. General-purpose Head Pose remains a separate longer-term route, with CPU bone pose or CPU palette-derived pose as candidates. Neither requires a general Actor Instance Registry as a prerequisite for the first slice.
- Face eligibility is per feature. Native Identification, Actor ownership, a FACE shader bit, Shape name, or vertex count alone does not opt a draw into NPR. Draw-local basis validity and material/resource compatibility remain separate gates.
- Head pose synchronization and Actor/Geometry instance lifetime remain open for general use. Current RT6 producer success does not close these gaps.
- Face Transport does not yet have a lighting consumer. RT7, `t70`, and the established `PrepassFeatureLane` ABI remain unchanged by the accepted transport contract.
- Some local, unpublished research worktrees contain capture-derived data or local filesystem paths; they are not present in the public repository. They remain local pending a separate publication-safety review.

## Recommended next baseline and route

1. Complete CI and independent review for Draft PR #8; retain PR #4 as the explicit base and do not merge either branch as part of this stage.
2. Keep invalid or unassociated transport publications and incompatible/unopted materials on Native rendering.
3. Integrate Menu Localization and ARCH-01D only through their own reviews; neither is present in the Face Transport base.
4. Continue Material declaration carrier work independently; do not block the first face experiment on a universal HDPT-to-Geometry resolver.

This checkpoint records state and relationships. It does not merge branches, certify all branches as production-ready, or replace their stage-specific acceptance reports.
