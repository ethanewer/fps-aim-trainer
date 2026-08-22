---
name: external-code-review
description: "Run an external code review over uncommitted/unstaged changes. Always run external code review before committing to verify your work."
---

# External code review (Cursor bugbot via CLI)

Runs Cursor's **bugbot** review subagent over uncommitted/unstaged changes. It reports findings only; it never edits files.

## Invoke

`agent` isn't on pi's bash PATH; use the version-resolved entrypoint:

```bash
CUR="C:/Users/Ethan/AppData/Local/cursor-agent"
VER="$(ls -d "$CUR/versions"/*/ 2>/dev/null | sort -r | head -1)"

```

Run the review from the repo root:

```bash
"$VER/node.exe" "$VER/index.js" agent -p -f --trust --workspace "<repo-root>" \
  "Use the review-bugbot skill. Launch one bugbot subagent (run_in_background=false).
   Full Repository Path: <repo-root>
   Diff: uncommitted changes
   The subagent computes the diff itself — do not precompute it. Report findings as a Severity | file:line | Finding table, sorted by severity (high first). Do not fix anything."
```

- `-f` pre-approves shell/git calls; drop it to prompt per call. `--trust` skips the workspace prompt.
- `Diff: uncommitted changes` reviews only dirty working-tree edits; use `branch changes` for committed+staged+unstaged vs the base branch.

## Expected results

- Empty diff → "nothing to review".
- No issues → one line, e.g. "Bugbot found no bugs".
- Findings → a `Severity | file:line | Finding` table (high first).

Treat it as advisory, not truth: agree with and verify each finding before acting; it does not auto-fix.