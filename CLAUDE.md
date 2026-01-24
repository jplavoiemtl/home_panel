# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working Guidelines

1. **Think first, then read**: Before making any changes, think through the problem and read relevant files in the codebase.

2. **Check in before major changes**: Before making any major changes, check in with the user to verify the plan.

3. **Explain changes at a high level**: At every step, provide a high-level explanation of what changes were made.

4. **Keep it simple**: Make every task and code change as simple as possible. Avoid massive or complex changes. Every change should impact as little code as possible. Simplicity is paramount.

5. **Maintain architecture documentation**: Keep a documentation file that describes how the architecture of the app works inside and out.

6. **Never speculate about unread code**: Never make claims about code you haven't opened. If a specific file is referenced, read it before answering. Investigate and read relevant files BEFORE answering questions about the codebase. Give grounded, hallucination-free answers.

7. **Markdown formatting for documentation**: When creating or editing markdown files (especially plan documents in `doc/`), follow proper formatting to avoid linting warnings:
   - Use only one H1 (`#`) heading per document - use H2 (`##`) for major sections (MD025)
   - Align table columns with pipes matching header width (MD060)
   - Add blank lines before and after lists (MD032)
   - Add blank lines after bold headers before their associated lists
   - Add blank lines before code blocks that follow text
   - Ensure nested lists have proper spacing

8. **No compiler available**: There is no compiler available in this environment. The user compiles the project themselves. Do not attempt to compile or build the project.

9. **Always push commits**: When creating a git commit, always push to the remote repository as well.
