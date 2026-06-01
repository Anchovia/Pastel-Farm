# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

## 5. Code Style

- All comments must be written in **English**.

## 6. Before You Start

**Summarize, confirm, then act.**

- Before starting any task, summarize the plan and **ask for explicit approval** before proceeding.
- If you haven't seen the relevant source files, **ask before implementing**. Don't guess at existing code structure.
- Before proposing structural changes, check `ARCHITECTURE.md`. Decisions already marked as resolved should not be revisited without discussion.
- Do not create new files or add dependencies without explicit approval.

## 7. Build & Workflow

**The user owns the build. You own the handoff.**

- **Never run the build yourself.** The user handles all builds.
- After implementation, list what the user should verify in the build result (behavior, visuals, errors, edge cases).
- After implementation, list what **existing functionality might be affected or broken**.
- After build verification is confirmed, update relevant docs as appropriate: `README`, `DEVLOG`, `DESIGN`, `ARCHITECTURE`.

## 8. Refactoring

- Do not mix refactoring with feature implementation.
- If refactoring is needed, **propose it as a separate task** after the current task is complete.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.