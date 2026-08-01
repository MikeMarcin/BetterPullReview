# Install the portable BetterPullReview RFC agent set

This archive is a repository-root overlay for BetterPullReview.

1. Extract it at the BetterPullReview repository root, preserving hidden directories.
2. Review and merge `.agents/skills/bpr-implement-rfc/config/codex-config.fragment.toml` into `.codex/config.toml` if the repository does not already define the `[agents]` table.
3. Run:

   ```sh
   python .agents/skills/bpr-implement-rfc/scripts/render_runtime_adapters.py
   python .agents/skills/bpr-implement-rfc/scripts/validate_portable_agents.py
   ```

4. Run the repository documentation validator after the overlay is committed or combined with other documentation changes:

   ```sh
   python3 scripts/verify_documentation.py --root .
   ```

5. For Claude Code through Amazon Bedrock, merge the `modelOverrides` object from `.agents/skills/bpr-implement-rfc/config/claude-bedrock-model-overrides.example.json` into the applicable local or organization-managed Claude Code settings, replacing the example ARNs.

The canonical skill remains `.agents/skills/bpr-implement-rfc/SKILL.md`. The `.claude/skills/bpr-implement-rfc/SKILL.md` file is a generated entrypoint, not a second maintained copy.

The checked-in quality profile routes mechanically checkable utility work to the lowest-cost model family, semantic scouting to the balanced family, implementation and ordinary review to the deep family, and only capability-critical review to the deep-max family. The primary agent remains the sole BetterPullReview RFC integrator and tracker/commit/status authority.
