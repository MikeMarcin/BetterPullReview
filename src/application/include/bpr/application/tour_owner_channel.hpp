#ifndef BPR_APPLICATION_TOUR_OWNER_CHANNEL_HPP
#define BPR_APPLICATION_TOUR_OWNER_CHANNEL_HPP
#include "bpr/application/review_tour_host.hpp"
#include "bpr/core/review_cycle_command.hpp"
#include "bpr/core/review_preparation.hpp"
#include <functional>
#include <span>
namespace bpr::application {
	inline constexpr std::size_t maximum_tour_channel_bytes = 5U * 1024U * 1024U;
	enum class tour_owner_operation : std::uint8_t { load,
													 mutate,
													 request_status,
													 assist_issue,
													 assist_cancel,
													 hooks_show,
													 /// [[BPR-1168]]. One finite `bpr cycle` command for the owner's cycle store.
													 cycle };
	/// \brief What to write, chosen by the author or their agent. The owner resolves it and reads the draft
	/// and hook itself; no context, range offsets beyond these or target ever arrive from the caller.
	struct tour_owner_assist_query
	{
		core::writing_hook Hook = core::writing_hook::problem;
		core::tour_stop_id Stop;
		bool               HasRange = false, HasQuote = false;
		std::uint64_t      Begin = 0, End = 0;
		std::string        Quote, Session;
	};
	/// \brief Most bytes of a text the caller already read for a cycle command.
	inline constexpr std::size_t maximum_cycle_owner_text_bytes = 64U * 1024U;
	/// \brief Most bytes of a response document the caller read from its standard input.
	inline constexpr std::size_t maximum_cycle_owner_document_bytes = 4U * 1024U * 1024U;
	/// \brief [[BPR-1168]]. One `bpr cycle` command a terminal forwards to the
	/// profile owner: a name from the finite registry and its typed arguments,
	/// with every path already absolute and every chosen input already read.
	/// \details The owner resolves the cycle and runs the command on its own
	/// store; the caller names no store, SQL, process or provider.
	struct cycle_owner_request
	{
		core::cycle_command Command = core::cycle_command::show;
		/// The caller's operation identity; with `Step`, it names this request.
		std::string Operation;
		/// Which of the caller's requests under that identity this is.
		std::uint64_t Step = 0;
		/// The cycle (RC-n) or, for `open`, the absolute location.
		std::vector<std::string> Positional;
		/// Options from the command's registry entry, as typed.
		std::vector<std::pair<std::string, std::string>> Options;
		/// The caller's absolute working folder, used only to find the one
		/// cycle bound to it when none is named.
		std::string Directory;
		/// Note or goal text the caller read from a file, its standard input or its editor.
		std::optional<std::string> Text;
		/// A response document the caller read from its standard input.
		std::optional<std::string> Document;
		/// Check a note's target without writing it.
		bool Check = false;
		/// A person at the caller's terminal, who sees each draft change's result.
		bool Interactive = false;
	};
	/// \brief The owner's answer: the command's own result, as the terminal shows it.
	struct cycle_owner_response
	{
		std::string   Operation;
		std::uint64_t Step = 0;
		bool          Ok   = false;
		/// A failure's stable code, words, next command and detail (compact JSON).
		std::string Code, Message, Next, Detail;
		/// Success: the data (compact JSON) and the words a person reads.
		std::string Data, Human;
		bool        Complete = true;
		/// Exported bytes the caller writes to its own standard output.
		std::string Output;
	};
	/// \brief Finite local-only command. No editor, provider or publication authority crosses here.
	struct tour_owner_request
	{
		tour_owner_operation    Operation = tour_owner_operation::load;
		std::string             Draft;
		tour_service_request    Edit;
		tour_owner_assist_query Assist{ };
		cycle_owner_request     Cycle{ };
	};
	struct tour_owner_response
	{
		std::optional<tour_saved_target>    Target;
		std::optional<core::review_tour>    Draft;
		std::string                         UndoLabel, RedoLabel;
		tour_resume_hint                    Resume;
		std::optional<tour_request_receipt> Receipt;
		std::string                         Error;
		/// Assist context, assist outcome or hooks, already rendered as one bounded JSON document.
		std::string Json;
		/// [[BPR-1168]]. The answer to a cycle command.
		std::optional<cycle_owner_response> Cycle;
	};
	enum class tour_channel_failure : std::uint8_t { unavailable,
													 refused,
													 cancelled,
													 indeterminate,
													 /// The owner speaks another protocol version; nothing was sent.
													 version_mismatch };
	using tour_channel_bytes = std::vector<std::uint8_t>;
	/// \brief One bounded response on the effect lane. Destruction without completion is uncertain.
	class tour_owner_reply
	{
	public:
		virtual ~tour_owner_reply()                                             = default;
		virtual void complete(std::span<const std::uint8_t> aResponse) noexcept = 0;
	};
	struct tour_owner_message
	{
		tour_channel_bytes                Payload;
		std::shared_ptr<tour_owner_reply> Reply;
	};
	/// \brief Native transport polled only on the owner's effect lane, retained through shutdown.
	class tour_owner_server
	{
	public:
		virtual ~tour_owner_server()                                = default;
		virtual auto receive() -> std::optional<tour_owner_message> = 0;
	};
	/// \brief Answers one admitted request on the owner's store lane. Empty output is an uncertain close.
	using tour_owner_handler = std::function<tour_channel_bytes(std::span<const std::uint8_t>)>;
	/// \brief Advances the native transport by one bounded step and answers a complete request.
	/// Runs on the effect lane; a handler failure closes the connection without a reply.
	inline auto serve_tour_owner_once(tour_owner_server& aServer, const tour_owner_handler& aHandler) -> bool
	{
		auto message = aServer.receive();
		if (!message) {
			return false;
		}
		tour_channel_bytes response;
		try {
			response = aHandler(message->Payload);
		} catch (...) {
			response.clear();
		}
		message->Reply->complete(response);
		return true;
	}
}
#endif
