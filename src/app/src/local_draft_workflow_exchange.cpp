#include "local_draft_workflow_exchange.hpp"

#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/agent_configuration_exchange.hpp"

#include <QFileInfo>
#include <QString>

#include <fstream>
#include <iterator>
#include <system_error>

namespace bpr::app {
	local_draft_workflow_exchange::local_draft_workflow_exchange(std::filesystem::path aProfileRoot)
		: Root{ std::move(aProfileRoot) / "workflow-exchange" }
	{ }

	auto local_draft_workflow_exchange::locate(const std::string_view aDraft, const std::uint64_t aInvocation) const -> paths
	{
		const auto folder = Root / std::filesystem::path{ std::string{ aDraft } } / std::to_string(aInvocation);
		return { .Request = (folder / "request.json").string(), .Response = (folder / "response.json").string() };
	}

	auto local_draft_workflow_exchange::write_request(const paths& aPaths, const std::string_view aBytes) -> bool
	{
		auto       error  = std::error_code{ };
		const auto folder = std::filesystem::path{ aPaths.Request }.parent_path();
		std::filesystem::create_directories(folder, error);
		if (error) {
			return false;
		}
		// Owner private: the request holds the draft's text.
		for (auto at = folder; at != Root.parent_path() && !at.empty(); at = at.parent_path()) {
			std::filesystem::permissions(at, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error);
		}
		auto file = std::ofstream{ aPaths.Request, std::ios::binary | std::ios::trunc };
		file.write(aBytes.data(), static_cast<std::streamsize>(aBytes.size()));
		file.close();
		std::filesystem::permissions(aPaths.Request, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, error);
		return file.good() && !error;
	}

	auto local_draft_workflow_exchange::read_response(const paths& aPaths, const std::size_t aLimit) -> std::optional<std::string>
	{
		auto file = std::ifstream{ aPaths.Response, std::ios::binary };
		if (!file) {
			return std::nullopt;
		}
		auto bytes = std::string(aLimit + 1U, '\0');
		file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		bytes.resize(static_cast<std::size_t>(file.gcount()));
		return bytes;
	}

	auto read_workflow_profile(const std::filesystem::path& aProfileRoot) -> application::local_draft_workflow_profile
	{
		auto profile = application::local_draft_workflow_profile{ };
		auto store   = adapters::sqlite::make_agent_configuration_store(aProfileRoot);
		auto stored  = store ? store->load() : std::nullopt;
		if (!stored) {
			return profile;
		}
		for (const auto& entry : stored->Agents.Agents) {
			profile.Agents.push_back(entry.Name);
		}
		for (const auto& workflow : stored->Workflows) {
			for (const auto& program : workflow.Programs) {
				const auto info = QFileInfo{ QString::fromStdString(program.Program) };
				if (info.isFile() && info.isExecutable()) {
					profile.Programs.push_back(program.Program);
				}
			}
		}
		profile.Workflows = std::move(stored->Workflows);
		return profile;
	}
} // namespace bpr::app
