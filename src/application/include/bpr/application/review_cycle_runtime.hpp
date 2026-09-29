#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_capture.hpp"
#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/review_material_bridge.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

/// \file
/// \brief The local Review Cycles runtime (RFC-BPR-0053, RFC-BPR-0060).
/// \details Composed from a profile's cycle store and two executors, nothing
/// else: no GitHub account, provider, credential vault, agent adapter or
/// executable lookup is needed to open, create, capture or reopen a cycle.
namespace bpr::application {
	/// \brief Owns the cycle store on the decision lane.
	/// \details Store calls run one at a time on the effect lane in request
	/// order; each accepted call completes exactly once on the decision lane.
	/// Selecting another review subject cancels work issued for the previous
	/// selection, whose completions then report `cancelled`. Executors and
	/// completion targets must stay alive until `is_quiescent()`.
	class review_cycle_runtime final
	{
	public:
		template <typename T>
		using completion = std::function<void(cycle_store_result<T>)>;

		review_cycle_runtime(std::unique_ptr<review_cycle_store> aStore, effect_executor& aEffects, decision_executor& aDecisions);
		~review_cycle_runtime();
		review_cycle_runtime(const review_cycle_runtime&)                    = delete;
		auto operator=(const review_cycle_runtime&) -> review_cycle_runtime& = delete;
		review_cycle_runtime(review_cycle_runtime&&)                         = delete;
		auto operator=(review_cycle_runtime&&) -> review_cycle_runtime&      = delete;

		/// \brief Makes a subject current and supersedes work for the previous one.
		/// \returns The new selection generation.
		auto               select(core::review_subject aSubject) -> std::uint64_t;
		[[nodiscard]] auto selected() const noexcept -> const std::optional<core::review_subject>&;

		/// Each returns false, without calling the completion, when closing.
		auto open(completion<core::review_cycle_catalog> aDone) -> bool;
		auto create(cycle_operation_id aOperation, core::create_review_cycle aCommand, completion<cycle_write_receipt> aDone) -> bool;
		/// \brief Loads the selected cycle. A pull-request selection is refused:
		/// the cycle store never answers for another authority.
		auto load_selected(completion<std::optional<core::review_cycle_state>> aDone) -> bool;
		/// \brief Installs a manifest and records it as a capture of the selected cycle.
		auto capture_selected(cycle_operation_id aOperation, std::uint64_t aExpectedRevision, core::cycle_capture_observation aCapture,
							  std::vector<std::uint8_t> aManifest, completion<cycle_write_receipt> aDone) -> bool;

		/// \brief Captures the selected cycle's bound workspace through `aSource`
		/// on the effect lane: read, verify, install and record, or nothing.
		/// \details `aSource` must stay alive until the completion runs.
		auto capture_workspace(workspace_source_port& aSource, cycle_capture_request aRequest, completion<cycle_capture_outcome> aDone) -> bool;

		/// \brief One bounded page of a resolved pair of the selected cycle,
		/// from retained material only. Superseded by a new selection.
		auto compare_selected(core::cycle_comparison_pair aPair, std::optional<std::string> aAfter, std::size_t aLimit, bool aChangedOnly,
							  completion<cycle_comparison_page_observation> aDone) -> bool;
		/// \brief One file of a resolved pair (or full source) of the selected cycle.
		auto compare_file_selected(core::cycle_comparison_pair aPair, std::string aPath, core::diff_options aOptions, completion<cycle_file_observation> aDone) -> bool;

		/// \brief Stops admitting work and cancels outstanding calls.
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
