# Contributing to MANIFOLD

## The golden rule

> **Nothing is done because it looks done. It is done when its test is green.**

Every code work package ships with a **mechanically verifiable acceptance test**
that must pass in CI before it merges. No exceptions.

## Workflow

1. **Branch** off `main`: `feat/<area>-<short-desc>`, `fix/<...>`, or `docs/<...>`.
2. **Own your folder.** An owner edits only their folder(s); cross-folder needs go
   through an interface (a header, a DataAsset schema, an event) — never a reach-in.
   See the ownership map in `Docs/BUILD_PLAN.md §7`.
3. **Write the acceptance test** for your work package (see `Tools/CI/run_tests.ps1`
   for the WP→test map). Determinism-critical code must be bitwise-reproducible.
4. **Build + run the full suite locally** (see below) — expect `91 Success, 0 Fail`
   plus your new test.
5. **Open a PR** using the template; link the work package; confirm the acceptance
   test is green.

## Build & test

Close the Unreal Editor first (Live Coding holds a build lock), then:

```powershell
Tools\CI\run_tests.ps1              # build + run all MANIFOLD automation tests
Tools\CI\run_tests.ps1 -WorkPackage S3   # just one WP's tests
```

Or directly:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    MANIFOLDEditor Win64 Development -Project="$PWD\MANIFOLD.uproject" -WaitMutex
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\MANIFOLD.uproject" -ExecCmds="Automation RunTests MANIFOLD; Quit" `
    -unattended -nullrhi -nosplash -nopause -stdout
```

## Conventions

- **Commits:** Conventional Commits — `type(scope): summary`
  (`feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `perf`). Reference the WP id
  in the scope where it applies, e.g. `feat(S3): ...`.
- **C++:** Epic style — tabs, `F`/`U`/`I` prefixes, `MANIFOLD<Module>` module names.
  `UPROPERTY`/`UFUNCTION` reflection only where a real consumer needs it. Blueprint
  cannot see `uint64` — keep 64-bit seeds/hashes C++-only or expose as `int64`.
- **Determinism:** the core is bitwise-reproducible. No `Math::Rand`, no wall-clock,
  no unordered iteration in simulation paths — use `FDeterministicRNG` and fixed steps.
- **Content:** correspondences are data (`Data/Correspondences/*.json`), not code.
- **Binaries:** all `*.uasset` / `*.umap` / art / audio go through **Git LFS**
  (`git lfs install` once per machine; patterns in `.gitattributes`).

## Prerequisites

Unreal Engine 5.8 · Visual Studio 2022 (C++ / MSVC 14.4x) · .NET Framework 4.8 SDK ·
Git LFS.
