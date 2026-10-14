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
	struct private_draft_row
	{
		core::review_draft_id    Id;
		core::source_line_ref    Anchor;
		core::review_draft_state State;
		std::string              Head = { }, Path = { }, Body = { };
		bool                     Current  = false;
		bool                     Unused   = false;
		std::uint64_t            Revision = 0;
	};
	struct private_conversation_projection
	{
		std::uint64_t                                               Generation = 0;
		bool                                                        Available  = false;
		bool                                                        Busy       = false;
		std::vector<private_thread_row>                             Threads;
		std::optional<core::private_review_thread_id>               SelectedThread;
		std::uint64_t                                               SelectedVersion = 0;
		std::vector<private_turn_row>                               Turns;
		std::vector<core::private_source_observation>               Findings;
		std::vector<private_draft_row>                              Drafts;
		bool                                                        IncludeArchived = false;
		std::vector<std::pair<core::agent_profile_id, std::string>> Profiles;
		bool                                                        MoreFindings = false;
		bool                                                        MoreThreads  = false;
		bool                                                        MoreTurns    = false;
		std::string                                                 Status;
		std::optional<core::private_review_scope>                   SavedScope;
	};
	enum class private_conversation_action : std::uint8_t { refresh,
															clear_unused_drafts,
															discard_saved_draft,
															next_threads,
															next_findings,
															inspect_finding,
															include_archived,
															open_draft,
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
		core::agent_profile_id                        Profile;
		core::agent_setting_facts                     Requested;
		bool                                          IncludeArchived = false;
		std::optional<core::review_draft_id>          Draft;
		std::string                                   Body;
		std::optional<core::private_review_scope>     SavedScope;
	};
	using private_conversation_dispatch = std::function<void(private_conversation_intent)>;

	struct private_composition_projection
	{
		std::uint64_t                                               Generation = 0;
		std::vector<core::private_source_observation>               Sources;
		std::vector<core::private_composition_target>               Targets;
		std::vector<core::review_composition_state>                 Workspaces;
		std::optional<core::review_composition_state>               Current;
		std::uint64_t                                               Version = 0;
		std::optional<core::review_composition_revision>            Candidate;
		std::vector<std::pair<core::agent_profile_id, std::string>> Profiles;
		bool                                                        Busy = false;
		std::string                                                 Status;
	};
	enum class private_composition_action : std::uint8_t { create,
														   split,
														   select,
														   revise,
														   target,
														   ask_agent,
														   accept,
														   cancel };
	struct private_composition_intent
	{
		private_composition_action                      Action     = private_composition_action::create;
		std::uint64_t                                   Generation = 0;
		core::review_composition_mode                   Mode       = core::review_composition_mode::reviewer_authored;
		std::optional<core::review_composition_id>      Composition;
		std::uint64_t                                   ExpectedVersion = 0;
		std::optional<core::private_source_reference>   Candidate;
		std::optional<core::private_composition_target> Target;
		core::agent_profile_id                          Profile;
		core::agent_setting_facts                       Requested;
		std::string                                     Body;
	};
	using private_composition_dispatch = std::function<void(private_composition_intent)>;
}
#endif
