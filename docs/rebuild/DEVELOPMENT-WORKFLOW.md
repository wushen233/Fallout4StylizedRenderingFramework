# F4SRF development workflow — cloud CI and local validation

Status: candidate infrastructure proposed; validate first successful CI/artifact before calling it operational.

## Responsibilities
ChatGPT or Codex implements on isolated branches; the non-author reviews. GitHub Actions performs Windows ReleaseDbg, unit, stock/feature shader tests and packaging. Codex handles local tools, asset authoring, reverse engineering, artifact verification, MO2 and authorized game evidence. User owns visual approval and merge/deployment authorization. No automatic merge, deployment, game launch, or public release.

## Candidate builds
Open an internal Draft PR against main. The existing upstream CI performs Lint and Windows Release, retaining the complete original unit and shader suites. Reuse the same Windows build for candidate packaging, instead of duplicating expensive compilation in a second workflow. The upstream package command validates the staged archive. Internal PRs additionally verify the archive, build identity, DLL equality, payload paths and hashes; upload the package and evidence for 14 days. No release publication on PRs. Existing main/release behavior is unchanged by this change; manual upstream dispatch from main is a release operation, not a candidate operation.

GitHub pull_request builds normally test a synthetic merge commit. The candidate evidence records its exact SHA and tree, PR head/base, and whether the tested tree equals the PR head tree. Never label a merge SHA as a head SHA. If the PR head/base changes, obtain a fresh run; if the tested tree differs from the intended deployment source, review the integration or rebase before deployment. Compare actual DLL startup commit with source_commit, not the PR head unless equal. Archive name alone is not provenance.

Download the Actions artifact (outer ZIP) and extract it; the inner FO4CommunityShaders archive is the MO2 package. Verify SHA256SUMS.txt for the inner ZIP and PAYLOAD-SHA256SUMS.txt for extracted files; BUILD-METADATA.json binds the evidence to the run. Require overall CI success, independent source review and explicit deployment authorization. The Windows artifact can exist when the separate Lint job failed; it is not a passed candidate in that case. Preserve prior rollback installation.

## CI scope and cost
Start with existing full suites. Record actual elapsed runner time, test outcomes and cache behavior before proposing fast/candidate/full tiers. No invented timeout/version gate or blanket skipped tests. Hosted Windows runners consume private-repo allowance; the account owner must check billing/limits before repeated expensive runs. No extra automatic runs just to poll status. A future separate workflow is optional, not required; prioritize one build per revision. Self-hosted runners require a separate trust/isolation review.

## Evidence and baseline
First validate a production-source-equivalent upstream candidate. Lint correction in CI-01 is intended only to sort the two adjacent include directives already flagged in the upstream baseline; the complete lint/build/shader matrix must establish whether it is sufficient. Do not treat static edits as passed tests. Compiler version must be taken from actual logs if available; unknown otherwise. Do not claim runtime GPU/visual results from CI. No NPR production implementation in CI-01.
