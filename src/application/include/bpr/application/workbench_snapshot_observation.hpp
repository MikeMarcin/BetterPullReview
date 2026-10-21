#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_SNAPSHOT_OBSERVATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_SNAPSHOT_OBSERVATION_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/review_ledger.hpp"

#include <functional>
#include <optional>
#include <stop_token>
#include <vector>

namespace bpr::application {
	/// \brief Answers workbench observations from the read model's file list.
	///
	/// \details The only source of a review's files is
	/// `github_pull_request_snapshot::Files`, and the only way to name one is
	/// `core::file_identity_of`. This translates between them and holds no
	/// state of its own.
	///
	/// It reads no repository, runs no process, and touches no clock. The
	/// supplier is a plain callable so that composition decides where the
	/// snapshot comes from and this class never learns.
	class snapshot_workbench_observation final : public workbench_observation_port
	{
	public:
		/// \brief Supplies the files of one review, or nothing if unavailable.
		///
		/// \details Returning a disengaged optional means the snapshot for that
		/// authority is not held — a different thing from a review that has no
		/// files, which is an engaged empty vector. Collapsing the two would
		/// make "not loaded yet" indistinguishable from "nothing to review".
		using file_supplier = std::function<
			std::optional<std::vector<github_file_record>>(
				const workbench_pull_request_authority&)>;

		/// \brief Supplies accepted exact-material ledger facts for one file.
		///
		/// \details No value means this file's current comparison material is not
		/// known, so it remains conservatively actionable. The supplier must never
		/// substitute a receipt for another authority, file, or material.
		using actionability_supplier = std::function<
			std::optional<core::actionability_input>(
				const workbench_pull_request_authority&,
				core::file_identity)>;

		/// \brief RFC-BPR-0089. How a commit filter forms one file's diff.
		struct file_filter
		{
			std::optional<core::filtered_diff_endpoints> Endpoints;
			bool                                         Split = false;
		};

		/// \brief Supplies the commit filter's form for one file, or none when
		/// the file keeps the comparison's own diff.
		using filter_supplier = std::function<
			std::optional<file_filter>(
				const workbench_pull_request_authority&,
				core::file_identity)>;

		explicit snapshot_workbench_observation(
			file_supplier          aFiles,
			actionability_supplier aActionability = { },
			filter_supplier        aFilter        = { });

		[[nodiscard]] auto assemble(
			const workbench_observation_request& aRequest,
			std::stop_token                      aStop)
			-> workbench_file_observation override;

	private:
		file_supplier          Files;
		actionability_supplier Actionability;
		filter_supplier        Filter;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_SNAPSHOT_OBSERVATION_HPP
