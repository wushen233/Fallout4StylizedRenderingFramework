# SPDX-License-Identifier: GPL-3.0-or-later
# Verify the package produced by the existing upstream xmake packaging rule.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    function Read-Git {
        $value = & git @args
        if ($LASTEXITCODE -ne 0) { throw "git $args failed" }
        return $value
    }

    $source = (Read-Git rev-parse HEAD).Trim()
    $sourceTree = (Read-Git rev-parse 'HEAD^{tree}').Trim()
    foreach ($sha in @($source, $env:GITHUB_SHA)) {
        if ($sha -cnotmatch '^[0-9a-f]{40}$') { throw 'Missing or invalid candidate commit identity' }
    }
    if ($source -cne $env:GITHUB_SHA) { throw 'Checkout does not match the workflow commit' }
    $prHead = $null
    $prBase = $null
    $headTree = $null
    $sameTree = $null
    $publishRequested = $env:PUBLISH_REQUESTED -eq 'true'
    switch ($env:GITHUB_EVENT_NAME) {
        'pull_request' {
            if ($publishRequested) { throw 'PR cannot publish' }
            $checkoutKind = 'pull_request_merge'
            $prHead = $env:PR_HEAD_SHA
            $prBase = $env:PR_BASE_SHA
            foreach ($sha in @($prHead, $prBase)) {
                if ($sha -cnotmatch '^[0-9a-f]{40}$') { throw 'Invalid PR commit identity' }
            }
            $parents = (Read-Git rev-list --parents -n 1 HEAD).Trim() -split ' '
            if ($parents.Count -ne 3 -or $parents[1] -cne $prBase -or $parents[2] -cne $prHead) {
                throw 'Checkout is not the expected PR merge commit'
            }
            $headTree = (Read-Git rev-parse "$prHead^{tree}").Trim()
            $sameTree = $sourceTree -ceq $headTree
        }
        'push' {
            if ($env:GITHUB_REF -ne 'refs/heads/main' -or $publishRequested) {
                throw 'Only non-publishing main pushes are accepted'
            }
            $checkoutKind = 'main_push'
        }
        'workflow_dispatch' {
            if ($publishRequested -and $env:GITHUB_REF -ne 'refs/heads/main') {
                throw 'Publication requires main'
            }
            $checkoutKind = 'manual_dispatch'
        }
        default { throw 'Unsupported candidate event' }
    }
    if ($publishRequested) {
        if ($env:RELEASE_IDENTITY -cnotmatch '^v[0-9]+\.[0-9]+\.[0-9]+$') {
            throw 'Publication requires a stable identity'
        }
    } elseif ($env:RELEASE_IDENTITY -cnotmatch '^v[0-9]+\.[0-9]+\.[0-9]+-dev\.[0-9]+$') {
        throw 'Candidate requires a development identity'
    }
    $archives = @(Get-ChildItem -LiteralPath 'build/packages' -File -Filter '*.zip')
    if ($archives.Count -ne 1) { throw "Expected exactly one package, found $($archives.Count)" }
    $archive = $archives[0]
    $expectedName = "FO4CommunityShaders-$($env:RELEASE_IDENTITY).zip"
    if ($archive.Name -ne $expectedName) { throw 'Package name disagrees with configured release identity' }
    $dllPath = 'build/windows/x64/releasedbg/FO4CommunityShaders.dll'
    $dll = Get-Item -LiteralPath $dllPath
    $dllHash = (Get-FileHash -LiteralPath $dll.FullName -Algorithm SHA256).Hash
    $dllBytes = [System.IO.File]::ReadAllBytes($dll.FullName)
    $dllText = [System.Text.Encoding]::ASCII.GetString($dllBytes)
    if (-not $dllText.Contains($source) -or -not $dllText.Contains($env:RELEASE_IDENTITY)) {
        throw 'Built DLL does not contain the expected commit and build identity'
    }

    $entries = [System.Collections.Generic.Dictionary[string,string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $zip = [System.IO.Compression.ZipFile]::OpenRead($archive.FullName)
    try {
        foreach ($entry in $zip.Entries) {
            if ([string]::IsNullOrEmpty($entry.Name)) { continue }
            $name = $entry.FullName.Replace('\', '/')
            if ($name -match '(^/|:|(^|/)\.\.(/|$)|[\r\n])') { throw "Invalid archive entry: $name" }
            if ($entries.ContainsKey($name)) { throw "Duplicate archive entry: $name" }
            $stream = $entry.Open()
            try {
                $hash = (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash
            } finally {
                $stream.Dispose()
            }
            $entries.Add($name, $hash)
        }
        foreach ($name in @('F4SE/Plugins/FO4CommunityShaders.dll', 'Shaders/DFPrepass.hlsl', 'Shaders/DFLight.hlsl', 'Shaders/DFComposite.hlsl')) {
            if (-not $entries.ContainsKey($name)) { throw "Missing required archive entry: $name" }
        }
        if ($entries['F4SE/Plugins/FO4CommunityShaders.dll'] -ne $dllHash) {
            throw 'Packaged DLL differs from build output'
        }
    } finally {
        $zip.Dispose()
    }

    $evidence = 'build/candidate-evidence'
    New-Item -ItemType Directory -Path $evidence -Force | Out-Null
    $archiveHash = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash
    "$archiveHash  $($archive.Name)" | Set-Content -LiteralPath "$evidence/SHA256SUMS.txt" -Encoding utf8NoBOM
    $entries.Keys | Sort-Object | ForEach-Object { "$($entries[$_])  $_" } |
        Set-Content -LiteralPath "$evidence/PAYLOAD-SHA256SUMS.txt" -Encoding utf8NoBOM
    $xmakeVersion = & xmake --version
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record xmake version' }
    $metadata = [ordered]@{
        schema_version = 2
        repository = $env:GITHUB_REPOSITORY
        workflow_run_id = $env:GITHUB_RUN_ID
        workflow_run_attempt = $env:GITHUB_RUN_ATTEMPT
        workflow_url = "$($env:GITHUB_SERVER_URL)/$($env:GITHUB_REPOSITORY)/actions/runs/$($env:GITHUB_RUN_ID)"
        workflow_event = $env:GITHUB_EVENT_NAME
        source_ref = $env:GITHUB_REF
        publish_requested = $publishRequested
        checkout_kind = $checkoutKind
        source_commit = $source
        source_tree = $sourceTree
        pr_head = $prHead
        pr_base = $prBase
        pr_head_tree = $headTree
        source_tree_equals_pr_head = $sameTree
        build_identity = $env:RELEASE_IDENTITY
        build_mode = 'releasedbg'
        xmake_version = ($xmakeVersion -join "`n")
        compiler_version = 'See Configure/Build logs; UNKNOWN if not reported'
        runner_os = $env:RUNNER_OS
        runner_arch = $env:RUNNER_ARCH
        runner_image = $env:ImageVersion
        submodules = @(Read-Git submodule status --recursive)
        archive = $archive.Name
        archive_bytes = $archive.Length
        archive_sha256 = $archiveHash
        dll_sha256 = $dllHash
        payload_file_count = $entries.Count
        unit_step = 'passed'
        shader_step = 'passed'
        lint = 'See separate Lint job; require full workflow success before deployment'
        generated_utc = [DateTime]::UtcNow.ToString('o')
    }
    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$evidence/BUILD-METADATA.json" -Encoding utf8NoBOM
    Write-Host "Verified candidate $source ($($entries.Count) files), SHA256 $archiveHash"
} finally {
    Pop-Location
}
