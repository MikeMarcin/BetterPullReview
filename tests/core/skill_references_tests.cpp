// [[BPR-1916]]. `/skill-name` references in a prompt, without Qt: where they
// are, which skill each names (personal over repository unless `/repo:`), the
// quiet note for unknown names, and completion with each skill's source.
#include "bpr/core/review_skill.hpp"
#include "bpr/core/skill_references.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto catalog() -> std::vector<skill_entry>
	{
		return merge_skill_catalog(
			{
				{ .Name = "technical-book-humanizer", .Description = "Rewrite technical prose.", .Source = skill_source::personal, .Origin = "~/.claude/skills" },
				{ .Name = "shared-voice", .Description = "Personal voice.", .Source = skill_source::personal, .Origin = "~/.claude/skills" },
				{ .Name = "Bad_Name", .Source = skill_source::personal, .Origin = "~/.claude/skills" },
			},
			{
				{ .Name = "shared-voice", .Description = "Team voice.", .Source = skill_source::repository, .Origin = ".claude/skills at base 1a2b3c4" },
				{ .Name = "technical-writing", .Description = "Team technical writing.", .Source = skill_source::repository, .Origin = ".agents/skills at base 1a2b3c4" },
			});
	}

	void names()
	{
		expect(valid_skill_name("technical-book-humanizer") && valid_skill_name("a1"), "lowercase, digits and hyphens are names");
		expect(!valid_skill_name("") && !valid_skill_name("-x") && !valid_skill_name("Upper") && !valid_skill_name("a_b") && !valid_skill_name(std::string(65U, 'a')), "anything else is not");
		const auto merged = catalog();
		expect(merged.size() == 4U && merged[0].Source == skill_source::personal && merged[0].Name == "shared-voice" && merged[2].Source == skill_source::repository,
			   "the catalog is personal then repository, each by name, invalid names dropped");
	}

	void references()
	{
		const auto found = find_skill_references("/technical-book-humanizer, then (/repo:shared-voice) and \"/personal:shared-voice\".");
		expect(found.size() == 3U, "three references");
		expect(found[0].Begin == 0U && found[0].Name == "technical-book-humanizer" && !found[0].Qualifier, "a reference at the start");
		expect(found[1].Name == "shared-voice" && found[1].Qualifier == skill_source::repository, "repo: picks the repository");
		expect(found[2].Qualifier == skill_source::personal, "personal: picks the person's");
		for (const auto text : { "/usr/bin", "a/b", "/name.md", "https://example.invalid/x", "/Upper", "x/name", "/name/", "/name_x", "/name@2" }) {
			expect(find_skill_references(text).empty(), std::string{ "not a reference: " } + text);
		}
		expect(find_skill_references("End with /shared-voice.").size() == 1U, "a sentence's full stop ends a reference");
	}

	void resolution()
	{
		const auto merged = catalog();
		const auto texts  = std::array<std::string_view, 2>{ "Use /shared-voice and /technical-writing.", "Also /repo:shared-voice, /shared-voice again and /unknown-skill." };
		const auto result = resolve_skill_references(texts, merged);
		expect(result.Resolved.size() == 3U, "three distinct skills");
		expect(merged[result.Resolved[0]].Source == skill_source::personal && merged[result.Resolved[0]].Name == "shared-voice", "a personal skill wins a shared name");
		expect(merged[result.Resolved[1]].Name == "technical-writing" && merged[result.Resolved[1]].Source == skill_source::repository, "a repository-only name is the repository's");
		expect(merged[result.Resolved[2]].Source == skill_source::repository && merged[result.Resolved[2]].Name == "shared-voice", "repo: picks the repository's over the person's");
		expect(result.Unknown == std::vector<std::string>{ "unknown-skill" }, "an unknown name is kept as text");
		expect(skill_resolution_note(result) == std::optional<std::string>{ "/unknown-skill matched no skill, so it was sent as text." }, "with a quiet note");
		expect(!skill_resolution_note(resolve_skill_references(std::array<std::string_view, 1>{ "/shared-voice" }, merged)), "no note when every name matched");
		auto many = std::vector<skill_entry>{ };
		for (const auto* name : { "a", "b", "c", "d", "e" }) {
			many.push_back({ .Name = name, .Source = skill_source::personal, .Origin = "~/.claude/skills" });
		}
		const auto bounded = resolve_skill_references(std::array<std::string_view, 1>{ "/a /b /c /d /e" }, merge_skill_catalog(many, { }));
		expect(bounded.Resolved.size() == maximum_referenced_skills && bounded.Dropped == 1U, "at most four skills, the rest counted");
	}

	void completion()
	{
		const auto merged = catalog();
		const auto at     = skill_completion_at("Rewrite with /sha", 17U);
		expect(at && at->Begin == 13U && at->Prefix == "sha", "the reference being typed");
		expect(!skill_completion_at("a/sha", 5U) && !skill_completion_at("plain", 5U), "not inside a word or without a slash");
		const auto rows = complete_skill_references("sha", merged);
		expect(rows.size() == 2U && merged[rows[0].Entry].Source == skill_source::personal && rows[0].Insert == "shared-voice", "the personal one first, inserted bare");
		expect(rows[1].Shadowed && rows[1].Insert == "repo:shared-voice" && merged[rows[1].Entry].Origin == ".claude/skills at base 1a2b3c4", "the hidden repository one is inserted with repo:, with its source");
		const auto all = complete_skill_references("", merged);
		expect(all.size() == 4U && all[0].Insert == "shared-voice" && all[2].Insert == "technical-book-humanizer", "an empty prefix lists every skill by name");
		const auto qualified = complete_skill_references("repo:t", merged);
		expect(qualified.size() == 1U && qualified[0].Insert == "repo:technical-writing", "a qualifier lists only its source");
	}

	void front_matter()
	{
		const auto facts = read_skill_front_matter("---\nname: fixture-voice\ndescription: \"A fixture, quoted.\"\nmetadata:\n  x: y\n---\nBody\n");
		expect(facts && facts->Name == "fixture-voice" && facts->Description == "A fixture, quoted.", "name and description");
		expect(!read_skill_front_matter("no front matter") && !read_skill_front_matter("---\ndescription: x\n---\n") && !read_skill_front_matter("---\nname: x\n"), "no name or no closed block is no skill");
	}
}

auto main() -> int
{
	names();
	references();
	resolution();
	completion();
	front_matter();
	std::cout << "skill-references:pass\n";
	return EXIT_SUCCESS;
}
