#ifndef BPR_APP_TOUR_RUNTIME_HPP
#define BPR_APP_TOUR_RUNTIME_HPP
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/review_tour_host.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
namespace bpr::app {
	struct tour_cli_error
	{
		std::string Code, Message;
		int         Exit = 7;
	};
	template <typename T>
	using tour_cli_result = std::expected<T, tour_cli_error>;
	struct tour_remote_read
	{
		application::tour_saved_target            Target;
		application::github_pull_request_snapshot Snapshot;
	};
	struct tour_source_material
	{
		core::tour_source_range Source;
		std::string             Text;
	};

	/// \brief Narrow headless read composition; no mutation or model capability exists.
	class tour_remote_reader
	{
	public:
		virtual auto source(const core::tour_code_destination&) -> tour_cli_result<tour_source_material> { return std::unexpected(tour_cli_error{ "source_unavailable", "This read provider cannot load pinned source.", 5 }); }
		virtual ~tour_remote_reader()                                                                                                                                    = default;
		virtual auto connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string>                                                         = 0;
		virtual auto open(std::string aHostname, core::repository_locator aRepository, core::pull_request_number aNumber, bool aGh) -> tour_cli_result<tour_remote_read> = 0;
	};
	using tour_reader_factory = std::function<std::unique_ptr<tour_remote_reader>(std::filesystem::path, core::bpr_profile_id)>;
	struct tour_local_snapshot
	{
		application::tour_saved_target Target;
		core::review_tour              Draft;
		std::string                    UndoLabel, RedoLabel;
	};
	/// \brief One short profile-owned transaction. Destroy before waiting for human input.
	class tour_runtime final
	{
	public:
		tour_runtime(std::filesystem::path aBase, std::string aProfile, tour_reader_factory aReaders = { });
		~tour_runtime();
		[[nodiscard]] auto        admission() const -> tour_cli_result<core::bpr_profile_id>;
		[[nodiscard]] auto        connect(std::string aHostname, bool aGh, bool aInteractive) -> tour_cli_result<std::string>;
		[[nodiscard]] auto        open(std::string aHostname, core::repository_locator aRepository, core::pull_request_number aNumber, bool aGh, std::string aRoot, bool aOffline) -> tour_cli_result<tour_local_snapshot>;
		[[nodiscard]] auto        load(std::string_view aDraft, bool aMachine) -> tour_cli_result<tour_local_snapshot>;
		[[nodiscard]] auto        source(std::string_view aDraft, const core::tour_code_destination& aDestination, bool aGh) -> tour_cli_result<tour_source_material>;
		[[nodiscard]] auto        mutate(std::string_view aDraft, application::tour_service_request aRequest) -> tour_cli_result<tour_local_snapshot>;
		[[nodiscard]] static auto draft_token(const application::tour_saved_target& aTarget) -> std::string;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
	[[nodiscard]] auto make_tour_remote_reader(std::filesystem::path, core::bpr_profile_id) -> std::unique_ptr<tour_remote_reader>;
}
#endif
