#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP

#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief One current file receipt supplied to context construction.
	struct agent_file_context_input
	{
		std::string        Path;
		core::file_receipt Receipt;
		bool               ChangedSinceReview = false;

		auto operator<=>(const agent_file_context_input&) const = default;
	};

	/// \brief Exact identity and revision of one explicitly selected draft.
	struct agent_draft_choice
	{
		core::review_draft_id Id;
		std::uint64_t         Revision = 0U;

		auto operator<=>(const agent_draft_choice&) const = default;
	};

	/// \brief The current unpublished draft observed by the application host.
	///
	/// \details Prose remains in the host observation and never enters the Qt
	/// intent. The choice must match both identity and revision at dispatch.
	struct agent_draft_observation
	{
		agent_draft_choice         Choice;
		core::review_comment_draft Draft;

		auto operator<=>(const agent_draft_observation&) const = default;
	};

	/// \brief Bounded observations approved for one agent invocation.
	///
	/// \details `Private` is the BPR-0238 value supplied by the caller. This
	/// type owns no ledger and performs no load. Notes cross only when their IDs
	/// also appear in `IncludedNotes`; drafts are explicit input values.
	struct agent_context_observation
	{
		core::application_session_id                     ApplicationSession;
		core::agent_revision_binding                     Binding;
		std::string                                      PullRequestUrl;
		std::optional<core::git_commit_oid>              PreviousUpload;
		std::vector<agent_file_context_input>            Files;
		private_ledger_observation                       Private;
		std::optional<core::supplement_semantic_context> Selection;
		std::optional<agent_draft_observation>           CurrentDraft;
		std::vector<local_note_id>                       IncludedNotes;
		std::optional<agent_draft_choice>                IncludedDraft;

		auto operator<=>(const agent_context_observation&) const = default;
	};

	enum class agent_context_failure : std::uint8_t {
		invalid_observation,
		selected_note_missing,
		duplicate_selected_note,
		selected_draft_missing,
		selected_draft_stale,
		selected_draft_invalid,
	};

	/// \brief Builds context only from caller-supplied bounded observations.
	[[nodiscard]] auto build_agent_context_supplement(
		const agent_context_observation& aObservation,
		const core::agent_session&       aSession)
		-> std::expected<core::agent_context_supplement, agent_context_failure>;

	/// \brief What one agent process is started with.
	struct agent_launch_request
	{
		core::agent_protocol     Protocol = core::agent_protocol::external_terminal;
		core::agent_session_id   Session;
		std::string              Executable;
		std::vector<std::string> Arguments;
		/// Explicit accepted workspace; the launcher never inherits host cwd.
		std::string WorkingDirectory;
		/// Materialised from the profile's environment-name allowlist.
		std::vector<std::pair<std::string, std::string>> Environment;
		/// Initial protocol input; complete context for non-App-Server modes.
		std::string Input;
		/// App Server context held until its initialized thread is identified.
		std::string DeferredContext;

		auto operator<=>(const agent_launch_request&) const = default;
	};

	using environment_reader
		= std::function<std::optional<std::string>(const std::string&)>;

	[[nodiscard]] auto materialize_agent_environment(
		const core::agent_profile& aProfile,
		const environment_reader&  aParent)
		-> std::vector<std::pair<std::string, std::string>>;

	enum class agent_launch_failure : std::uint8_t {
		invalid_profile,
		profile_session_mismatch,
		unsupported_supplement_schema,
	};

	/// \brief Builds one protocol-specific process request.
	[[nodiscard]] auto build_agent_launch(
		const core::agent_profile&            aProfile,
		const core::agent_context_supplement& aSupplement,
		const environment_reader&             aParent)
		-> std::expected<agent_launch_request, agent_launch_failure>;

	/// \brief Renders every supplement field with explicit count/presence data.
	[[nodiscard]] auto encode_agent_supplement(
		const core::agent_context_supplement& aSupplement) -> std::string;

	inline constexpr std::size_t maximum_agent_stdout_bytes = 1U * 1024U * 1024U;
	inline constexpr std::size_t maximum_agent_stderr_bytes = 1U * 1024U * 1024U;
	inline constexpr std::size_t maximum_agent_frame_bytes  = 64U * 1024U;

	enum class agent_protocol_failure : std::uint8_t {
		malformed_frame,
		incomplete_frame,
		frame_too_large,
		output_too_large,
		remote_failed,
		remote_interrupted,
		causal_violation,
	};

	enum class agent_protocol_event_kind : std::uint8_t {
		output,
		initialization_complete,
		context_channel_ready,
	};

	struct agent_protocol_event
	{
		agent_protocol_event_kind Kind = agent_protocol_event_kind::output;
		std::string               Text;

		auto operator<=>(const agent_protocol_event&) const = default;
	};

	/// \brief Builds the next App Server message after a decoded control event.
	[[nodiscard]] auto build_agent_protocol_followup(
		const agent_launch_request& aRequest,
		const agent_protocol_event& aControl)
		-> std::expected<std::string, agent_protocol_failure>;

	/// \brief Incrementally decodes one selected protocol's stdout frames.
	class agent_protocol_decoder final
	{
	public:
		explicit agent_protocol_decoder(core::agent_protocol aProtocol);

		[[nodiscard]] auto push(std::string_view aBytes)
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>;
		[[nodiscard]] auto finish()
			-> std::expected<std::vector<agent_protocol_event>, agent_protocol_failure>;
		/// \brief Records that the host completely wrote one App Server follow-up.
		[[nodiscard]] auto acknowledge_followup(agent_protocol_event_kind aControl)
			-> std::expected<void, agent_protocol_failure>;

	private:
		core::agent_protocol Protocol;
		std::string          Pending;
		std::size_t          TotalBytes   = 0U;
		bool                 Failed       = false;
		std::uint8_t         Phase        = 0U;
		bool                 SawAssistant = false;
		std::string          RemoteSession;
		std::string          RemoteThread;
		std::string          RemoteTurn;
	};

	/// \brief Every terminal condition the host exposes distinctly.
	enum class agent_run_outcome : std::uint8_t {
		success,
		failed_to_start,
		nonzero_exit,
		protocol_failure,
		cancelled,
		timeout,
		stale_result,
	};

	[[nodiscard]] auto name(agent_run_outcome aOutcome) noexcept -> std::string_view;

	struct agent_stream_event
	{
		core::agent_session_id Session;
		std::uint64_t          Sequence = 0U;
		std::string            Text;

		auto operator<=>(const agent_stream_event&) const = default;
	};

	struct agent_run_result
	{
		core::agent_session_id          Session;
		agent_run_outcome               Outcome = agent_run_outcome::failed_to_start;
		std::vector<agent_stream_event> Events;
		std::string                     StandardError;
		std::optional<int>              ExitCode;
		/// Exact decoder cause when `Outcome` is `protocol_failure`.
		std::optional<agent_protocol_failure> ProtocolFailure;

		auto operator<=>(const agent_run_result&) const = default;
	};

	using agent_output_sink = std::function<void(const agent_stream_event&)>;

	/// \brief Process effect boundary used by the agent application host.
	class agent_process_port
	{
	public:
		virtual ~agent_process_port() = default;

		[[nodiscard]] virtual auto run(
			const agent_launch_request& aRequest,
			int                         aTimeoutMilliseconds,
			const agent_output_sink&    aSink = { },
			std::stop_token             aStop = { }) -> agent_run_result = 0;
		virtual void cancel() noexcept                                   = 0;
	};

	/// \brief Applies the core's immutable session decision to a process result.
	[[nodiscard]] auto finalize_agent_run(
		agent_run_result                    aResult,
		const core::agent_session&          aSession,
		const core::agent_revision_binding& aObserved) -> agent_run_result;
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_LAUNCH_HPP
