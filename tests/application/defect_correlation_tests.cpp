#include "bpr/application/defect_correlation.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto defect(
		const std::uint64_t      aId,
		const std::uint8_t       aAnchor,
		const core::defect_state aState) -> core::defect_record
	{
		return core::defect_record{
			.Id     = core::defect_id{ aId },
			.Anchor = core::source_line_ref{
				.Comparison = opaque<core::file_comparison_digest>(aAnchor),
				.Side       = core::diff_side::right,
				.Line       = 5U },
			.State = aState,
		};
	}

	[[nodiscard]] auto link(const std::uint64_t aId, std::string aThread)
		-> application::defect_thread_link
	{
		return application::defect_thread_link{
			.Defect   = core::defect_id{ aId },
			.ThreadId = std::move(aThread),
		};
	}

	using state = core::defect_state;

	void test_code_moving_beneath_a_defect_asks_for_a_look()
	{
		const auto defects = std::array{ defect(1U, 10U, state::open) };
		const auto changed = std::array{ opaque<core::file_comparison_digest>(10U) };

		const auto result = application::correlate(defects, { }, { }, changed);

		expect(
			result.Defects.front().State == state::needs_verification,
			"a defect whose anchor moved is put in front of the reviewer");
		expect(
			result.Evidence.size() == 1U
				&& result.Evidence.front().Kind
					== core::defect_evidence_kind::nearby_code_change,
			"the evidence names why");
	}

	void test_an_untouched_anchor_raises_nothing()
	{
		const auto defects = std::array{ defect(1U, 10U, state::open) };
		const auto changed = std::array{ opaque<core::file_comparison_digest>(99U) };

		const auto result = application::correlate(defects, { }, { }, changed);

		expect(
			result.Defects.front().State == state::open
				&& result.Evidence.empty(),
			"a defect elsewhere is left alone");
	}

	void test_an_author_reply_and_a_resolution_are_both_evidence()
	{
		const auto defects = std::array{ defect(1U, 10U, state::open) };
		const auto links   = std::array{ link(1U, "thread-a") };

		const auto replied = std::array{ application::defect_thread_observation{
			.ThreadId = "thread-a", .AuthorRepliedSince = true } };
		const auto afterReply
			= application::correlate(defects, links, replied, { });
		expect(
			afterReply.Defects.front().State == state::needs_verification
				&& afterReply.Evidence.front().Kind
					== core::defect_evidence_kind::author_reply,
			"an author reply asks the reviewer to look");

		const auto resolved = std::array{ application::defect_thread_observation{
			.ThreadId = "thread-a", .Resolved = true } };
		const auto afterResolution
			= application::correlate(defects, links, resolved, { });
		expect(
			afterResolution.Defects.front().State == state::needs_verification
				&& afterResolution.Evidence.front().Kind
					== core::defect_evidence_kind::thread_resolved_externally,
			"an externally resolved thread asks the reviewer to look");
	}

	// The property the whole RFC turns on, asserted at the host rather than
	// inherited from the core's assertion of it.
	void test_no_correlation_ever_verifies()
	{
		const auto anchor = opaque<core::file_comparison_digest>(10U);

		for (const auto starting :
			 { state::open, state::fix_claimed, state::needs_verification,
			   state::reopened, state::verified, state::withdrawn,
			   state::accepted_as_is }) {
			const auto defects = std::array{ defect(1U, 10U, starting) };
			const auto links   = std::array{ link(1U, "thread-a") };
			const auto changed = std::array{ anchor };

			// Everything at once: code moved, the author replied, and someone
			// resolved the thread. If any combination could verify, this is it.
			const auto threads = std::array{ application::defect_thread_observation{
				.ThreadId           = "thread-a",
				.Resolved           = true,
				.AuthorRepliedSince = true } };

			const auto result
				= application::correlate(defects, links, threads, changed);

			if (starting == state::verified) {
				expect(
					result.Defects.front().State == state::verified,
					"a verified defect stays verified rather than being "
					"disturbed");
			} else {
				expect(
					result.Defects.front().State != state::verified,
					"no combination of evidence ever produces verified");
			}
		}
	}

	void test_a_decided_defect_is_left_alone()
	{
		for (const auto decided :
			 { state::verified, state::withdrawn, state::accepted_as_is }) {
			const auto defects = std::array{ defect(1U, 10U, decided) };
			const auto links   = std::array{ link(1U, "thread-a") };
			const auto changed = std::array{ opaque<core::file_comparison_digest>(10U) };
			const auto threads = std::array{ application::defect_thread_observation{
				.ThreadId = "thread-a", .Resolved = true, .AuthorRepliedSince = true } };

			const auto result
				= application::correlate(defects, links, threads, changed);

			expect(
				result.Defects.front().State == decided
					&& result.Evidence.empty(),
				"a decided defect is untouched and raises nothing");
		}
	}

	void test_correlation_touches_only_the_defect_it_names()
	{
		const auto defects = std::array{
			defect(1U, 10U, state::open),
			defect(2U, 20U, state::open),
		};
		const auto links   = std::array{ link(1U, "thread-a") };
		const auto threads = std::array{ application::defect_thread_observation{
			.ThreadId = "thread-a", .AuthorRepliedSince = true } };

		const auto result = application::correlate(defects, links, threads, { });

		expect(
			result.Defects.at(0).State == state::needs_verification,
			"the linked defect is flagged");
		expect(
			result.Defects.at(1).State == state::open,
			"the unlinked defect beside it is untouched");
		expect(
			result.Evidence.size() == 1U
				&& result.Evidence.front().Defect == core::defect_id{ 1U },
			"the evidence names the defect it belongs to");
	}

	void test_an_unlinked_thread_observation_is_ignored()
	{
		const auto defects = std::array{ defect(1U, 10U, state::open) };
		const auto threads = std::array{ application::defect_thread_observation{
			.ThreadId = "thread-nobody-linked", .AuthorRepliedSince = true } };

		const auto result = application::correlate(defects, { }, threads, { });

		expect(
			result.Defects.front().State == state::open
				&& result.Evidence.empty(),
			"a thread no defect is linked to raises nothing");
	}

	void test_repeated_correlation_settles()
	{
		const auto defects = std::array{ defect(1U, 10U, state::open) };
		const auto changed = std::array{ opaque<core::file_comparison_digest>(10U) };

		const auto first = application::correlate(defects, { }, { }, changed);
		const auto again
			= application::correlate(first.Defects, { }, { }, changed);

		expect(
			again.Defects == first.Defects,
			"correlating the same facts twice reaches the same state");
		expect(
			again.Evidence.empty(),
			"the second pass raises nothing new");
	}
} // namespace

auto main() -> int
{
	test_code_moving_beneath_a_defect_asks_for_a_look();
	test_an_untouched_anchor_raises_nothing();
	test_an_author_reply_and_a_resolution_are_both_evidence();
	test_no_correlation_ever_verifies();
	test_a_decided_defect_is_left_alone();
	test_correlation_touches_only_the_defect_it_names();
	test_an_unlinked_thread_observation_is_ignored();
	test_repeated_correlation_settles();

	std::cout << "defect-correlation-tests:pass\n";
	return EXIT_SUCCESS;
}
