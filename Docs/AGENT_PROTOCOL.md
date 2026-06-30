# MANIFOLD — Agent Workspace Protocol

**Version:** 1.0  
**Status:** Active  
**Owner:** You (Human Lead)  
**Applies to:** All agents (Claude Code, Codex CLI, OpenCode, human specialists)

---

## 1. Purpose

This protocol enables **safe parallel execution** of Work Packages (WPs) by multiple agents/people. It enforces the folder ownership boundaries defined in `BUILD_PLAN.md §7` and the CI merge gate from `§11`. No agent may merge until its acceptance test passes in CI.

---

## 2. Ownership Map (Non-Negotiable)

| Agent / Role | Owns (Read/Write) | Never Touches |
|--------------|-------------------|---------------|
| **Agent-Sys** (Systems) | `Source/Core/*`, `Source/Kernels/*`, `Source/Correspond/*`, `Source/Telemetry/*` | `Content/*`, `Data/*` |
| **Agent-VFX** | `Content/VFX/*` | `Source/*`, `Content/Realms/*` |
| **Agent-Infra** | `Tools/CI/*`, `.github/workflows/*` | `Source/*`, `Content/*` |
| **Human Lead** (You) | `Data/Correspondences/*`, `Content/Realms/*`, `Content/Audio/*`, `Docs/*` | — |
| **Specialist** (later) | By agreement | — |

**Cross-boundary rule:** If Agent-Sys needs a DataAsset schema from Design, Agent-Sys writes an interface request to `Docs/interface-requests.md`. Human Lead approves. Both sides then code to the agreed interface.

---

## 3. Agent Work-Package Lifecycle

### 3.1 Handoff (Human → Agent)
1. Human copies `Docs/AGENT_WP_TEMPLATE.md` → fills in WP ID, goal, context, constraints, steps, acceptance test.
2. Human creates a **feature branch**: `wp/<ID>-<short-title>` (e.g., `wp/S3-orbits-kernel`).
3. Human pushes branch + opens Draft PR titled `[WP S3] Orbits Kernel`.
4. Human assigns agent (via comment: `@claude-code implement this WP`).

### 3.2 Execution (Agent)
1. Agent reads: `BUILD_PLAN.md`, `TOOLING_BIBLE.md`, `AGENT_PROTOCOL.md`, the WP brief, and any dependency outputs.
2. Agent **edits ONLY its owned folders**.
3. Agent writes implementation + unit/integration tests matching the acceptance test.
4. Agent runs local verification: `.\Tools\CI\run_tests.ps1 -WorkPackage <ID>`
5. Agent pushes commits to the feature branch.

### 3.3 Verification (CI Gate)
1. CI runs on PR push (GitHub Actions `ue-tests` job on self-hosted UE5 runner).
2. **Gate:** All tests for the WP must pass. No exceptions.
3. If tests fail: Agent iterates on the same branch until green.
4. If blocked by missing interface: Agent writes to `Docs/interface-requests.md` and stops. Human resolves.

### 3.4 Merge (Human)
1. Human reviews: green CI, no edits outside owned folders, acceptance test matches WP table.
2. Human merges via **squash merge** (clean history).
3. Branch deleted. Next WP can start (per dependency graph).

---

## 4. Integration Cadence

| Cadence | Purpose |
|---------|---------|
| **Per WP** | Each WP merges independently when its test is green. |
| **Daily (auto)** | `nightly` workflow runs full slice + perf baseline. Catches integration drift early. |
| **Weekly (human)** | Human reviews `nightly` results, updates `BUILD_PLAN.md` if scope shifts, approves next wave of WPs. |

**No long-lived integration branches.** The `main` branch is always the integrated, tested state.

---

## 5. Communication Contract

### 5.1 Agent → Human (via PR comments)
- **Progress:** "S3: Keplerian solver done, resonance query 80% — pushing WIP commit"
- **Blocked:** "S5: Need `FStructuralMapping` DataAsset schema — added to interface-requests.md"
- **Done:** "S3: All acceptance tests pass locally. Ready for CI."

### 5.2 Human → Agent (via PR review / issue)
- **Approve interface:** "Interface-requests.md #42 APPROVED — schema added to `Source/Correspond/Public/CorrespondenceTypes.h`"
- **Scope correction:** "S3: Use double precision for semi-major axis (not float) — per TOOLING_BIBLE §2 determinism"
- **Stop work:** "S7: Deferred to Phase 2 — slice doesn't need lazy realization yet"

### 5.3 Agent ↔ Agent (never direct)
All cross-agent needs go through **Human Lead + interface-requests.md**. Agents do not DM each other.

---

## 6. Branching & Naming Conventions

| Type | Pattern | Example |
|------|---------|---------|
| Feature (WP) | `wp/<ID>-<kebab-title>` | `wp/S3-orbits-kernel` |
| Hotfix | `fix/<ID>-<issue>` | `fix/S2-rng-replay-desync` |
| Docs | `docs/<topic>` | `docs/concept-bible-orbits` |
| CI | `ci/<description>` | `ci/add-perf-benchmarks` |

**Commit messages:** Conventional Commits
```
feat(S3): add Keplerian n-body solver with resonance ratios
test(S3): add stable-orbits + resonance-query fixtures
fix(S2): correct RNG state serialization for replay
```

---

## 7. Tooling Requirements for Agents

| Tool | Version | Purpose |
|------|---------|---------|
| **Claude Code** | Latest (Opus 4.8 / Sonnet 4.6) | Primary coding agent |
| **Git** | 2.45+ | Version control |
| **PowerShell** | 7.x | Run `run_tests.ps1` |
| **UE5 Editor** | 5.8+ | Only for human Lead; agents run headless via CI |
| **dotnet** | 8.0+ | UnrealBuildTool |

Agents **do not need UE5 installed locally** — they write code, push, CI validates.

---

## 8. Security & Hygiene

- No secrets in repo. UE5 runner credentials via GitHub Environments / runner config.
- Agents run with **read-only** access to `main` — only push to their feature branches.
- Human Lead holds merge rights to `main` and `develop`.
- Large assets (Megascans, AI-generated props) via Git LFS — tracked in `.gitattributes`.

---

## 9. Escalation Path

| Issue | Resolution |
|-------|------------|
| Test flakiness | Human Lead investigates, marks test `@flaky` in `BUILD_PLAN.md`, fixes root cause in next WP |
| Interface dispute | Human Lead decides, documents in `interface-requests.md` with APPROVED status |
| Scope creep | Human Lead rejects — "Not in vertical slice. Phase 2+" |
| Agent hallucination | Human Lead reverts commit, clarifies WP brief, re-assigns |

---

## 10. Quick Reference Card

```
HUMAN:  cp Docs/AGENT_WP_TEMPLATE.md → wp/S3-brief.md → fill → git push wp/S3-orbits-kernel
AGENT:  read BUILD_PLAN.md + brief → edit Source/Kernels/Orbits/* → test locally → push
CI:     runs on self-hosted UE5 → all S3 tests green?
HUMAN:  review PR → squash merge → delete branch
NEXT:   S4 (parallel) or S5 (depends on S3,S4)
```

---

**Remember:** The CI gate is the only definition of done. "It compiles" ≠ done. "It looks right" ≠ done. **Test green in CI = done.**