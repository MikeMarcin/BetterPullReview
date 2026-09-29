#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RUNTIME_HPP

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_cycle_store.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
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

		/// \brief Stops admitting work and cancels outstanding calls.
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};

	/// \brief Cycle material named by subject, revision and object.
	struct cycle_material_request
	{
		core::local_cycle_subject      Subject;
		core::cycle_source_revision_id Revision;
		cycle_object_ref               Object;
		std::uint64_t                  MaximumBytes = 0;
	};
	/// \brief A material read tagged by its authority: a pull request's Git
	/// blob through the existing diff blob port, or a cycle's stored object.
	using review_material_request = std::variant<diff_blob_request, cycle_material_request>;

	enum class review_material_status : std::uint8_t {
		available,
		missing,
		/// The request names an authority this bridge does not serve.
		wrong_authority,
		too_large,
		corrupt,
		cancelled,
		failure,
	};
	struct review_material_observation
	{
		review_material_status    Status = review_material_status::failure;
		std::vector<std::uint8_t> Bytes;
	};

	/// \brief Routes a tagged material request to its own authority's source.
	/// \details Pull-request requests go unchanged to the existing diff blob
	/// port; a local runtime has none and refuses them. Cycle requests are
	/// served only for this profile's catalog authority, only for a revision
	/// of that cycle, and only the object that revision recorded. Neither side
	/// falls back to the other.
	class review_material_bridge final
	{
	public:
		review_material_bridge(diff_blob_port* aPullRequests, review_cycle_store* aCycles, core::local_cycle_authority aCycleAuthority) noexcept;
		[[nodiscard]] auto materialize(const review_material_request& aRequest, std::stop_token aStop) -> review_material_observation;

	private:
		diff_blob_port*             PullRequests;
		review_cycle_store*         Cycles;
		core::local_cycle_authority CycleAuthority;
	};
}

#endif
