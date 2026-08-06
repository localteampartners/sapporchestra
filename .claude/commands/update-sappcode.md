---
description: Pull the latest sappcode template and merge new files / updated tooling into this project.
---

Update this project to the latest sappcode template. The shell script
handles file-level work (add missing, prompt-overwrite tooling); you
handle the parts that need judgment (CLAUDE.md routing table, new doc
intros).

## Steps

1. **Run the update script** from this project's root.

   The script lives in the user's local sappcode checkout. Default path:
   `~/tools/sappcode/update-project.sh`. Honor `$SAPPCODE_DIR`
   if it's set:

   ```bash
   "${SAPPCODE_DIR:-$HOME/tools/sappcode}/update-project.sh" "$PWD"
   ```

   If the script fails to find sappcode, ask the user for the path —
   don't guess across multiple locations.

2. **Read the script's summary.** It prints:
   - **Added** — template files that didn't exist here before.
   - **Updated tooling** — `.claude/*` files the user accepted overwrites for.
   - **Skipped tooling** — `.claude/*` files the user kept their version of.
   - **Preserved user content** — `_project/*.md`, `CLAUDE.md`, etc., never touched.

3. **Merge `CLAUDE.md` routing-table changes manually.** The script
   never modifies `CLAUDE.md` because it likely has project-specific
   edits. Diff against the new template:

   ```bash
   diff -u CLAUDE.md "${SAPPCODE_DIR:-$HOME/tools/sappcode}/template/CLAUDE.md"
   ```

   For any new rows in the template's "When to read what" or routing
   tables that are missing here, propose them as concrete edits. Apply
   only on the user's approval. Do not auto-merge — the user may have
   intentionally diverged.

4. **Merge `_project/README.md` index changes** the same way:

   ```bash
   diff -u _project/README.md "${SAPPCODE_DIR:-$HOME/tools/sappcode}/template/_project/README.md"
   ```

5. **For each new doc file added** (e.g., `_project/ROADMAP.md` if this
   project was on a pre-roadmap version), tell the user briefly what
   it's for and offer to seed it with project-specific content. Do not
   auto-fill from the placeholders.

6. **Append a CHANGELOG entry.** Read `_project/.sappcode-version` to
   get the SHA the script just stamped, then add to
   `_project/CHANGELOG.md` under "Unreleased":

   ```
   - YYYY-MM-DD — Updated to sappcode @ <sha>
   ```

7. **Don't commit.** Show `git status`, summarize what changed in plain
   English, and let the user review and commit when they're ready.

## Rules

- **Don't touch user-content files** beyond what the script already did.
- **Don't auto-merge `CLAUDE.md`** — propose, get approval, apply.
- **Skip silently** if the script reports "no changes" — say so briefly,
  don't invent work.
