#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_SNAPSHOT_OBSERVATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_SNAPSHOT_OBSERVATION_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

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
				const core::pull_request_authority&)>;

		explicit snapshot_workbench_observation(file_supplier aFiles);

		[[nodiscard]] auto assemble(
			const workbench_observation_request& aRequest,
			std::stop_token                      aStop)
			-> workbench_file_observation override;

	private:
		file_supplier Files;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WORKBENCH_SNAPSHOT_OBSERVATION_HPP
