#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_REVIEW_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PRIVATE_REVIEW_PROJECTION_HPP
#include "bpr/core/review_composition.hpp"
#include <functional>
namespace bpr::application {
	/// Bounded presentation values. No store, process or publication capability crosses into Qt.
	struct private_thread_row
	{
		core::private_review_thread        Thread;
		core::private_subject_availability Availability                                        = core::private_subject_availability::unplaceable;
		auto                               operator==(const private_thread_row&) const -> bool = default;
	};
	struct private_turn_row
	{
		core::private_review_revision      Revision;
		core::private_source_selection     Selection;
		core::private_subject_availability Availability  = core::private_subject_availability::unplaceable;
		std::uint64_t                      BodyBytes     = 0;
		bool                               BodyContinues = false;
		std::string                        Lineage;
		auto                               operator==(const private_turn_row&) const -> bool = default;
	};
	struct private_conversation_projection
	{
		std::uint64_t                                 Generation = 0;
		bool                                          Available  = false;
		bool                                          Busy       = false;
		std::vector<private_thread_row>               Threads;
		std::optional<core::private_review_thread_id> SelectedThread;
		std::vector<private_turn_row>                 Turns;
		bool                                          MoreThreads = false;
		bool                                          MoreTurns   = false;
		std::string                                   Status;
	};
	enum class private_conversation_action : std::uint8_t { refresh,
															next_threads,
															select_thread,
															next_turns,
															read_body,
															provenance,
															create_pr,
															create_selected,
															reply,
															edit,
															archive,
															erase,
															compose,
															ask_agent };
	struct private_conversation_intent
	{
		private_conversation_action                   Action     = private_conversation_action::refresh;
		std::uint64_t                                 Generation = 0;
		std::optional<core::private_review_thread_id> Thread;
		std::uint64_t                                 ExpectedVersion = 0;
		std::optional<core::private_source_selection> Revision;
		std::vector<core::private_source_selection>   Sources;
		std::string                                   Body;
	};
	using private_conversation_dispatch = std::function<void(private_conversation_intent)>;
}
#endif
