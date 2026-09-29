#ifndef BPR_APPLICATION_TOUR_ASSIST_HPP
#define BPR_APPLICATION_TOUR_ASSIST_HPP
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/review_preparation.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
namespace bpr::application {
	inline constexpr std::size_t          maximum_tour_assist_tickets   = 4;
	inline constexpr std::size_t          maximum_tour_assist_proposals = 4;
	inline constexpr std::size_t          maximum_assist_sources        = 8;
	inline constexpr std::size_t          maximum_assist_session_bytes  = 64;
	inline constexpr std::chrono::seconds assist_lifetime{ 1800 };
	/// \brief One bounded pending request for text from the active agent, owned by the draft's journal.
	/// The agent supplies replacement text only. It never names the target, the range or the hook: those
	/// are fixed here at issue and checked again at completion.
	struct tour_assist_ticket
	{
		std::uint64_t                Id   = 0;
		core::writing_hook           Hook = core::writing_hook::problem;
		core::tour_stop_id           Stop;
		core::writing_scope          Scope = core::writing_scope::field;
		std::uint64_t                Begin = 0, End = 0;
		std::uint64_t                DraftRevision = 0, HookRevision = 0;
		std::array<std::uint8_t, 32> RawDigest{ }, ContextDigest{ };
		std::vector<writing_source>  Sources;
		std::int64_t                 Expires = 0;
		std::string                  Session;
		auto                         operator==(const tour_assist_ticket&) const -> bool = default;
	};
	/// \brief A body-free outline proposal the author may insert in part. No ready bit and no comment text.
	struct tour_assist_proposal
	{
		std::uint64_t                         Id = 0, DraftRevision = 0;
		std::vector<core::tour_outline_entry> Entries;
		std::int64_t                          Expires                                               = 0;
		auto                                  operator==(const tour_assist_proposal&) const -> bool = default;
	};
	[[nodiscard]] inline auto valid(const tour_assist_ticket& aTicket) -> bool
	{
		if (aTicket.Id == 0 || aTicket.Hook > core::writing_hook::comment || aTicket.Begin > aTicket.End || aTicket.DraftRevision == 0 || aTicket.HookRevision == 0
			|| aTicket.Session.size() > maximum_assist_session_bytes || aTicket.Sources.size() > maximum_assist_sources
			|| (aTicket.Hook == core::writing_hook::comment) != (aTicket.Stop.value() != 0) || aTicket.Scope > core::writing_scope::empty_field) {
			return false;
		}
		std::size_t total = 0;
		for (const auto& source : aTicket.Sources) {
			total += source.Text.size();
			if (source.Id.empty() || source.Id.size() > 128 || source.Text.size() > 4096 || total > 8192 || !core::valid_writing_utf8(source.Id) || !core::valid_writing_utf8(source.Text)) {
				return false;
			}
		}
		return core::valid_writing_utf8(aTicket.Session);
	}
	[[nodiscard]] inline auto valid(const tour_assist_proposal& aProposal) -> bool
	{
		if (aProposal.Id == 0 || aProposal.DraftRevision == 0 || aProposal.Entries.empty() || aProposal.Entries.size() > 24) {
			return false;
		}
		return std::ranges::all_of(aProposal.Entries, [](const auto& aEntry) {
			return !aEntry.Title.empty() && aEntry.Title.size() <= 180 && aEntry.Related.size() <= 8 && core::valid_writing_utf8(aEntry.Title) && aEntry.Suggestion.Reason.size() <= 240 && aEntry.Suggestion.Question.size() <= 240
				&& aEntry.Suggestion.AnchorRef.size() <= 128;
		});
	}
}
#endif
