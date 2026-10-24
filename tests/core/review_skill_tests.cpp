/// \file
/// \brief [[BPR-1752]], RFC-BPR-0092 owner decision D1 and answer Q9 A.
/// The review skill's front matter, the offer of a newer built-in version, and
/// the review brief's bounds, without any file but the shipped skill.

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/review_skill.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto skill(const std::string_view aFrontMatter, const std::string_view aBody = "# Review\n") -> std::string
	{
		return "---\n" + std::string{ aFrontMatter } + "---\n\n" + std::string{ aBody };
	}

	void test_the_shipped_skill_is_a_review_skill()
	{
		std::ifstream input{ BPR_TEST_REVIEW_SKILL, std::ios::binary };
		const auto    text  = std::string{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
		const auto    facts = core::inspect_review_skill(text);
		expect(!text.empty() && facts.has_value() && facts->Version == 2U && facts->Description.starts_with("Review one GitHub pull request"),
			   "the shipped bpr-pr-review is a review skill of version 2");
		// [[BPR-1753]], owner decision (a) 2026-10-23: every review
		// prompt answers with a summary, even with no finding; and decision
		// (b): the conversation so far is the agent's to continue.
		expect(text.find("brief.conversation") != std::string::npos && text.find("summary") != std::string::npos,
			   "the shipped skill reads the conversation and always ends with a summary");
		expect(text.find(")BPR\"") == std::string::npos, "the shipped text can be embedded as a raw string");
	}

	void test_front_matter()
	{
		const auto plain = core::inspect_review_skill(skill("name: bpr-pr-review\ndescription: Reviews.\nmetadata:\n  bpr-version: 7\n  role: x\n"));
		expect(plain && plain->Description == "Reviews." && plain->Version == 7U, "bare values and a nested version");
		const auto quoted = core::inspect_review_skill(skill("name: 'bpr-pr-review'\ndescription: \"It says \\\"review\\\".\"\nmetadata:\n    bpr-version: \"2\"\n"));
		expect(quoted && quoted->Description == "It says \"review\"." && quoted->Version == 2U, "quoted values, any indent under metadata");
		const auto crlf = core::inspect_review_skill("---\r\nname: bpr-pr-review\r\ndescription: Reviews.\r\n---\r\nbody\r\n");
		expect(crlf && !crlf->Version, "CRLF lines read, and a missing version is absent");
		const auto topLevelVersion = core::inspect_review_skill(skill("name: bpr-pr-review\ndescription: d\nbpr-version: 3\n"));
		expect(topLevelVersion && !topLevelVersion->Version, "a version outside metadata is not the version");
		const auto badVersion = core::inspect_review_skill(skill("name: bpr-pr-review\ndescription: d\nmetadata:\n  bpr-version: 3a\n"));
		expect(badVersion && !badVersion->Version, "a version that is not a number is absent");

		expect(core::inspect_review_skill("# no front matter\n").error() == core::review_skill_refusal::no_front_matter, "no front matter is refused");
		expect(core::inspect_review_skill("---\nname: bpr-pr-review\ndescription: d\n").error() == core::review_skill_refusal::no_front_matter,
			   "unclosed front matter is refused");
		expect(core::inspect_review_skill(skill("description: d\n")).error() == core::review_skill_refusal::name_changed, "a missing name is refused");
		expect(core::inspect_review_skill(skill("name: my-review\ndescription: d\n")).error() == core::review_skill_refusal::name_changed, "another name is refused");
		expect(core::inspect_review_skill(skill("name: bpr-pr-review\n")).error() == core::review_skill_refusal::no_description, "a missing description is refused");
		expect(core::inspect_review_skill(skill("name: bpr-pr-review\ndescription: \"\"\n")).error() == core::review_skill_refusal::no_description,
			   "an empty description is refused");
		expect(core::inspect_review_skill(skill("name: bpr-pr-review\ndescription: d\n", std::string(core::maximum_review_skill_bytes, 'x'))).error()
				   == core::review_skill_refusal::too_large,
			   "a skill over 64 KiB is refused");
		expect(core::inspect_review_skill(skill("name: bpr-pr-review\ndescription: d\n", "\xff\xfe")).error() == core::review_skill_refusal::not_text,
			   "a skill that is not UTF-8 is refused");
		expect(core::stable_name(core::review_skill_refusal::name_changed) == "name_changed", "refusals have stable names");
	}

	void test_newer_version_offer()
	{
		const auto copy = core::review_skill_copy_record{ .BaseVersion = 2U };
		expect(!core::newer_review_skill(3U, std::nullopt), "an unedited default is never offered anything");
		expect(core::newer_review_skill(3U, copy) == 3U, "a copy made from version 2 is offered version 3");
		expect(!core::newer_review_skill(2U, copy), "the same version is not offered");
		auto kept        = copy;
		kept.KeptVersion = 3U;
		expect(!core::newer_review_skill(3U, kept), "Keep mine for version 3 is remembered");
		expect(core::newer_review_skill(4U, kept) == 4U, "and version 4 is offered again");
	}

	[[nodiscard]] auto thread(std::string aPath, const bool aResolved, std::string aBody) -> core::brief_thread
	{
		return { .Path = std::move(aPath), .Line = 3U, .Resolved = aResolved, .Comments = { { .Author = "octo", .Body = std::move(aBody) } } };
	}

	void test_brief_bounds()
	{
		auto input = core::review_brief_input{
			.Title       = std::string(2000U, 't'),
			.Description = std::string(10000U, 'a') + "\n" + std::string(10000U, 'b') + "\n",
			.BaseBranch  = "main",
			.Files       = { { .Path = "src/a.cpp" }, { .Path = "", .Status = core::brief_file_status::added }, { .Path = "src/b.cpp", .PreviousPath = "src/old.cpp", .Status = core::brief_file_status::renamed } },
			.Threads     = { thread("resolved.cpp", true, "fixed"), thread("big.cpp", false, std::string(core::maximum_brief_thread_bytes, 'x')), thread("open.cpp", false, "please look"), { .Path = "empty.cpp" } },
			.Workspace   = core::brief_workspace::linked_clone_history,
		};
		const auto brief = core::bound_review_brief(input);
		expect(brief.has_value() && core::valid_brief(*brief), "a bounded brief is valid");
		expect(brief->Title.size() == core::maximum_brief_title_bytes, "the title is cut to its bound");
		expect(brief->Description == std::string(10000U, 'a') + "\n" && brief->DescriptionCutBytes == 10001U, "the description is cut at a line and the cut counted");
		expect(brief->Files.size() == 2U && brief->FilesOmitted == 1U && brief->Files[1].PreviousPath == "src/old.cpp", "a file without a path is left out and counted");
		expect(brief->Threads.size() == 2U && brief->Threads[0].Path == "open.cpp" && brief->Threads[1].Path == "resolved.cpp" && brief->ThreadsOmitted == 1U,
			   "unresolved first, the oversized thread left out and counted, a later one still fits, an empty one dropped uncounted");
		expect(core::bound_review_brief(input) == brief, "bounding is deterministic");

		auto unbroken        = input;
		unbroken.Description = std::string(20000U, 'z');
		expect(core::bound_review_brief(unbroken)->Description.size() == core::maximum_brief_description_bytes, "a description without a line break is cut at its bound");

		auto withSkill  = input;
		withSkill.Skill = core::brief_review_skill{ .Source = core::review_skill_source::profile_copy, .Text = skill("name: other\ndescription: d\n") };
		expect(!core::bound_review_brief(withSkill), "a skill that is not a review skill refuses the brief");
		withSkill.Skill->Text = skill("name: bpr-pr-review\ndescription: d\n");
		expect(core::bound_review_brief(withSkill).has_value(), "a review skill rides the brief");

		auto tooBig = *brief;
		tooBig.Threads.push_back(thread("more.cpp", false, std::string(core::maximum_brief_thread_bytes, 'y')));
		expect(!core::valid_brief(tooBig), "threads over their bound are not a valid brief");
		expect(core::stable_name(core::brief_workspace::private_snapshot) == "private_snapshot" && core::stable_name(core::brief_file_status::type_changed) == "type_changed",
			   "workspace and status names are stable");
	}

	/// [[BPR-1753]], owner decision (b) 2026-10-23. The conversation so
	/// far rides the brief, newest kept, each side cut, the older counted.
	void test_brief_conversation()
	{
		auto input = core::review_brief_input{ .Title = "Fix" };
		for (auto index = 0; index < 3; ++index) {
			input.Conversation.push_back({ .Asked = "Question " + std::to_string(index), .Answered = "Answer " + std::to_string(index) });
		}
		const auto few = core::bound_review_brief(input);
		expect(few && few->Conversation == input.Conversation && few->ConversationOmitted == 0U && core::valid_brief(*few), "a short conversation rides whole, oldest first");

		auto many = core::review_brief_input{ };
		for (auto index = std::size_t{ 0U }; index < core::maximum_brief_turns + 4U; ++index) {
			many.Conversation.push_back({ .Asked = "Q" + std::to_string(index), .Answered = "A" });
		}
		const auto newest = core::bound_review_brief(many);
		expect(newest && newest->Conversation.size() == core::maximum_brief_turns && newest->ConversationOmitted == 4U && newest->Conversation.front().Asked == "Q4"
				   && newest->Conversation.back().Asked == "Q" + std::to_string(core::maximum_brief_turns + 3U),
			   "past the turn bound the oldest turns are left out and counted");

		auto lengthy = core::review_brief_input{ };
		for (auto index = 0; index < 8; ++index) {
			lengthy.Conversation.push_back({ .Asked = "Q" + std::to_string(index), .Answered = std::string(core::maximum_brief_turn_bytes + 100U, 'a') });
		}
		const auto cut   = core::bound_review_brief(lengthy);
		auto       bytes = std::size_t{ 0U };
		for (const auto& turn : cut->Conversation) {
			bytes += turn.Asked.size() + turn.Answered.size();
		}
		expect(cut && cut->Conversation.back().Asked == "Q7" && cut->Conversation.back().Answered.size() == core::maximum_brief_turn_bytes
				   && bytes <= core::maximum_brief_conversation_bytes && cut->ConversationOmitted == 8U - cut->Conversation.size() && cut->ConversationOmitted > 0U && core::valid_brief(*cut),
			   "each answer is cut to its bound and the turns together fit theirs");
		expect(core::bound_review_brief(lengthy) == cut, "bounding the conversation is deterministic");

		auto over                          = *few;
		over.Conversation.front().Answered = std::string(core::maximum_brief_turn_bytes + 1U, 'x');
		expect(!core::valid_brief(over), "an answer over its bound is not a valid brief");
	}

	[[nodiscard]] auto oid(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	void test_supplement_carries_the_brief()
	{
		auto request = core::agent_supplement_request{
			.ApplicationSession = core::application_session_id{ 1U },
			.Session            = { .Id = core::agent_session_id{ 2U }, .Profile = core::external_terminal_profile_id },
			.PullRequestUrl     = "https://github.com/o/r/pull/1",
		};
		request.Session.Binding.Host        = core::git_host_id{ 1U };
		request.Session.Binding.Repository  = core::repository_id{ 1U };
		request.Session.Binding.PullRequest = core::pull_request_id{ 1U };
		request.Session.Binding.BaseOid     = oid(1U);
		request.Session.Binding.HeadOid     = oid(2U);
		request.Session.Binding.LeftEpoch   = core::epoch_ordinal{ 1U };
		request.Session.Binding.RightEpoch  = core::epoch_ordinal{ 1U };
		request.Brief                       = core::bound_review_brief({ .Title = "Fix", .Description = "Body" });
		const auto built                    = core::build_agent_supplement(request);
		expect(built && built->SchemaVersion == 3U && built->Brief == request.Brief, "version three carries the brief unchanged");
		request.Brief->Title = std::string(core::maximum_brief_title_bytes + 1U, 't');
		expect(!core::build_agent_supplement(request), "a brief outside its bounds refuses the supplement");
	}
} // namespace

auto main() -> int
{
	test_the_shipped_skill_is_a_review_skill();
	test_front_matter();
	test_newer_version_offer();
	test_brief_bounds();
	test_brief_conversation();
	test_supplement_carries_the_brief();
	std::cout << "review-skill-tests:pass\n";
	return EXIT_SUCCESS;
}
