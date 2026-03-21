# Git LFS Cleanup (Targeted Content Exclusions)

This repository currently has significant LFS usage in a set of large scene/content folders.

## Current Audit Snapshot (2026-03-21)

- LFS history footprint: ~24,347 MB
- Estimated LFS removable from requested folders: ~16,375 MB
- Estimated post-cleanup LFS history footprint: ~7,972 MB

## Validated Rewrite Command (PowerShell)

Run from repository root:

```powershell
$pathsToDrop = @(
  'Content/Scene_Junkyard/',
  'Content/Scene_Trash/',
  'Content/OLDSandbox/',
  'Content/NiagaraExamples/',
  'Content/Mining_pack/',
  'Content/MiningPack/',
  'Content/Scene_UnfinishedBuilding/',
  'Content/Scene_Warehouse/',
  'Content/BigNiagaraBundle/'
)

$args = @(
  '-m','git_filter_repo',
  '--force',
  '--invert-paths'
)

foreach ($path in $pathsToDrop) {
  $args += @('--path', $path)
}

python @args
```

Notes:
- This rewrites history.
- `git filter-repo` removes `origin`; add it back before pushing:

```powershell
git remote add origin https://github.com/Girgis1/Agent.git
git push --force origin main
```

## Important GitHub LFS Quota Note

GitHub documents that removed LFS objects can still count against LFS quota unless the repository is deleted/recreated (or support purges objects). See:

- https://docs.github.com/en/repositories/working-with-files/managing-large-files/removing-files-from-git-large-file-storage
