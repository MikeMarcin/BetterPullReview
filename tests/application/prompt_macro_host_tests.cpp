/// \file
/// \brief [[BPR-2185]] (review N1 of BPR-2179). The suggested prompts' host
/// on manual lanes with a memory store: a save is written from the revision
/// it names, never the one the projection holds when it runs; a write over
/// another window's is stale; a refused write and an unreadable store fail
/// without changing the store; each outcome is heard before the store is
/// read back; a new prompt's identity is the host's; nothing is heard after
/// shutdown.

#include "bpr/application/manual_executor.hpp"
#include "bpr/application/prompt_macro_host.hpp"
#include "bpr/core/prompt_pack.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
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

	/// A store with the revision rule: a write from the revision held moves
	/// it one on; any other is stale.
	class memory_store final : public application::prompt_macro_store_port
	{
	public:
		[[nodiscard]] auto load() -> std::optional<application::prompt_macro_stored> override
		{
			if (Unreadable) {
				return std::nullopt;
			}
			return Held;
		}

		[[nodiscard]] auto store(const core::prompt_macro_differences& aDifferences, const std::uint64_t aFrom) -> application::prompt_macro_write override
		{
			Writes.push_back(aFrom);
			if (Refuse) {
				return application::prompt_macro_write::failed;
			}
			if (aFrom != Held.Revision) {
				return application::prompt_macro_write::stale;
			}
			Held.Differences = aDifferences;
			Held.Revision    = aFrom + 1U;
			return application::prompt_macro_write::stored;
		}

		application::prompt_macro_stored Held;
		std::vector<std::uint64_t>       Writes;
		bool                             Refuse     = false;
		bool                             Unreadable = false;
	};

	struct fixture
	{
		application::manual_executor                      Effects;
		application::manual_executor                      Decision;
		memory_store                                      Store;
		std::vector<std::string>                          Heard;
		std::vector<application::prompt_macro_projection> Told;
		std::unique_ptr<application::prompt_macro_host>   Host;

		explicit fixture(std::function<std::string()> aMint = { })
		{
			auto dependencies         = application::prompt_macro_dependencies{ };
			dependencies.Store        = &Store;
			dependencies.Effects      = &Effects;
			dependencies.Decision     = &Decision;
			dependencies.MintIdentity = std::move(aMint);
			dependencies.Changed      = [this](const application::prompt_macro_projection& aProjection) {
				Heard.emplace_back("changed");
				Told.push_back(aProjection);
			};
			Host = std::make_unique<application::prompt_macro_host>(std::move(dependencies));
		}

		void settle()
		{
			for (auto turn = 0; turn < 8; ++turn) {
				Effects.drain();
				Decision.drain();
			}
		}

		void save(core::prompt_macro_differences aDifferences, const std::uint64_t aFrom, std::optional<application::prompt_macro_write>& aOutcome)
		{
			Host->save(std::move(aDifferences), aFrom, [this, &aOutcome](const application::prompt_macro_write aWrite) {
				Heard.emplace_back("settled");
				aOutcome = aWrite;
			});
			settle();
		}
	};

	[[nodiscard]] auto one_own(const std::string& aId) -> core::prompt_macro_differences
	{
		auto differences = core::prompt_macro_differences{ };
		differences.Own.push_back(core::prompt_macro{ .Id = aId, .Body = { .Name = aId, .Template = aId } });
		return differences;
	}

	void test_a_save_names_its_revision()
	{
		auto test = fixture{ };
		test.Host->load();
		test.settle();
		expect(test.Host->projection().Loaded && test.Host->projection().Readable && test.Host->projection().Revision == 0U, "the store is read at revision 0");
		// Another window writes; this window reads it back on focus.
		test.Store.Held.Differences = one_own("own.theirs");
		test.Store.Held.Revision    = 1U;
		test.Host->load();
		test.settle();
		expect(test.Host->projection().Revision == 1U, "the projection holds the newer revision");
		// The page's edits were derived from revision 0.
		auto outcome = std::optional<application::prompt_macro_write>{ };
		test.Heard.clear();
		test.save(one_own("own.mine"), 0U, outcome);
		expect(test.Store.Writes.back() == 0U, "review N1(b): the store is asked to write from the revision named, not the one the projection holds");
		expect(outcome == application::prompt_macro_write::stale && test.Store.Held.Differences == one_own("own.theirs"), "so another window's write is never erased");
		expect(test.Heard == std::vector<std::string>{ "settled", "changed" }, "the outcome is heard before the store read back is told");
		expect(test.Told.back().Revision == 1U && test.Told.back().LastWrite == application::prompt_macro_write::stale, "and the projection is the store's");
		test.save(one_own("own.mine"), 1U, outcome);
		expect(outcome == application::prompt_macro_write::stored && test.Store.Held.Revision == 2U && test.Host->projection().Revision == 2U, "a write from the revision held is stored");
	}

	void test_failures_change_nothing()
	{
		auto test = fixture{ };
		test.Host->load();
		test.settle();
		test.Store.Refuse = true;
		auto outcome      = std::optional<application::prompt_macro_write>{ };
		test.save(one_own("own.mine"), 0U, outcome);
		expect(outcome == application::prompt_macro_write::failed && test.Store.Held.Revision == 0U, "a refused write fails and changes nothing");
		test.Store.Refuse     = false;
		test.Store.Unreadable = true;
		test.Host->load();
		test.settle();
		expect(!test.Host->projection().Readable, "an unreadable store");
		const auto writes = test.Store.Writes.size();
		test.save(one_own("own.mine"), 0U, outcome);
		expect(outcome == application::prompt_macro_write::failed && test.Store.Writes.size() == writes, "is never written");
	}

	void test_saves_go_in_order()
	{
		auto test = fixture{ };
		test.Host->load();
		test.settle();
		auto first  = std::optional<application::prompt_macro_write>{ };
		auto second = std::optional<application::prompt_macro_write>{ };
		test.Host->save(one_own("own.a"), 0U, [&first](const application::prompt_macro_write aWrite) { first = aWrite; });
		test.Host->save(one_own("own.b"), 1U, [&second](const application::prompt_macro_write aWrite) { second = aWrite; });
		test.settle();
		expect(first == application::prompt_macro_write::stored && second == application::prompt_macro_write::stored && test.Store.Held.Revision == 2U, "one store operation at a time, in order");
	}

	void test_identities()
	{
		auto       own = fixture{ };
		const auto id  = own.Host->mint_identity();
		expect(bpr::core::valid_prompt_uuid(id), "RFC-BPR-0125 AC24: the host mints a version 4 UUID");
		expect(own.Host->mint_identity() != id, "a fresh one each time");
		auto minted = fixture{ [] { return std::string{ "fixed" }; } };
		expect(minted.Host->mint_identity() == "fixed", "a composition may supply the identity");
		const auto counted = application::mint_prompt_uuid([] { return std::array<std::uint8_t, 16>{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; });
		expect(counted == "ffffffff-ffff-4fff-bfff-ffffffffffff", "the version and variant bits are set");
	}

	void test_shutdown()
	{
		auto test = fixture{ };
		test.Host->load();
		test.settle();
		auto outcome = std::optional<application::prompt_macro_write>{ };
		test.Host->save(one_own("own.late"), 0U, [&outcome](const application::prompt_macro_write aWrite) { outcome = aWrite; });
		test.Host->shutdown();
		const auto told = test.Told.size();
		test.settle();
		expect(!outcome && test.Told.size() == told && test.Host->is_quiescent(), "after shutdown nothing is heard and the host is quiescent");
	}
} // namespace

auto main() -> int
{
	test_a_save_names_its_revision();
	test_failures_change_nothing();
	test_saves_go_in_order();
	test_identities();
	test_shutdown();
	std::cout << "prompt-macro-host-tests:pass\n";
	return EXIT_SUCCESS;
}
