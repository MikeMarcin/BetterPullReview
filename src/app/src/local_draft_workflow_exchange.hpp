#ifndef BPR_SRC_APP_SRC_LOCAL_DRAFT_WORKFLOW_EXCHANGE_HPP
#define BPR_SRC_APP_SRC_LOCAL_DRAFT_WORKFLOW_EXCHANGE_HPP

#include "bpr/application/local_draft_workbench.hpp"

#include <filesystem>

namespace bpr::app {
	/// \brief [[BPR-2033]], RFC-BPR-0112. A companion's request and answer
	/// files under the profile (`workflow-exchange/DRAFT/INVOCATION/`), owner
	/// private. Nothing here starts a program: the author runs it.
	class local_draft_workflow_exchange final : public application::workflow_exchange_port
	{
	public:
		explicit local_draft_workflow_exchange(std::filesystem::path aProfileRoot);
		[[nodiscard]] auto locate(std::string_view aDraft, std::uint64_t aInvocation) const -> paths override;
		[[nodiscard]] auto write_request(const paths& aPaths, std::string_view aBytes) -> bool override;
		[[nodiscard]] auto read_response(const paths& aPaths, std::size_t aLimit) -> std::optional<std::string> override;

	private:
		std::filesystem::path Root;
	};

	/// \brief [[BPR-2032]]. The profile's workflows, agents and which bound
	/// programs are executable files now; read on the effect lane.
	[[nodiscard]] auto read_workflow_profile(const std::filesystem::path& aProfileRoot) -> application::local_draft_workflow_profile;
} // namespace bpr::app

#endif
