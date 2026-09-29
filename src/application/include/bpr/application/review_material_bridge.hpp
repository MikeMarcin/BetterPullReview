#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MATERIAL_BRIDGE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MATERIAL_BRIDGE_HPP

#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/review_cycle_store.hpp"

#include <cstdint>
#include <stop_token>
#include <variant>
#include <vector>

/// \file
/// \brief Material requests tagged by their authority (RFC-BPR-0053,
/// RFC-BPR-0054).
namespace bpr::application {
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
	/// of that cycle, and only an object that revision pinned (its manifest or
	/// content the manifest references). Neither side
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
