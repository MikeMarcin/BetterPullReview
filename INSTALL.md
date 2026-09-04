# Install the portable BetterPullReview RFC agent set

This archive is a repository-root overlay for BetterPullReview.

1. Extract it at the BetterPullReview repository root, preserving hidden directories.
2. Review and merge `.agents/skills/implement-rfc/config/codex-config.fragment.toml` into `.codex/config.toml` if the repository does not already define the `[agents]` table.
3. Run:

   ```sh
   python3 .agents/skills/implement-rfc/scripts/render_runtime_adapters.py
   python3 .agents/skills/implement-rfc/scripts/validate_portable_agents.py
   ```

4. Run the repository documentation validator after the overlay is committed or combined with other documentation changes:

   ```sh
   python3 scripts/verify_documentation.py --root .
   ```

5. For Claude Code through Amazon Bedrock, merge the `modelOverrides` object from `.agents/skills/implement-rfc/config/claude-bedrock-model-overrides.example.json` into the applicable local or organization-managed Claude Code settings, replacing the example ARNs.

The canonical skill remains `.agents/skills/implement-rfc/SKILL.md`. The `.claude/skills/implement-rfc/SKILL.md` file is a generated entrypoint, not a second maintained copy.

The checked-in quality profile routes utility work and prescribed test execution to `mechanical`, semantic scouting to `balanced`, routine implementation to `standard`, difficult implementation and ordinary review to `deep`, and deeper review of a named material risk to `critical`. The primary agent remains the sole BetterPullReview RFC integrator and tracker/commit/status authority.
