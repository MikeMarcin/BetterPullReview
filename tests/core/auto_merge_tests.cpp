// [[BPR-2329]]. Auto-merge's offer, decisions and overlay, without Qt or a provider.
#include "bpr/core/auto_merge.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::auto_merge_change;
	using core::auto_merge_refusal;
	using core::merge_method;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto facts(const bool aEnabled) -> core::auto_merge_facts
	{
		return { .Open = true, .Allowed = true, .Enabled = aEnabled, .ViewerCanEnable = true, .ViewerCanDisable = true, .Methods = { .Merge = false, .Squash = true, .Rebase = true } };
	}

	void test_the_offer_follows_the_repository_and_github()
	{
		const auto enable = core::auto_merge_offered(facts(false));
		expect(enable.Change == auto_merge_change::enable && enable.Methods == std::vector{ merge_method::squash, merge_method::rebase }, "off: Enable, with only the methods the repository allows");
		expect(core::auto_merge_offered(facts(true)).Change == auto_merge_change::disable, "on: Disable");
		auto closed = facts(false);
		closed.Open = false;
		expect(!core::auto_merge_offered(closed).Change, "a closed pull request offers nothing");
		auto disallowed    = facts(true);
		disallowed.Allowed = false;
		expect(!core::auto_merge_offered(disallowed).Change, "a repository that does not allow auto-merge offers nothing");
		auto viewer            = facts(false);
		viewer.ViewerCanEnable = false;
		expect(!core::auto_merge_offered(viewer).Change, "GitHub saying the viewer may not enable it offers nothing");
		auto noMethod    = facts(false);
		noMethod.Methods = { };
		expect(!core::auto_merge_offered(noMethod).Change, "no allowed method offers nothing");
	}

	void test_a_change_is_decided_in_order()
	{
		expect(core::decide_auto_merge_change(facts(false), auto_merge_change::enable, merge_method::squash) == auto_merge_refusal::none, "enable with an allowed method");
		expect(core::decide_auto_merge_change(facts(false), auto_merge_change::enable, merge_method::merge) == auto_merge_refusal::method_not_allowed, "a method the repository does not allow");
		expect(core::decide_auto_merge_change(facts(false), auto_merge_change::enable, std::nullopt) == auto_merge_refusal::method_not_allowed, "enable needs a method");
		expect(core::decide_auto_merge_change(facts(true), auto_merge_change::enable, merge_method::squash) == auto_merge_refusal::already, "already on");
		expect(core::decide_auto_merge_change(facts(false), auto_merge_change::disable, std::nullopt) == auto_merge_refusal::already, "already off");
		expect(core::decide_auto_merge_change(facts(true), auto_merge_change::disable, std::nullopt) == auto_merge_refusal::none, "disable");
		auto closed = facts(false);
		closed.Open = false;
		expect(core::decide_auto_merge_change(closed, auto_merge_change::enable, merge_method::squash) == auto_merge_refusal::not_open, "closed first");
		auto disallowed    = facts(false);
		disallowed.Allowed = false;
		expect(core::decide_auto_merge_change(disallowed, auto_merge_change::enable, merge_method::squash) == auto_merge_refusal::not_allowed, "not allowed");
		auto viewer             = facts(true);
		viewer.ViewerCanDisable = false;
		expect(core::decide_auto_merge_change(viewer, auto_merge_change::disable, std::nullopt) == auto_merge_refusal::not_permitted, "not permitted");
	}

	void test_the_overlay_holds_the_answer_until_a_read_agrees()
	{
		auto overlay = core::auto_merge_overlay{ };
		core::begin_auto_merge_change(overlay, 3U, auto_merge_change::enable, merge_method::squash);
		expect(core::project_auto_merge(false, overlay) == core::projected_auto_merge{ .Enabled = true, .Pending = true, .Method = merge_method::squash }, "shown on at once, pending");
		core::settle_auto_merge_change(overlay, 4U, true);
		expect(overlay.pending(), "an answer to another request changes nothing");
		core::settle_auto_merge_change(overlay, 3U, true);
		expect(core::project_auto_merge(false, overlay) == core::projected_auto_merge{ .Enabled = true, .Method = merge_method::squash }, "answered, it is on before a read agrees");
		core::observe_auto_merge(overlay, false);
		expect(core::project_auto_merge(false, overlay).Enabled, "one stale read is not believed");
		core::observe_auto_merge(overlay, false);
		expect(!overlay.held() && !core::project_auto_merge(false, overlay).Enabled, "a second is");

		core::begin_auto_merge_change(overlay, 5U, auto_merge_change::disable, std::nullopt);
		core::abandon_auto_merge_change(overlay, 5U);
		expect(!overlay.held() && core::project_auto_merge(true, overlay) == core::projected_auto_merge{ .Enabled = true }, "a failed change shows the read again");
		core::begin_auto_merge_change(overlay, 6U, auto_merge_change::disable, std::nullopt);
		core::settle_auto_merge_change(overlay, 6U, false);
		core::observe_auto_merge(overlay, false);
		expect(!overlay.held(), "a read that agrees ends it");
	}
} // namespace

auto main() -> int
{
	test_the_offer_follows_the_repository_and_github();
	test_a_change_is_decided_in_order();
	test_the_overlay_holds_the_answer_until_a_read_agrees();
	std::cout << "auto-merge-core-tests:pass\n";
	return EXIT_SUCCESS;
}
