#ifndef BPR_APP_GITHUB_APPLICATION_REVIEW_MUTATION_SELECTION_HPP
#define BPR_APP_GITHUB_APPLICATION_REVIEW_MUTATION_SELECTION_HPP
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/review_mutation_port.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/port.hpp"
	#include <cstddef>
	#include <deque>
	#include <functional>
	#include <mutex>
#endif

// Which pull request the live review-mutation branch may act on: the
// designated BPR-Test target and the observed selection (RFC-BPR-0022).
namespace bpr::app {
	inline constexpr auto designated_review_hostname = std::string_view{ "github.com" };

	inline constexpr auto designated_review_owner = std::string_view{ "MikeMarcin" };

	inline constexpr auto designated_review_name = std::string_view{ "BPR-Test" };

	inline constexpr auto designated_review_repository_id = std::uint64_t{ 1'328'277'631U };

	inline constexpr auto designated_review_repository_node = std::string_view{ "R_kgDOTyvofw" };

	inline constexpr auto designated_review_pull_request_id = std::uint64_t{ 4'650'736'621U };

	inline constexpr auto designated_review_pull_request_node = std::string_view{ "PR_kwDOTyvof88AAAABFTSb7Q" };

	inline constexpr auto designated_review_number = std::uint32_t{ 2U };

	inline constexpr auto designated_review_base_ref = std::string_view{ "codex/rfc0022-fixture-base-20260926" };

	inline constexpr auto designated_review_base_full_ref = std::string_view{ "refs/heads/codex/rfc0022-fixture-base-20260926" };

	inline constexpr auto designated_review_head_ref = std::string_view{ "codex/rfc0022-live-comments-20260926" };

	inline constexpr auto designated_review_base_oid = std::string_view{ "a939a85a5525994fe9e9cb4c973ad9362ed427c7" };

	inline constexpr auto designated_review_head_oid = std::string_view{ "61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a" };

	[[nodiscard]] auto designated_review_replay_namespace_path(
		const std::filesystem::path& aApplicationDataRoot)
		-> std::filesystem::path;

	struct designated_review_mutation_selection
	{
		application::review_mutation_target_binding  Target;
		application::review_mutation_acting_identity ActingIdentity;
	};

	[[nodiscard, maybe_unused]] auto select_designated_review_mutation(
		const application::accepted_pull_request_handoff& aHandoff,
		const application::github_pull_request_snapshot&  aSnapshot)
		-> std::optional<designated_review_mutation_selection>;

	// [[BPR-0897]]. In a non-attended release build this composes the live
	// mutation provider for whichever pull request is open, and that is the
	// intended product behaviour: a review tool that cannot publish to the
	// pull request in front of the reviewer is not a review tool. The owner
	// recorded the intent on 2026-09-24 (decision e1d2139c) and chose neither
	// a gate until [[BPR-0641]] opens inline publication nor a per-repository
	// designation step. So only the observed identity is checked here: the
	// snapshot names the handed-off pull request, still open, at the
	// handed-off base and head. Contrast the attended designated-live lane,
	// which is gated twice: designated_live_mutation_gate proves the pull
	// request field by field before launch, and
	// select_designated_review_mutation above refuses unless every identity
	// equals a compile-time constant.
	//
	// [[BPR-1826]]. With `aAdmitClosed` a closed or merged pull request is
	// selected too, its binding saying it is not open: the write
	// connection composed for it resolves and unresolves threads and
	// writes nothing else (`application::valid_for`). Every other caller
	// still asks for an open pull request.
	[[nodiscard]] auto select_observed_review_mutation(
		const application::accepted_pull_request_handoff& aHandoff,
		const application::github_pull_request_snapshot&  aSnapshot,
		const bool                                        aAdmitClosed = false)
		-> std::optional<designated_review_mutation_selection>;
#if defined(BPR_BUILD_TESTING)
	/// Holds decision posts for deterministic fixture races.
	class gated_review_mutation_decision_executor final
		: public application::decision_executor
	{
	public:
		explicit gated_review_mutation_decision_executor(
			application::decision_executor& aTarget);

		void post(std::function<void()> aTask) override;

		void hold();

		void release();

		[[nodiscard]] auto release_next() -> bool;

		[[nodiscard]] auto pending() const -> std::size_t;

	private:
		application::decision_executor&   Target;
		mutable std::mutex                Mutex;
		std::deque<std::function<void()>> Held;
		bool                              Holding = false;
	};
#endif
}
#endif
