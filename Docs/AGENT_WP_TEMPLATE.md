# Agent Work-Package Brief Template

Copy this file, fill in the brackets, hand to an agent (Claude Code / Codex CLI / etc.).

---

ROLE: You are implementing a single work package for MANIFOLD. Read /Docs/BUILD_PLAN.md first.

WP: <ID — Title>
GOAL: <one sentence from the Build Plan table>

CONTEXT/INPUTS:
- Interfaces: <headers, DataAsset schemas, events this WP consumes>
- Data files: <paths to correspondence specs, concept bibles, fixtures>
- Dependency outputs: <what prior WPs delivered that this one needs>

CONSTRAINTS:
- Edit ONLY these folders: <owned folders from Build Plan A§7>. Touch nothing else.
- Respect the design pillars (BUILD_PLAN.md §2). Deterministic where the core requires it.
- Lean: smallest correct implementation. No speculative features.

STEPS:
<the brief steps from the WP row>

DELIVERABLE: <the artifact — e.g., C++ module, DataAsset, Niagara system, level, spec doc>

ACCEPTANCE TEST (must pass in CI before you report done):
<the exact command / assertion / hash / fps check from the Build Plan table>

DONE = test green in CI AND no edits outside owned folders. If blocked by a missing interface, write the request to /Docs/interface-requests.md and stop.
