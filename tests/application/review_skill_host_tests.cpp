/// \file
/// \brief [[BPR-PENDING-AQ5]], RFC-BPR-0092 owner decision D1 and answer Q9 A.
/// The review skill host on manual lanes with a memory store: the built-in
/// default, the first edit making the copy with its base version and digest,
/// a refused save keeping nothing, Restore default saving a copy first and
/// removing nothing when that fails, a newer built-in offered and Keep mine
/// remembered, and the skill each brief carries.

#include "bpr/application/manual_executor.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/core/sha256.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using application::review_skill_answer;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class memory_store final : public application::review_skill_store_port
	{
	public:
		[[nodiscard]] auto load_review_skill() -> std::optional<std::optional<application::stored_review_skill>> override
		{
			return Readable ? std::optional{ Stored } : std::nullopt;
		}
		[[nodiscard]] auto store_review_skill(const application::stored_review_skill& aSkill) -> bool override
		{
			if (Refuse) {
				return false;
			}
			Stored = aSkill;
			return true;
		}
		[[nodiscard]] auto forget_review_skill() -> bool override
		{
			Stored.reset();
			return true;
		}

		std::optional<application::stored_review_skill> Stored;
		bool                                            Readable = true;
		bool                                            Refuse   = false;
	};

	[[nodiscard]] auto skill_text(const unsigned aVersion, const std::string_view aBody) -> std::string
	{
		return "---\nname: bpr-pr-review\ndescription: Reviews.\nmetadata:\n  bpr-version: " + std::to_string(aVersion) + "\n---\n" + std::string{ aBody };
	}

	struct fixture
	{
		application::manual_executor                    Effects;
		application::manual_executor                    Decision;
		memory_store                                    Store;
		std::vector<std::string>                        Copies;
		bool                                            CopyFails = false;
		std::unique_ptr<application::review_skill_host> Host;
		std::optional<review_skill_answer>              Answer;

		explicit fixture(std::string aBuiltIn)
		{
			auto dependencies     = application::review_skill_dependencies{ };
			dependencies.Store    = &Store;
			dependencies.BuiltIn  = std::move(aBuiltIn);
			dependencies.SaveCopy = [this](const std::string& aText) -> std::optional<std::string> {
				if (CopyFails) {
					return std::nullopt;
				}
				Copies.push_back(aText);
				return "/Users/me/Downloads/bpr-pr-review SKILL " + std::to_string(Copies.size()) + ".md";
			};
			dependencies.Effects  = &Effects;
			dependencies.Decision = &Decision;
			Host                  = std::make_unique<application::review_skill_host>(std::move(dependencies));
		}

		void settle()
		{
			for (auto turn = 0; turn < 4; ++turn) {
				Effects.drain();
				Decision.drain();
			}
		}

		[[nodiscard]] auto answered() -> application::review_skill_host::answer
		{
			Answer.reset();
			return [this](const review_skill_answer aAnswer) { Answer = aAnswer; };
		}
	};

	void test_the_built_in_default_until_an_edit()
	{
		auto test = fixture{ skill_text(1U, "Built in.\n") };
		test.Host->load();
		test.settle();
		const auto view = test.Host->projection();
		expect(view.Loaded && view.Source == core::review_skill_source::built_in && view.BuiltInVersion == 1U && !view.Offer && view.Text == skill_text(1U, "Built in.\n"),
			   "with no copy the built-in text is the skill");
		const auto brief = test.Host->brief_skill();
		expect(brief && brief->Source == core::review_skill_source::built_in && brief->Version == 1U && brief->Text == view.Text, "and the brief carries it");

		test.Host->save(skill_text(1U, "Built in.\n"), test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::done && !test.Store.Stored, "saving the built-in text unchanged makes no copy");
	}

	void test_the_first_edit_makes_the_copy()
	{
		const auto builtIn = skill_text(1U, "Built in.\n");
		auto       test    = fixture{ builtIn };
		test.Host->load();
		test.settle();
		test.Host->save(skill_text(1U, "Mine.\n"), test.answered());
		test.settle();
		const auto digest = core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(builtIn.data()), builtIn.size() });
		expect(test.Answer == review_skill_answer::done && test.Store.Stored && test.Store.Stored->Text == skill_text(1U, "Mine.\n")
				   && test.Store.Stored->Record.BaseVersion == 1U && test.Store.Stored->Record.BaseDigest == digest,
			   "the first edit stores the copy with the built-in version and digest it came from");
		const auto view = test.Host->projection();
		expect(view.Source == core::review_skill_source::profile_copy && view.CopyBaseVersion == 1U, "the copy is the skill now");
		expect(test.Host->brief_skill()->Source == core::review_skill_source::profile_copy && test.Host->brief_skill()->Text == skill_text(1U, "Mine.\n"),
			   "and the brief carries the copy");

		test.Host->save("---\nname: something-else\ndescription: d\n---\n", test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::refused && test.Host->projection().Refusal == core::review_skill_refusal::name_changed
				   && test.Store.Stored->Text == skill_text(1U, "Mine.\n"),
			   "a save that renames the skill is refused and keeps the stored copy");

		test.Store.Refuse = true;
		test.Host->save(skill_text(1U, "Another.\n"), test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::not_saved && test.Host->projection().Text == skill_text(1U, "Mine.\n"), "a store that refuses leaves the copy as it was");
	}

	void test_restore_default_saves_a_copy_first()
	{
		auto test         = fixture{ skill_text(1U, "Built in.\n") };
		test.Store.Stored = application::stored_review_skill{ .Text = skill_text(1U, "Mine.\n"), .Record = { .BaseVersion = 1U } };
		test.Host->load();
		test.settle();
		test.CopyFails = true;
		test.Host->use_built_in(true, test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::copy_failed && test.Store.Stored.has_value(), "when the copy for Downloads fails nothing is removed");
		test.CopyFails = false;
		test.Host->use_built_in(true, test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::done && !test.Store.Stored && test.Copies == std::vector{ skill_text(1U, "Mine.\n") }
				   && test.Host->projection().SavedCopy == "/Users/me/Downloads/bpr-pr-review SKILL 1.md"
				   && test.Host->projection().Source == core::review_skill_source::built_in,
			   "Restore default writes the copy to Downloads, then removes it and says where");
		test.Store.Stored = application::stored_review_skill{ .Text = skill_text(1U, "Again.\n"), .Record = { .BaseVersion = 1U } };
		test.Host->load();
		test.settle();
		test.Host->use_built_in(false, test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::done && !test.Store.Stored && test.Copies.size() == 1U, "unticked, no copy is written");
	}

	void test_a_newer_built_in_is_offered_and_never_applied_by_itself()
	{
		auto test         = fixture{ skill_text(2U, "Built in two.\n") };
		test.Store.Stored = application::stored_review_skill{ .Text = skill_text(1U, "Mine.\n"), .Record = { .BaseVersion = 1U } };
		test.Host->load();
		test.settle();
		auto view = test.Host->projection();
		expect(view.Offer == 2U && view.Source == core::review_skill_source::profile_copy && view.Text == skill_text(1U, "Mine.\n"),
			   "version 2 is offered and the edited copy stays the skill");
		test.Host->keep_mine(test.answered());
		test.settle();
		expect(test.Answer == review_skill_answer::done && test.Store.Stored->Record.KeptVersion == 2U && !test.Host->projection().Offer,
			   "Keep mine is remembered for version 2");

		auto later         = fixture{ skill_text(3U, "Built in three.\n") };
		later.Store.Stored = test.Store.Stored;
		later.Host->load();
		later.settle();
		expect(later.Host->projection().Offer == 3U, "and version 3 is offered again");
	}

	void test_a_broken_stored_copy_falls_back_to_the_built_in_text()
	{
		auto test         = fixture{ skill_text(1U, "Built in.\n") };
		test.Store.Stored = application::stored_review_skill{ .Text = "no front matter", .Record = { .BaseVersion = 1U } };
		test.Host->load();
		test.settle();
		const auto view = test.Host->projection();
		expect(view.StoredProblem == core::review_skill_refusal::no_front_matter && view.Source == core::review_skill_source::profile_copy,
			   "a stored copy that is no longer a review skill is reported");
		expect(test.Host->brief_skill()->Source == core::review_skill_source::built_in, "and requests carry the built-in text until it is fixed");

		auto unreadable           = fixture{ skill_text(1U, "Built in.\n") };
		unreadable.Store.Readable = false;
		unreadable.Host->load();
		unreadable.settle();
		expect(unreadable.Host->projection().Loaded && unreadable.Host->brief_skill()->Source == core::review_skill_source::built_in, "an unreadable store leaves the built-in text");
		unreadable.Host->shutdown();
		expect(unreadable.Host->is_quiescent(), "nothing is left running");
	}

	void test_the_shipped_skill_is_built_in()
	{
		const auto facts = core::inspect_review_skill(application::built_in_review_skill());
		expect(facts && facts->Version == 2U, "the built-in text is the packaged bpr-pr-review, version 2");
	}
} // namespace

auto main() -> int
{
	test_the_built_in_default_until_an_edit();
	test_the_first_edit_makes_the_copy();
	test_restore_default_saves_a_copy_first();
	test_a_newer_built_in_is_offered_and_never_applied_by_itself();
	test_a_broken_stored_copy_falls_back_to_the_built_in_text();
	test_the_shipped_skill_is_built_in();
	std::cout << "review-skill-host-tests:pass\n";
	return EXIT_SUCCESS;
}
