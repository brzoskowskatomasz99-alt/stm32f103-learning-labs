<#
git_publish.ps1 - import a project tree as a folder into the remote main
of a labs-style repo, or update existing files, with fast-forward push.
(P4 plugin core, ASCII-only source to avoid encoding issues)

Modes:
  A) Full import (default): export a commit tree of the current repo into
     a NEW folder on origin/main, commit, fast-forward push.
     .\tools\git_publish.ps1 -Folder 06-smart-agri-lora-mqtt -Message "Add lab 06"
  B) Incremental: sync specified working-tree files into an EXISTING folder.
     .\tools\git_publish.ps1 -Folder 06-smart-agri-lora-mqtt `
         -Files ACCEPTANCE-M3-GATEWAY-20260815.md -Message "close T06"

Params:
  -Repo        source repo path (default current dir)
  -SrcCommit   commit to export (default HEAD)
  -Folder      target folder name (required)
  -Files       incremental file list relative to repo root (empty = full import)
  -Message     commit message
  -PushBranch  remote branch to push (default main)
  -NoPush      commit only, no push
  -DryRun      rehearsal: no fetch, no push

Safety gate: exported tree must not contain secrets.h / *.bin / *.hex / *.axf / *.map.
This script NEVER force-pushes; a rejected (non-fast-forward) push aborts.
Exit: 0 ok, 1 fail
#>
param(
    [string]$Repo = (Get-Location).Path,
    [string]$SrcCommit = "HEAD",
    [string]$Folder = "",
    [string[]]$Files = @(),
    [string]$Message = "",
    [string]$PushBranch = "main",
    [switch]$NoPush,
    [switch]$DryRun
)

# 显式 Continue: 防止调用方 EAP=Stop 时 PS5.1 把 git stderr 变成终止错误;
# 本脚本所有失败路径都有显式 $LASTEXITCODE 检查。
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false
$forbidden = 'secrets\.h$', '\.bin$', '\.hex$', '\.axf$', '\.map$'

function Fail([string]$msg) { Write-Output "[PUBLISH] ABORT: $msg"; exit 1 }

function Remove-PublishWorktree([string]$wtPath) {
    # git worktree list 输出用正斜杠, 必须规范化后比较(不区分大小写)
    $target = [IO.Path]::GetFullPath($wtPath)
    $entries = git worktree list --porcelain 2>$null
    foreach ($line in $entries) {
        if ($line -like "worktree *") {
            $p = [IO.Path]::GetFullPath($line.Substring(9).Trim())
            if ($p -ieq $target) {
                git worktree remove --force $wtPath *> $null
                break
            }
        }
    }
    # 清理"目录已消失但仍注册"的残留
    git worktree prune *> $null
}

if (-not $Folder) { Fail "-Folder is required" }
if (-not (Test-Path $Repo)) { Fail "repo not found: $Repo" }
Push-Location $Repo
try { $srcCommitSha = git rev-parse $SrcCommit } catch { Fail "commit not found: $SrcCommit" }

if (-not $DryRun) {
    git fetch origin *> $null
    if ($LASTEXITCODE -ne 0) { Fail "git fetch failed" }
}
$remoteSha = git rev-parse "origin/$PushBranch"
if ($LASTEXITCODE -ne 0) { Fail "remote branch origin/$PushBranch not found" }
Write-Output "[PUBLISH] origin/$PushBranch = $remoteSha"

$wt = Join-Path $env:TEMP "dsh_publish_wt"
$tar = Join-Path $env:TEMP "dsh_publish_export.tar"
Remove-PublishWorktree $wt
Remove-Item $wt, $tar -Force -Recurse -ErrorAction SilentlyContinue
git worktree add -q --detach $wt "origin/$PushBranch"
if ($LASTEXITCODE -ne 0) { Fail "worktree add failed" }

try {
    if ($Files.Count -gt 0) {
        Write-Output "[PUBLISH] incremental mode: $($Files -join ', ')"
        foreach ($f in $Files) {
            if (-not (Test-Path (Join-Path $Repo $f))) { Fail "file missing: $f" }
            $dst = Join-Path $wt (Join-Path $Folder $f)
            $dstDir = Split-Path $dst -Parent
            New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
            Copy-Item -Force (Join-Path $Repo $f) $dst
        }
    } else {
        Write-Output "[PUBLISH] full import: $SrcCommit -> $Folder/"
        git archive --format=tar --output=$tar $SrcCommit
        if ($LASTEXITCODE -ne 0) { Fail "git archive failed" }
        $dstDir = Join-Path $wt $Folder
        New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
        tar -xf $tar -C $dstDir
        if ($LASTEXITCODE -ne 0) { Fail "tar extract failed" }
        $count = (Get-ChildItem $dstDir -Recurse -File | Measure-Object).Count
        if ($count -eq 0) { Fail "export empty" }
        Write-Output "[PUBLISH] exported $count files"
    }

    $hits = Get-ChildItem (Join-Path $wt $Folder) -Recurse -File |
        Where-Object { $_.Name -match ($forbidden -join '|') }
    if ($hits) { Fail "forbidden files: $($hits.Name -join ', ')" }

    git -C $wt add -A
    if ($LASTEXITCODE -ne 0) { Fail "git add failed" }
    if (-not $Message) {
        $Message = if ($Files.Count -gt 0) {
            "Update $Folder ($($Files -join ', '))"
        } else {
            "Add $Folder (import from $($srcCommitSha.Substring(0,7)))"
        }
    }
    git -C $wt commit -q -m $Message
    if ($LASTEXITCODE -ne 0) { Fail "commit failed (no changes?)" }
    $newSha = git -C $wt rev-parse HEAD
    Write-Output "[PUBLISH] COMMIT $newSha $Message"

    if ($NoPush) { Write-Output "[PUBLISH] DONE (not pushed, worktree: $wt)"; exit 0 }
    if ($DryRun) { Write-Output "[PUBLISH] DRY-RUN done, not pushed"; exit 0 }

    git -C $wt push origin "HEAD:$PushBranch"
    if ($LASTEXITCODE -ne 0) {
        Fail "push rejected (non-fast-forward). Pull first; never force."
    }
    Write-Output "[PUBLISH] PUSHED origin/$PushBranch = $newSha"
} finally {
    Remove-PublishWorktree $wt
    Remove-Item $wt -Force -Recurse -ErrorAction SilentlyContinue
    Remove-Item $tar -Force -ErrorAction SilentlyContinue
    Pop-Location
}
Write-Output "[PUBLISH] OK"
