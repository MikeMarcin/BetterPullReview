#ifndef BPR_APPLICATION_TOUR_OWNER_CHANNEL_HPP
#define BPR_APPLICATION_TOUR_OWNER_CHANNEL_HPP
#include "bpr/application/review_tour_host.hpp"
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
													 hooks_show };
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
	/// \brief Finite local-only command. No editor, provider or publication authority crosses here.
	struct tour_owner_request
	{
		tour_owner_operation    Operation = tour_owner_operation::load;
		std::string             Draft;
		tour_service_request    Edit;
		tour_owner_assist_query Assist{ };
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
	};
	enum class tour_channel_failure : std::uint8_t { unavailable,
													 refused,
													 cancelled,
													 indeterminate };
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
