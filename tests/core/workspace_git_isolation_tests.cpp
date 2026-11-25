// RFC-BPR-0096. The command-scope configuration BetterPullReview's own Git
// runs with in a retained agent workspace: fixed overrides, every driver the
// repository's configuration names disabled, a configured hook refused.
#include "bpr/core/workspace_git_isolation.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;
	using namespace std::string_view_literals;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto holds(const workspace_git_isolation& aIsolation, const std::string_view aKey, const std::string_view aValue) -> bool
	{
		return std::ranges::any_of(aIsolation.Overrides, [&](const git_config_override& aOverride) { return aOverride.Key == aKey && aOverride.Value == aValue; });
	}

	void test_fixed_overrides()
	{
		const auto fixed = fixed_workspace_git_overrides();
		const auto empty = plan_workspace_git_isolation({ });
		expect(empty.Refusal == workspace_git_refusal::none && empty.Overrides == fixed, "an empty listing carries the fixed overrides alone");
		for (const auto& [key, value] : { std::pair{ "core.fsmonitor"sv, "false"sv }, std::pair{ "core.editor"sv, ":"sv }, std::pair{ "sequence.editor"sv, ":"sv },
										  std::pair{ "log.showSignature"sv, "false"sv }, std::pair{ "commit.gpgSign"sv, "false"sv }, std::pair{ "diff.external"sv, ""sv },
										  std::pair{ "credential.helper"sv, ""sv }, std::pair{ "rebase.backend"sv, "merge"sv }, std::pair{ "rerere.enabled"sv, "false"sv },
										  std::pair{ "protocol.allow"sv, "never"sv }, std::pair{ "protocol.ext.allow"sv, "never"sv }, std::pair{ "core.askPass"sv, ""sv } }) {
			expect(holds(empty, key, value), "the fixed overrides set " + std::string{ key });
		}
	}

	void test_drivers()
	{
		// Git's own listing: lower-case sections and keys, the subsection as
		// written, dots inside it included.
		const auto listing   = "core.repositoryformatversion\0filter.evil.clean\0filter.evil.smudge\0diff.Evil.textconv\0merge.my.driver.driver\0"
							   "filter.evil.process\0user.name\0include.path\0diff.external\0merge.conflictstyle\0"sv;
		const auto planned   = plan_workspace_git_isolation(listing);
		const auto fixedSize = fixed_workspace_git_overrides().size();
		expect(planned.Refusal == workspace_git_refusal::none, "a listing with drivers is planned");
		expect(planned.Overrides.size() == fixedSize + 4U + 2U + 1U, "each driver is disabled once");
		for (const auto key : { "filter.evil.clean"sv, "filter.evil.smudge"sv, "filter.evil.process"sv, "diff.Evil.textconv"sv, "diff.Evil.command"sv, "merge.my.driver.driver"sv }) {
			expect(holds(planned, key, ""), "the driver's command is emptied: " + std::string{ key });
		}
		expect(holds(planned, "filter.evil.required", "false"), "a required filter is not required");
		expect(planned.Overrides[fixedSize].Key == "filter.evil.clean" && planned.Overrides[fixedSize + 4U].Key == "diff.Evil.textconv",
			   "drivers follow the fixed overrides in the order the listing names them");
		expect(plan_workspace_git_isolation(listing) == planned, "equal listings plan equally");
	}

	void test_refusals()
	{
		expect(plan_workspace_git_isolation("hook.lint.command\0"sv).Refusal == workspace_git_refusal::configured_hook, "a configured hook is refused");
		expect(plan_workspace_git_isolation("hook.lint.command\0"sv).Overrides.empty(), "a refusal carries no overrides");
		for (const auto malformed : { "core.bare"sv, "nodot\0"sv, ".x\0"sv, "core.\0"sv, "core.bare\0\0"sv }) {
			expect(plan_workspace_git_isolation(malformed).Refusal == workspace_git_refusal::malformed_listing, "a listing that is not Git's is refused");
		}
		auto many = std::string{ };
		for (auto index = std::size_t{ 0U }; index <= maximum_workspace_git_drivers; ++index) {
			many += "filter.d" + std::to_string(index) + ".clean";
			many += '\0';
		}
		expect(plan_workspace_git_isolation(many).Refusal == workspace_git_refusal::too_many_drivers, "more drivers than the bound are refused");
		many.resize(many.rfind("filter.d"));
		expect(plan_workspace_git_isolation(many).Refusal == workspace_git_refusal::none, "the bound itself is planned");
	}
}

int main()
{
	test_fixed_overrides();
	test_drivers();
	test_refusals();
	return EXIT_SUCCESS;
}
