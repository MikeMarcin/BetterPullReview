#include "bpr/core/local_draft_tour.hpp"

#include "bpr/core/hex.hpp"
#include "bpr/core/local_draft.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <limits>
#include <ranges>
#include <set>
#include <utility>

namespace bpr::core::preparation_wire {
	struct binding
	{
		std::uint64_t Candidate = 0;
		std::string   Path;
		std::string   Side;
		std::uint32_t First = 0;
		std::uint32_t Last  = 0;
		std::string   Excerpt;
	};
	struct destination
	{
		std::uint64_t Candidate = 0;
		std::string   Correspondence;
		std::uint32_t First = 0;
		std::uint32_t Last  = 0;
	};
	struct stop
	{
		std::uint64_t        Id = 0;
		std::string          Title;
		std::string          Text;
		binding              Source;
		std::vector<binding> Earlier;
		/// Written before RFC-BPR-0109 was deferred (2026-11-12): read and dropped, never written.
		std::optional<std::vector<std::uint64_t>> Citations;
		bool                                      Attention = false;
		destination                               Current;
		std::uint64_t                             Revision = 1;
		bool                                      Accepted = false;
		std::string                               Fingerprint;
	};
	struct proposal
	{
		std::uint64_t          Id = 0;
		std::string            Target;
		std::uint64_t          Conversation = 0;
		std::uint64_t          Candidate    = 0;
		std::uint64_t          Stop         = 0;
		std::uint64_t          Base         = 0;
		std::string            Title;
		std::string            Text;
		std::optional<binding> Source;
		/// Written before RFC-BPR-0109 was deferred (2026-11-12): read and dropped, never written.
		std::optional<std::string> Claim;
		std::optional<std::string> Method;
		std::string                Producer;
		std::string                Version;
		std::string                State;
	};
	struct turn
	{
		bool                         Author = true;
		std::string                  Subject;
		std::string                  Text;
		std::vector<std::uint64_t>   Proposals;
		std::optional<std::uint64_t> Cost;
		std::int64_t                 At = 0;
	};
	struct conversation
	{
		std::uint64_t     Id        = 0;
		std::uint64_t     Candidate = 0;
		std::string       Thread;
		std::string       Agent;
		std::string       AgentName;
		std::string       Handle;
		std::vector<turn> Turns;
		bool              Ended = false;
	};
	struct preparation
	{
		std::string               Body;
		std::uint64_t             BodyRevision  = 1;
		std::uint64_t             TitleRevision = 1;
		std::string               Problem;
		std::string               Solution;
		std::uint64_t             ProblemRevision  = 1;
		std::uint64_t             SolutionRevision = 1;
		std::vector<stop>         Stops;
		std::uint64_t             NextStop = 1;
		std::vector<proposal>     Proposals;
		std::uint64_t             NextProposal = 1;
		std::vector<conversation> Conversations;
		std::uint64_t             NextConversation = 1;
	};
	struct answer_item
	{
		std::string   Target;
		std::uint64_t Stop = 0;
		std::string   Title;
		std::string   Text;
		std::string   Path;
		std::uint32_t First = 0;
		std::uint32_t Last  = 0;
	};
} // namespace bpr::core::preparation_wire

namespace pw = bpr::core::preparation_wire;

template <>
struct glz::meta<pw::answer_item>
{
	using value_type            = pw::answer_item;
	static constexpr auto value = object("target", &value_type::Target, "stop", &value_type::Stop, "title", &value_type::Title, "text", &value_type::Text, "path", &value_type::Path,
										 "first", &value_type::First, "last", &value_type::Last);
};
template <>
struct glz::meta<pw::binding>
{
	using value_type            = pw::binding;
	static constexpr auto value = object("candidate", &value_type::Candidate, "path", &value_type::Path, "side", &value_type::Side, "first", &value_type::First, "last", &value_type::Last, "excerpt", &value_type::Excerpt);
};
template <>
struct glz::meta<pw::destination>
{
	using value_type            = pw::destination;
	static constexpr auto value = object("candidate", &value_type::Candidate, "correspondence", &value_type::Correspondence, "first", &value_type::First, "last", &value_type::Last);
};
template <>
struct glz::meta<pw::stop>
{
	using value_type            = pw::stop;
	static constexpr auto value = object("id", &value_type::Id, "title", &value_type::Title, "text", &value_type::Text, "source", &value_type::Source, "earlier", &value_type::Earlier, "citations", &value_type::Citations, "attention",
										 &value_type::Attention, "current", &value_type::Current, "revision", &value_type::Revision, "accepted", &value_type::Accepted, "fingerprint", &value_type::Fingerprint);
};
template <>
struct glz::meta<pw::proposal>
{
	using value_type            = pw::proposal;
	static constexpr auto value = object("id", &value_type::Id, "target", &value_type::Target, "conversation", &value_type::Conversation, "candidate", &value_type::Candidate, "stop", &value_type::Stop, "base", &value_type::Base, "title",
										 &value_type::Title, "text", &value_type::Text, "source", &value_type::Source, "claim", &value_type::Claim, "method", &value_type::Method, "producer", &value_type::Producer, "version", &value_type::Version,
										 "state", &value_type::State);
};
template <>
struct glz::meta<pw::turn>
{
	using value_type            = pw::turn;
	static constexpr auto value = object("author", &value_type::Author, "subject", &value_type::Subject, "text", &value_type::Text, "proposals", &value_type::Proposals, "cost", &value_type::Cost, "at", &value_type::At);
};
template <>
struct glz::meta<pw::conversation>
{
	using value_type            = pw::conversation;
	static constexpr auto value = object("id", &value_type::Id, "candidate", &value_type::Candidate, "thread", &value_type::Thread, "agent", &value_type::Agent, "agent_name", &value_type::AgentName, "handle", &value_type::Handle, "turns",
										 &value_type::Turns, "ended", &value_type::Ended);
};
template <>
struct glz::meta<pw::preparation>
{
	using value_type            = pw::preparation;
	static constexpr auto value = object("body", &value_type::Body, "body_revision", &value_type::BodyRevision, "title_revision", &value_type::TitleRevision, "problem", &value_type::Problem, "solution", &value_type::Solution,
										 "problem_revision", &value_type::ProblemRevision, "solution_revision", &value_type::SolutionRevision, "stops", &value_type::Stops, "next_stop", &value_type::NextStop, "proposals",
										 &value_type::Proposals, "next_proposal", &value_type::NextProposal, "conversations", &value_type::Conversations, "next_conversation", &value_type::NextConversation);
};

namespace bpr::core {
	namespace {
		constexpr auto read_options = glz::opts{ .error_on_unknown_keys = true };

		constexpr auto all_correspondences = std::array{ local_correspondence::exact, local_correspondence::changed, local_correspondence::ambiguous, local_correspondence::missing,
														 local_correspondence::unavailable };
		constexpr auto all_targets         = std::array{ local_proposal_target::title, local_proposal_target::description, local_proposal_target::problem,
														 local_proposal_target::solution, local_proposal_target::stop, local_proposal_target::new_stop };
		constexpr auto all_states          = std::array{ local_proposal_state::pending, local_proposal_state::accepted, local_proposal_state::rejected, local_proposal_state::outside_candidate };
		constexpr auto all_subjects        = std::array{ local_subject::whole_change, local_subject::title, local_subject::description, local_subject::problem,
														 local_subject::solution, local_subject::stop, local_subject::lines };

		template <typename TEnum, std::size_t TCount>
		[[nodiscard]] auto enum_from(const std::string_view aName, const std::array<TEnum, TCount>& aValues) -> std::optional<TEnum>
		{
			const auto found = std::ranges::find_if(aValues, [aName](const TEnum aValue) { return name(aValue) == aName; });
			return found == aValues.end() ? std::nullopt : std::optional{ *found };
		}

		[[nodiscard]] auto text_ok(const std::string_view aText, const std::size_t aLimit) -> bool
		{
			return aText.size() <= aLimit && utf8::valid(aText) && std::ranges::none_of(aText, [](const char aByte) { return aByte == '\0'; });
		}

		[[nodiscard]] auto line_ok(const std::string_view aText, const std::size_t aLimit) -> bool
		{
			return text_ok(aText, aLimit) && std::ranges::none_of(aText, [](const char aByte) {
					   const auto byte = static_cast<unsigned char>(aByte);
					   return byte < 0x20U || byte == 0x7FU;
				   });
		}

		[[nodiscard]] auto path_ok(const std::string_view aPath) -> bool
		{
			if (aPath.empty() || aPath.size() > maximum_tour_path_bytes || aPath.front() == '/' || aPath.back() == '/' || !utf8::valid(aPath)) {
				return false;
			}
			for (const auto part : std::views::split(aPath, '/')) {
				const auto segment = std::string_view{ part.begin(), part.end() };
				if (segment.empty() || segment == "." || segment == "..") {
					return false;
				}
			}
			return std::ranges::none_of(aPath, [](const char aByte) { return aByte == '\0' || aByte == '\n'; });
		}

		[[nodiscard]] auto split_lines(const std::string_view aText) -> std::vector<std::string_view>
		{
			auto lines = std::vector<std::string_view>{ };
			auto begin = std::size_t{ 0U };
			while (begin < aText.size()) {
				const auto end  = aText.find('\n', begin);
				const auto stop = end == std::string_view::npos ? aText.size() : end + 1U;
				lines.push_back(aText.substr(begin, stop - begin));
				begin = stop;
			}
			return lines;
		}

		/// A line without its terminator: the last line of a file may lack one.
		[[nodiscard]] auto bare(const std::string_view aLine) noexcept -> std::string_view
		{
			auto line = aLine;
			if (line.ends_with('\n')) {
				line.remove_suffix(1U);
			}
			if (line.ends_with('\r')) {
				line.remove_suffix(1U);
			}
			return line;
		}

		[[nodiscard]] auto advanced(local_draft aDraft, const std::int64_t aNow) -> local_draft
		{
			++aDraft.Version;
			aDraft.Updated = std::max(aDraft.Updated, aNow);
			return aDraft;
		}

		[[nodiscard]] auto latest_ordinal(const local_draft& aDraft) noexcept -> std::uint64_t
		{
			const auto* latest = latest_candidate(aDraft);
			return latest == nullptr ? 0U : latest->Ordinal;
		}

		[[nodiscard]] auto find_stop(local_draft& aDraft, const std::uint64_t aStop) -> local_tour_stop*
		{
			const auto found = std::ranges::find(aDraft.Preparation.Tour.Stops, aStop, &local_tour_stop::Id);
			return found == aDraft.Preparation.Tour.Stops.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto find_stop(const local_draft& aDraft, const std::uint64_t aStop) -> const local_tour_stop*
		{
			const auto found = std::ranges::find(aDraft.Preparation.Tour.Stops, aStop, &local_tour_stop::Id);
			return found == aDraft.Preparation.Tour.Stops.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto fingerprint(const local_tour_stop& aStop) -> std::array<std::uint8_t, 32>
		{
			auto       stream = sha256_stream{ };
			const auto add    = [&stream](const std::string_view aText) {
				const auto size = std::to_string(aText.size()) + ":";
				stream.update({ reinterpret_cast<const std::uint8_t*>(size.data()), size.size() });
				stream.update({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() });
			};
			add(aStop.Title);
			add(aStop.Text);
			add(std::to_string(aStop.Source.Candidate));
			add(aStop.Source.Path);
			add(aStop.Source.Side == diff_side::left ? "left" : "right");
			add(std::to_string(aStop.Source.FirstLine) + "-" + std::to_string(aStop.Source.LastLine));
			add(aStop.Source.Excerpt);
			return stream.finish();
		}

		/// Binds lines of one candidate's file read by the host.
		[[nodiscard]] auto bind(const std::uint64_t aCandidate, const local_file_reading& aFile, const diff_side aSide, const std::uint32_t aFirst, const std::uint32_t aLast)
			-> std::optional<local_source_binding>
		{
			if (!path_ok(aFile.Path) || !aFile.Text || aFirst == 0U || aLast < aFirst) {
				return std::nullopt;
			}
			auto excerpt = local_excerpt(*aFile.Text, aFirst, aLast);
			if (!excerpt || excerpt->size() > maximum_local_excerpt_bytes) {
				return std::nullopt;
			}
			return local_source_binding{ .Candidate = aCandidate, .Path = aFile.Path, .Side = aSide, .FirstLine = aFirst, .LastLine = aLast, .Excerpt = std::move(*excerpt) };
		}

		[[nodiscard]] auto binding_ok(const local_source_binding& aBinding) -> bool
		{
			return aBinding.Candidate != 0U && path_ok(aBinding.Path) && aBinding.FirstLine != 0U && aBinding.LastLine >= aBinding.FirstLine
				&& text_ok(aBinding.Excerpt, maximum_local_excerpt_bytes);
		}

		void mark_accepted(local_tour_stop& aStop)
		{
			aStop.Accepted            = true;
			aStop.AcceptedFingerprint = fingerprint(aStop);
		}

		/// Rebinds a stop to where its lines are on the latest candidate,
		/// keeping the binding it replaces.
		void rebind_to_current(local_tour_stop& aStop)
		{
			if (aStop.Current.Candidate == aStop.Source.Candidate && aStop.Current.FirstLine == aStop.Source.FirstLine) {
				return;
			}
			auto next      = aStop.Source;
			next.Candidate = aStop.Current.Candidate;
			next.FirstLine = aStop.Current.FirstLine;
			next.LastLine  = aStop.Current.LastLine;
			if (aStop.Earlier.size() >= maximum_local_draft_candidates) {
				aStop.Earlier.erase(aStop.Earlier.begin() + 1);
			}
			aStop.Earlier.push_back(std::exchange(aStop.Source, std::move(next)));
		}

		[[nodiscard]] auto field_revision(const local_draft& aDraft, const local_proposal_target aTarget, const std::uint64_t aStop) -> std::optional<std::uint64_t>
		{
			const auto& preparation = aDraft.Preparation;
			switch (aTarget) {
			case local_proposal_target::title:       return preparation.Description.TitleRevision;
			case local_proposal_target::description: return preparation.Description.Revision;
			case local_proposal_target::problem:     return preparation.Tour.ProblemRevision;
			case local_proposal_target::solution:    return preparation.Tour.SolutionRevision;
			case local_proposal_target::stop:        {
				const auto* stop = find_stop(aDraft, aStop);
				return stop == nullptr ? std::nullopt : std::optional{ stop->Revision };
			}
			case local_proposal_target::new_stop: return std::uint64_t{ 0U };
			}
			return std::nullopt;
		}

		[[nodiscard]] auto thread_ok(const std::string_view aThread) -> bool
		{
			// The Tour panel's Problem and Solution have chats of their own.
			if (aThread == local_change_thread || aThread == "problem" || aThread == "solution") {
				return true;
			}
			constexpr auto prefix = std::string_view{ "stop:" };
			return aThread.starts_with(prefix) && aThread.size() > prefix.size() && aThread.size() <= prefix.size() + 20U
				&& std::ranges::all_of(aThread.substr(prefix.size()), [](const char aByte) { return aByte >= '0' && aByte <= '9'; });
		}

		[[nodiscard]] auto permalink(const local_publication_result& aResult, const std::string_view aPath, const std::uint32_t aFirst, const std::uint32_t aLast) -> std::string
		{
			auto path = std::string{ };
			for (const auto byte : aPath) {
				const auto value = static_cast<unsigned char>(byte);
				if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') || byte == '/' || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
					path.push_back(byte);
				} else {
					constexpr auto digits = std::string_view{ "0123456789ABCDEF" };
					path.push_back('%');
					path.push_back(digits[value >> 4U]);
					path.push_back(digits[value & 0x0FU]);
				}
			}
			auto link = "https://" + aResult.Host + "/" + aResult.Owner + "/" + aResult.Name + "/blob/" + std::string{ canonical_hex(aResult.Head).view() } + "/" + path + "#L"
				+ std::to_string(aFirst);
			if (aLast != aFirst) {
				link += "-L" + std::to_string(aLast);
			}
			return link;
		}
	} // namespace

	auto name(const local_correspondence aCorrespondence) noexcept -> std::string_view
	{
		switch (aCorrespondence) {
		case local_correspondence::exact:       return "exact";
		case local_correspondence::changed:     return "changed";
		case local_correspondence::ambiguous:   return "ambiguous";
		case local_correspondence::missing:     return "missing";
		case local_correspondence::unavailable: return "unavailable";
		}
		return "unavailable";
	}

	auto name(const local_proposal_target aTarget) noexcept -> std::string_view
	{
		switch (aTarget) {
		case local_proposal_target::title:       return "title";
		case local_proposal_target::description: return "description";
		case local_proposal_target::problem:     return "problem";
		case local_proposal_target::solution:    return "solution";
		case local_proposal_target::stop:        return "stop";
		case local_proposal_target::new_stop:    return "new_stop";
		}
		return "description";
	}

	auto name(const local_proposal_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case local_proposal_state::pending:           return "pending";
		case local_proposal_state::accepted:          return "accepted";
		case local_proposal_state::rejected:          return "rejected";
		case local_proposal_state::outside_candidate: return "outside_candidate";
		}
		return "pending";
	}

	auto name(const local_subject aSubject) noexcept -> std::string_view
	{
		switch (aSubject) {
		case local_subject::whole_change: return "whole_change";
		case local_subject::title:        return "title";
		case local_subject::description:  return "description";
		case local_subject::problem:      return "problem";
		case local_subject::solution:     return "solution";
		case local_subject::stop:         return "stop";
		case local_subject::lines:        return "lines";
		}
		return "whole_change";
	}

	auto name(const preparation_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case preparation_refusal::stale:             return "stale";
		case preparation_refusal::too_long:          return "too_long";
		case preparation_refusal::invalid:           return "invalid";
		case preparation_refusal::unknown:           return "unknown";
		case preparation_refusal::full:              return "full";
		case preparation_refusal::outside_candidate: return "outside_candidate";
		case preparation_refusal::proposal_stale:    return "proposal_stale";
		case preparation_refusal::decided:           return "decided";
		case preparation_refusal::lines_needed:      return "lines_needed";
		case preparation_refusal::text_needed:       return "text_needed";
		}
		return "invalid";
	}

	auto name(const local_stop_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case local_stop_reason::none:         return "none";
		case local_stop_reason::not_accepted: return "not_accepted";
		case local_stop_reason::edited:       return "edited";
		case local_stop_reason::changed:      return "changed";
		case local_stop_reason::ambiguous:    return "ambiguous";
		case local_stop_reason::missing:      return "missing";
		case local_stop_reason::unavailable:  return "unavailable";
		}
		return "none";
	}

	// --- Material -------------------------------------------------------------------

	auto local_line_count(const std::string_view aText) noexcept -> std::uint32_t
	{
		auto count = std::uint32_t{ 0U };
		for (const auto byte : aText) {
			count += byte == '\n' ? 1U : 0U;
		}
		return count + (!aText.empty() && !aText.ends_with('\n') ? 1U : 0U);
	}

	auto local_excerpt(const std::string_view aText, const std::uint32_t aFirst, const std::uint32_t aLast) -> std::optional<std::string>
	{
		const auto lines = split_lines(aText);
		if (aFirst == 0U || aLast < aFirst || aLast > lines.size()) {
			return std::nullopt;
		}
		auto excerpt = std::string{ };
		for (auto line = aFirst; line <= aLast; ++line) {
			excerpt += lines[line - 1U];
		}
		return excerpt;
	}

	auto correspond(const local_source_binding& aBinding, const std::uint64_t aCandidate, const local_file_reading& aFile) -> local_destination
	{
		auto destination = local_destination{ .Candidate = aCandidate };
		if (aBinding.Candidate == aCandidate && aFile.Path == aBinding.Path) {
			destination.Correspondence = local_correspondence::exact;
			destination.FirstLine      = aBinding.FirstLine;
			destination.LastLine       = aBinding.LastLine;
			return destination;
		}
		if (aFile.Path != aBinding.Path) {
			return destination;
		}
		if (aFile.Deleted) {
			destination.Correspondence = local_correspondence::missing;
			return destination;
		}
		if (!aFile.Text) {
			return destination;
		}
		const auto file   = split_lines(*aFile.Text);
		const auto wanted = split_lines(aBinding.Excerpt);
		const auto count  = wanted.size();
		const auto at     = [&](const std::size_t aStart) {
			for (std::size_t index = 0U; index < count; ++index) {
				if (bare(file[aStart + index]) != bare(wanted[index])) {
					return false;
				}
			}
			return true;
		};
		destination.Correspondence = local_correspondence::changed;
		if (count == 0U || count > file.size()) {
			return destination;
		}
		// The same lines where they were first: nothing is guessed.
		if (aBinding.FirstLine >= 1U && aBinding.FirstLine - 1U + count <= file.size() && at(aBinding.FirstLine - 1U)) {
			destination.Correspondence = local_correspondence::exact;
			destination.FirstLine      = aBinding.FirstLine;
			destination.LastLine       = aBinding.FirstLine + static_cast<std::uint32_t>(count) - 1U;
			return destination;
		}
		auto found = std::vector<std::size_t>{ };
		for (std::size_t start = 0U; start + count <= file.size() && found.size() < 2U; ++start) {
			if (at(start)) {
				found.push_back(start);
			}
		}
		if (found.size() == 1U) {
			destination.Correspondence = local_correspondence::exact;
			destination.FirstLine      = static_cast<std::uint32_t>(found.front()) + 1U;
			destination.LastLine       = destination.FirstLine + static_cast<std::uint32_t>(count) - 1U;
		} else if (found.size() > 1U) {
			destination.Correspondence = local_correspondence::ambiguous;
		}
		return destination;
	}

	auto local_session_scope(const std::string_view aDraft, const std::uint64_t aCandidate, const std::string_view aThread, const std::string_view aAgent) -> std::string
	{
		return "local-draft:" + std::string{ aDraft } + ":candidate:" + std::to_string(aCandidate) + ":" + std::string{ aThread } + ":" + std::string{ aAgent };
	}

	auto local_stop_thread(const std::uint64_t aStop) -> std::string { return "stop:" + std::to_string(aStop); }

	// --- Validity and stored form -------------------------------------------------------

	auto valid(const local_draft_preparation& aPreparation) -> bool
	{
		const auto& tour        = aPreparation.Tour;
		const auto& description = aPreparation.Description;
		if (!text_ok(description.Body, maximum_local_description_bytes) || description.Revision == 0U || description.TitleRevision == 0U
			|| !text_ok(tour.Problem, maximum_tour_field_bytes) || !text_ok(tour.Solution, maximum_tour_field_bytes) || tour.ProblemRevision == 0U || tour.SolutionRevision == 0U
			|| tour.Stops.size() > maximum_tour_stops || aPreparation.Proposals.size() > maximum_local_proposals
			|| aPreparation.Conversations.size() > maximum_local_conversations) {
			return false;
		}
		auto total = tour.Problem.size() + tour.Solution.size();
		auto stops = std::set<std::uint64_t>{ };
		for (const auto& stop : tour.Stops) {
			if (stop.Id == 0U || stop.Id >= tour.NextStop || !stops.insert(stop.Id).second || !line_ok(stop.Title, maximum_tour_field_bytes) || !text_ok(stop.Text, maximum_tour_field_bytes)
				|| !binding_ok(stop.Source) || stop.Earlier.size() > maximum_local_draft_candidates || stop.Revision == 0U
				|| !std::ranges::all_of(stop.Earlier, binding_ok)) {
				return false;
			}
			total += stop.Title.size() + stop.Text.size() + stop.Source.Excerpt.size();
		}
		if (total > maximum_tour_bytes) {
			return false;
		}
		auto proposals = std::set<std::uint64_t>{ };
		for (const auto& proposal : aPreparation.Proposals) {
			if (proposal.Id == 0U || proposal.Id >= aPreparation.NextProposal || !proposals.insert(proposal.Id).second || !line_ok(proposal.Title, maximum_tour_field_bytes)
				|| !text_ok(proposal.Text, maximum_local_description_bytes) || (proposal.Source && !binding_ok(*proposal.Source)) || !line_ok(proposal.Producer, maximum_local_producer_bytes)
				|| !line_ok(proposal.Version, maximum_local_producer_bytes)) {
				return false;
			}
		}
		auto conversations = std::set<std::uint64_t>{ };
		for (const auto& conversation : aPreparation.Conversations) {
			if (conversation.Id == 0U || conversation.Id >= aPreparation.NextConversation || !conversations.insert(conversation.Id).second || !thread_ok(conversation.Thread)
				|| conversation.Turns.size() > maximum_local_turns || !line_ok(conversation.Agent, 256U) || !line_ok(conversation.AgentName, 256U)
				|| !line_ok(conversation.Handle, 1024U)
				|| !std::ranges::all_of(conversation.Turns, [](const local_turn& aTurn) { return text_ok(aTurn.Text, maximum_local_turn_bytes); })) {
				return false;
			}
		}
		return true;
	}

	auto encode_draft_preparation(const local_draft_preparation& aPreparation) -> std::string
	{
		const auto binding = [](const local_source_binding& aBinding) {
			return pw::binding{ .Candidate = aBinding.Candidate,
								.Path      = aBinding.Path,
								.Side      = aBinding.Side == diff_side::left ? "left" : "right",
								.First     = aBinding.FirstLine,
								.Last      = aBinding.LastLine,
								.Excerpt   = aBinding.Excerpt };
		};
		auto wire = pw::preparation{ .Body             = aPreparation.Description.Body,
									 .BodyRevision     = aPreparation.Description.Revision,
									 .TitleRevision    = aPreparation.Description.TitleRevision,
									 .Problem          = aPreparation.Tour.Problem,
									 .Solution         = aPreparation.Tour.Solution,
									 .ProblemRevision  = aPreparation.Tour.ProblemRevision,
									 .SolutionRevision = aPreparation.Tour.SolutionRevision,
									 .NextStop         = aPreparation.Tour.NextStop,
									 .NextProposal     = aPreparation.NextProposal,
									 .NextConversation = aPreparation.NextConversation };
		for (const auto& stop : aPreparation.Tour.Stops) {
			auto entry = pw::stop{ .Id          = stop.Id,
								   .Title       = stop.Title,
								   .Text        = stop.Text,
								   .Source      = binding(stop.Source),
								   .Attention   = stop.Attention,
								   .Current     = { .Candidate      = stop.Current.Candidate,
													.Correspondence = std::string{ name(stop.Current.Correspondence) },
													.First          = stop.Current.FirstLine,
													.Last           = stop.Current.LastLine },
								   .Revision    = stop.Revision,
								   .Accepted    = stop.Accepted,
								   .Fingerprint = stop.Accepted ? lower_hex(stop.AcceptedFingerprint) : std::string{ } };
			for (const auto& earlier : stop.Earlier) {
				entry.Earlier.push_back(binding(earlier));
			}
			wire.Stops.push_back(std::move(entry));
		}
		for (const auto& proposal : aPreparation.Proposals) {
			wire.Proposals.push_back({ .Id           = proposal.Id,
									   .Target       = std::string{ name(proposal.Target) },
									   .Conversation = proposal.Conversation,
									   .Candidate    = proposal.Candidate,
									   .Stop         = proposal.Stop,
									   .Base         = proposal.BaseRevision,
									   .Title        = proposal.Title,
									   .Text         = proposal.Text,
									   .Source       = proposal.Source ? std::optional{ binding(*proposal.Source) } : std::nullopt,
									   .Producer     = proposal.Producer,
									   .Version      = proposal.Version,
									   .State        = std::string{ name(proposal.State) } });
		}
		for (const auto& conversation : aPreparation.Conversations) {
			auto entry = pw::conversation{ .Id        = conversation.Id,
										   .Candidate = conversation.Candidate,
										   .Thread    = conversation.Thread,
										   .Agent     = conversation.Agent,
										   .AgentName = conversation.AgentName,
										   .Handle    = conversation.Handle,
										   .Ended     = conversation.Ended };
			for (const auto& turn : conversation.Turns) {
				entry.Turns.push_back(
					{ .Author = turn.Author, .Subject = std::string{ name(turn.Subject) }, .Text = turn.Text, .Proposals = turn.Proposals, .Cost = turn.CostMicros, .At = turn.At });
			}
			wire.Conversations.push_back(std::move(entry));
		}
		auto text = std::string{ };
		if (glz::write_json(wire, text)) {
			return { };
		}
		return text;
	}

	auto decode_draft_preparation(const std::string_view aText) -> std::optional<local_draft_preparation>
	{
		auto wire = pw::preparation{ };
		if (glz::read<read_options>(wire, aText)) {
			return std::nullopt;
		}
		auto       failed  = false;
		const auto binding = [&failed](const pw::binding& aBinding) {
			if (aBinding.Side != "left" && aBinding.Side != "right") {
				failed = true;
			}
			return local_source_binding{ .Candidate = aBinding.Candidate,
										 .Path      = aBinding.Path,
										 .Side      = aBinding.Side == "left" ? diff_side::left : diff_side::right,
										 .FirstLine = aBinding.First,
										 .LastLine  = aBinding.Last,
										 .Excerpt   = aBinding.Excerpt };
		};
		auto preparation = local_draft_preparation{
			.Description      = { .Body = wire.Body, .Revision = wire.BodyRevision, .TitleRevision = wire.TitleRevision },
			.Tour             = { .Problem = wire.Problem, .Solution = wire.Solution, .ProblemRevision = wire.ProblemRevision, .SolutionRevision = wire.SolutionRevision, .NextStop = wire.NextStop },
			.NextProposal     = wire.NextProposal,
			.NextConversation = wire.NextConversation,
		};
		for (const auto& entry : wire.Stops) {
			const auto correspondence = enum_from(entry.Current.Correspondence, all_correspondences);
			if (!correspondence) {
				return std::nullopt;
			}
			auto stop = local_tour_stop{ .Id        = entry.Id,
										 .Title     = entry.Title,
										 .Text      = entry.Text,
										 .Source    = binding(entry.Source),
										 .Attention = entry.Attention,
										 .Current   = { .Candidate      = entry.Current.Candidate,
														.Correspondence = *correspondence,
														.FirstLine      = entry.Current.First,
														.LastLine       = entry.Current.Last },
										 .Revision  = entry.Revision,
										 .Accepted  = entry.Accepted };
			for (const auto& earlier : entry.Earlier) {
				stop.Earlier.push_back(binding(earlier));
			}
			if (entry.Accepted) {
				if (entry.Fingerprint.size() != 64U) {
					return std::nullopt;
				}
				for (std::size_t index = 0U; index < 32U; ++index) {
					const auto digit = [](const char aByte) -> int {
						return aByte >= '0' && aByte <= '9' ? aByte - '0' : aByte >= 'a' && aByte <= 'f' ? aByte - 'a' + 10
																										 : -1;
					};
					const auto high = digit(entry.Fingerprint[index * 2U]);
					const auto low  = digit(entry.Fingerprint[(index * 2U) + 1U]);
					if (high < 0 || low < 0) {
						return std::nullopt;
					}
					stop.AcceptedFingerprint[index] = static_cast<std::uint8_t>((high << 4) | low);
				}
			} else if (!entry.Fingerprint.empty()) {
				return std::nullopt;
			}
			preparation.Tour.Stops.push_back(std::move(stop));
		}
		// A claim or planned check proposed before RFC-BPR-0109 was deferred
		// (2026-11-12) has nothing left to enter: it is dropped, and so are
		// the turns' references to it.
		auto dropped = std::set<std::uint64_t>{ };
		for (const auto& entry : wire.Proposals) {
			if (entry.Target == "claim" || entry.Target == "check") {
				dropped.insert(entry.Id);
				continue;
			}
			const auto target = enum_from(entry.Target, all_targets);
			const auto state  = enum_from(entry.State, all_states);
			if (!target || !state) {
				return std::nullopt;
			}
			preparation.Proposals.push_back({ .Id           = entry.Id,
											  .Target       = *target,
											  .Conversation = entry.Conversation,
											  .Candidate    = entry.Candidate,
											  .Stop         = entry.Stop,
											  .BaseRevision = entry.Base,
											  .Title        = entry.Title,
											  .Text         = entry.Text,
											  .Source       = entry.Source ? std::optional{ binding(*entry.Source) } : std::nullopt,
											  .Producer     = entry.Producer,
											  .Version      = entry.Version,
											  .State        = *state });
		}
		for (const auto& entry : wire.Conversations) {
			auto conversation = local_conversation{
				.Id = entry.Id, .Candidate = entry.Candidate, .Thread = entry.Thread, .Agent = entry.Agent, .AgentName = entry.AgentName, .Handle = entry.Handle, .Ended = entry.Ended
			};
			for (const auto& turn : entry.Turns) {
				const auto subject = enum_from(turn.Subject, all_subjects);
				if (!subject) {
					return std::nullopt;
				}
				auto proposals = turn.Proposals;
				std::erase_if(proposals, [&dropped](const std::uint64_t aId) { return dropped.contains(aId); });
				conversation.Turns.push_back(
					{ .Author = turn.Author, .Subject = *subject, .Text = turn.Text, .Proposals = std::move(proposals), .CostMicros = turn.Cost, .At = turn.At });
			}
			preparation.Conversations.push_back(std::move(conversation));
		}
		if (failed || !valid(preparation)) {
			return std::nullopt;
		}
		return preparation;
	}

	// --- Edits -------------------------------------------------------------------------

	auto edit_description(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aTitle, std::string aBody, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aBody.size() > maximum_local_description_bytes || aTitle.size() > maximum_local_draft_title_bytes) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		if (aTitle.empty() || !line_ok(aTitle, maximum_local_draft_title_bytes) || !text_ok(aBody, maximum_local_description_bytes)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto  next        = advanced(aDraft, aNow);
		auto& description = next.Preparation.Description;
		if (next.Title != aTitle) {
			next.Title = std::move(aTitle);
			++description.TitleRevision;
		}
		if (description.Body != aBody) {
			description.Body = std::move(aBody);
			++description.Revision;
		}
		return next;
	}

	auto edit_tour_brief(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aProblem, std::string aSolution, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aProblem.size() > maximum_tour_field_bytes || aSolution.size() > maximum_tour_field_bytes) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		if (!text_ok(aProblem, maximum_tour_field_bytes) || !text_ok(aSolution, maximum_tour_field_bytes)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto  next = advanced(aDraft, aNow);
		auto& tour = next.Preparation.Tour;
		if (tour.Problem != aProblem) {
			tour.Problem = std::move(aProblem);
			++tour.ProblemRevision;
		}
		if (tour.Solution != aSolution) {
			tour.Solution = std::move(aSolution);
			++tour.SolutionRevision;
		}
		if (!valid(next.Preparation)) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		return next;
	}

	auto transfer_to_description(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aPassage, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aPassage.empty()) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto body = aDraft.Preparation.Description.Body;
		if (!body.empty() && !body.ends_with("\n\n")) {
			body += body.ends_with('\n') ? "\n" : "\n\n";
		}
		body += aPassage;
		return edit_description(aDraft, aExpectedVersion, aDraft.Title, std::move(body), aNow);
	}

	auto add_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aTitle, std::string aText, const local_file_reading& aFile, const diff_side aSide,
						const std::uint32_t aFirst, const std::uint32_t aLast, const std::int64_t aNow) -> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aDraft.Preparation.Tour.Stops.size() >= maximum_tour_stops) {
			return std::unexpected{ preparation_refusal::full };
		}
		if (aTitle.size() > maximum_tour_field_bytes || aText.size() > maximum_tour_field_bytes) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		if (!line_ok(aTitle, maximum_tour_field_bytes) || !text_ok(aText, maximum_tour_field_bytes)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		const auto latest = latest_ordinal(aDraft);
		auto       source = bind(latest, aFile, aSide, aFirst, aLast);
		if (!source) {
			return std::unexpected{ preparation_refusal::outside_candidate };
		}
		auto  next   = advanced(aDraft, aNow);
		auto& tour   = next.Preparation.Tour;
		auto  stop   = local_tour_stop{ .Id = tour.NextStop++, .Title = std::move(aTitle), .Text = std::move(aText), .Source = std::move(*source) };
		stop.Current = correspond(stop.Source, latest, aFile);
		tour.Stops.push_back(std::move(stop));
		if (!valid(next.Preparation)) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		return next;
	}

	auto save_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aStop, std::string aTitle, std::string aText,
						 const std::optional<local_file_reading>& aFile, const diff_side aSide, const std::uint32_t aFirst, const std::uint32_t aLast, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aTitle.size() > maximum_tour_field_bytes || aText.size() > maximum_tour_field_bytes) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		if (!line_ok(aTitle, maximum_tour_field_bytes) || !text_ok(aText, maximum_tour_field_bytes)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto  next = advanced(aDraft, aNow);
		auto* stop = find_stop(next, aStop);
		if (stop == nullptr) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		if (aTitle.empty() || aText.empty()) {
			return std::unexpected{ preparation_refusal::text_needed };
		}
		const auto changed = stop->Title != aTitle || stop->Text != aText;
		stop->Title        = std::move(aTitle);
		stop->Text         = std::move(aText);
		if (aFile) {
			const auto latest = latest_ordinal(next);
			auto       source = bind(latest, *aFile, aSide, aFirst, aLast);
			if (!source) {
				return std::unexpected{ preparation_refusal::outside_candidate };
			}
			if (*source != stop->Source) {
				if (stop->Earlier.size() >= maximum_local_draft_candidates) {
					stop->Earlier.erase(stop->Earlier.begin() + 1);
				}
				stop->Earlier.push_back(std::exchange(stop->Source, std::move(*source)));
				stop->Current = correspond(stop->Source, latest, *aFile);
				++stop->Revision;
			}
		} else if (stop->Current.Correspondence != local_correspondence::exact) {
			return std::unexpected{ preparation_refusal::lines_needed };
		} else {
			rebind_to_current(*stop);
		}
		if (changed) {
			++stop->Revision;
		}
		mark_accepted(*stop);
		if (!valid(next.Preparation)) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		return next;
	}

	auto edit_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aStop, std::string aTitle, std::string aText,
						 const std::optional<local_file_reading>& aFile, const diff_side aSide, const std::uint32_t aFirst, const std::uint32_t aLast, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aTitle.size() > maximum_tour_field_bytes || aText.size() > maximum_tour_field_bytes) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		if (!line_ok(aTitle, maximum_tour_field_bytes) || !text_ok(aText, maximum_tour_field_bytes)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto  next = advanced(aDraft, aNow);
		auto* stop = find_stop(next, aStop);
		if (stop == nullptr) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		const auto changed = stop->Title != aTitle || stop->Text != aText;
		stop->Title        = std::move(aTitle);
		stop->Text         = std::move(aText);
		if (aFile) {
			const auto latest = latest_ordinal(next);
			auto       source = bind(latest, *aFile, aSide, aFirst, aLast);
			if (!source) {
				return std::unexpected{ preparation_refusal::outside_candidate };
			}
			if (*source != stop->Source) {
				if (stop->Earlier.size() >= maximum_local_draft_candidates) {
					stop->Earlier.erase(stop->Earlier.begin() + 1);
				}
				stop->Earlier.push_back(std::exchange(stop->Source, std::move(*source)));
				stop->Current = correspond(stop->Source, latest, *aFile);
				++stop->Revision;
			}
		}
		if (changed) {
			++stop->Revision;
		}
		if (!valid(next.Preparation)) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		return next;
	}

	auto accept_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aStop, const std::int64_t aNow) -> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		auto  next = advanced(aDraft, aNow);
		auto* stop = find_stop(next, aStop);
		if (stop == nullptr) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		if (stop->Title.empty() || stop->Text.empty()) {
			return std::unexpected{ preparation_refusal::text_needed };
		}
		if (stop->Current.Correspondence != local_correspondence::exact) {
			return std::unexpected{ preparation_refusal::lines_needed };
		}
		rebind_to_current(*stop);
		mark_accepted(*stop);
		return next;
	}

	auto unaccept_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aStop, const std::int64_t aNow) -> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		auto  next = advanced(aDraft, aNow);
		auto* stop = find_stop(next, aStop);
		if (stop == nullptr) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		stop->Accepted            = false;
		stop->AcceptedFingerprint = { };
		return next;
	}

	auto remove_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aStop, const std::int64_t aNow) -> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		auto next    = advanced(aDraft, aNow);
		auto removed = std::erase_if(next.Preparation.Tour.Stops, [aStop](const local_tour_stop& aEntry) { return aEntry.Id == aStop; });
		if (removed == 0U) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		return next;
	}

	auto move_local_stop(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aStop, const std::size_t aIndex, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		auto  next  = advanced(aDraft, aNow);
		auto& stops = next.Preparation.Tour.Stops;
		auto  found = std::ranges::find(stops, aStop, &local_tour_stop::Id);
		if (found == stops.end() || aIndex >= stops.size()) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		auto stop = std::move(*found);
		stops.erase(found);
		stops.insert(stops.begin() + static_cast<std::ptrdiff_t>(aIndex), std::move(stop));
		return next;
	}

	auto carry_local_tour(const local_draft& aDraft, const std::span<const local_file_reading> aLatest) -> local_draft
	{
		auto       next   = aDraft;
		const auto latest = latest_ordinal(next);
		for (auto& stop : next.Preparation.Tour.Stops) {
			const auto file = std::ranges::find(aLatest, stop.Source.Path, &local_file_reading::Path);
			stop.Current    = file == aLatest.end() ? local_destination{ .Candidate = latest, .Correspondence = local_correspondence::unavailable } : correspond(stop.Source, latest, *file);
		}
		return next;
	}

	auto decide_local_stop(const local_tour_stop& aStop) -> local_stop_decision
	{
		if (!aStop.Accepted) {
			return { };
		}
		if (aStop.AcceptedFingerprint != fingerprint(aStop)) {
			return { .State = tour_acceptance_state::changed, .Reason = local_stop_reason::edited };
		}
		switch (aStop.Current.Correspondence) {
		case local_correspondence::exact:       break;
		case local_correspondence::changed:     return { .State = tour_acceptance_state::changed, .Reason = local_stop_reason::changed };
		case local_correspondence::ambiguous:   return { .State = tour_acceptance_state::changed, .Reason = local_stop_reason::ambiguous };
		case local_correspondence::missing:     return { .State = tour_acceptance_state::changed, .Reason = local_stop_reason::missing };
		case local_correspondence::unavailable: return { .State = tour_acceptance_state::changed, .Reason = local_stop_reason::unavailable };
		}
		return { .State = tour_acceptance_state::accepted, .Reason = local_stop_reason::none };
	}

	auto decide_local_tour_head(const local_draft& aDraft) -> local_tour_head
	{
		auto head = local_tour_head{ .Stops = aDraft.Preparation.Tour.Stops.size() };
		for (const auto& stop : aDraft.Preparation.Tour.Stops) {
			head.ToAccept += decide_local_stop(stop).State == tour_acceptance_state::accepted ? 0U : 1U;
		}
		return head;
	}

	// --- Proposals -----------------------------------------------------------------------

	auto record_proposals(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aConversation, const std::uint64_t aCandidate,
						  const std::span<const local_proposal_draft> aProposals, const std::span<const local_file_reading> aFiles, const std::string_view aProducer,
						  const std::string_view aVersion, const std::int64_t aNow) -> preparation_result<std::pair<local_draft, std::vector<std::uint64_t>>>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aDraft.Preparation.Proposals.size() + aProposals.size() > maximum_local_proposals) {
			return std::unexpected{ preparation_refusal::full };
		}
		auto next = advanced(aDraft, aNow);
		auto ids  = std::vector<std::uint64_t>{ };
		for (const auto& draft : aProposals) {
			const auto base = field_revision(next, draft.Target, draft.Stop);
			if (!base || !line_ok(draft.Title, maximum_tour_field_bytes) || !text_ok(draft.Text, maximum_local_description_bytes)) {
				return std::unexpected{ preparation_refusal::invalid };
			}
			auto proposal = local_proposal{ .Id           = next.Preparation.NextProposal++,
											.Target       = draft.Target,
											.Conversation = aConversation,
											.Candidate    = aCandidate,
											.Stop         = draft.Stop,
											.BaseRevision = *base,
											.Title        = draft.Title,
											.Text         = draft.Text,
											.Producer     = std::string{ aProducer.substr(0U, std::min(aProducer.size(), maximum_local_producer_bytes)) },
											.Version      = std::string{ aVersion.substr(0U, std::min(aVersion.size(), maximum_local_producer_bytes)) } };
			if (draft.Target == local_proposal_target::new_stop) {
				const auto file   = std::ranges::find(aFiles, draft.Path, &local_file_reading::Path);
				auto       source = file == aFiles.end() || file->Deleted ? std::nullopt : bind(aCandidate, *file, diff_side::right, draft.FirstLine, draft.LastLine);
				if (source) {
					proposal.Source = std::move(source);
				} else {
					proposal.State = local_proposal_state::outside_candidate;
					// Kept as written, for reading; never bound.
					proposal.Title = draft.Title.empty() ? draft.Path : draft.Title;
					if (path_ok(draft.Path)) {
						proposal.Text = draft.Path + ":" + std::to_string(draft.FirstLine) + "-" + std::to_string(draft.LastLine) + "\n" + draft.Text;
					}
				}
			}
			ids.push_back(proposal.Id);
			next.Preparation.Proposals.push_back(std::move(proposal));
		}
		if (!valid(next.Preparation)) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		return std::pair{ std::move(next), std::move(ids) };
	}

	auto proposal_stale(const local_draft& aDraft, const local_proposal& aProposal) -> bool
	{
		if (aProposal.Candidate != latest_ordinal(aDraft)) {
			return true;
		}
		const auto revision = field_revision(aDraft, aProposal.Target, aProposal.Stop);
		return !revision || *revision != aProposal.BaseRevision;
	}

	auto accept_proposal(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aProposal, const std::int64_t aNow) -> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		const auto found = std::ranges::find(aDraft.Preparation.Proposals, aProposal, &local_proposal::Id);
		if (found == aDraft.Preparation.Proposals.end()) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		if (found->State == local_proposal_state::outside_candidate) {
			return std::unexpected{ preparation_refusal::outside_candidate };
		}
		if (found->State != local_proposal_state::pending) {
			return std::unexpected{ preparation_refusal::decided };
		}
		if (proposal_stale(aDraft, *found)) {
			return std::unexpected{ preparation_refusal::proposal_stale };
		}
		const auto& proposal = *found;
		auto        next     = advanced(aDraft, aNow);
		auto&       prep     = next.Preparation;
		switch (proposal.Target) {
		case local_proposal_target::title:
			if (proposal.Text.empty() || !line_ok(proposal.Text, maximum_local_draft_title_bytes)) {
				return std::unexpected{ proposal.Text.size() > maximum_local_draft_title_bytes ? preparation_refusal::too_long : preparation_refusal::invalid };
			}
			next.Title = proposal.Text;
			++prep.Description.TitleRevision;
			break;
		case local_proposal_target::description:
			prep.Description.Body = proposal.Text;
			++prep.Description.Revision;
			break;
		case local_proposal_target::problem:
			prep.Tour.Problem = proposal.Text;
			++prep.Tour.ProblemRevision;
			break;
		case local_proposal_target::solution:
			prep.Tour.Solution = proposal.Text;
			++prep.Tour.SolutionRevision;
			break;
		case local_proposal_target::stop: {
			auto* stop = find_stop(next, proposal.Stop);
			if (stop == nullptr) {
				return std::unexpected{ preparation_refusal::unknown };
			}
			if (!proposal.Title.empty()) {
				stop->Title = proposal.Title;
			}
			stop->Text = proposal.Text;
			++stop->Revision;
			// Its own proposal's Accept is the author's sign-off when its
			// lines are where they were.
			if (!stop->Title.empty() && !stop->Text.empty() && stop->Current.Correspondence == local_correspondence::exact) {
				rebind_to_current(*stop);
				mark_accepted(*stop);
			}
			break;
		}
		case local_proposal_target::new_stop: {
			if (!proposal.Source || prep.Tour.Stops.size() >= maximum_tour_stops) {
				return std::unexpected{ proposal.Source ? preparation_refusal::full : preparation_refusal::outside_candidate };
			}
			auto stop    = local_tour_stop{ .Id = prep.Tour.NextStop++, .Title = proposal.Title, .Text = proposal.Text, .Source = *proposal.Source };
			stop.Current = { .Candidate      = proposal.Source->Candidate,
							 .Correspondence = local_correspondence::exact,
							 .FirstLine      = proposal.Source->FirstLine,
							 .LastLine       = proposal.Source->LastLine };
			if (!stop.Title.empty() && !stop.Text.empty()) {
				mark_accepted(stop);
			}
			prep.Tour.Stops.push_back(std::move(stop));
			break;
		}
		}
		std::ranges::find(prep.Proposals, aProposal, &local_proposal::Id)->State = local_proposal_state::accepted;
		if (!valid(prep)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		return next;
	}

	auto reject_proposal(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aProposal, const std::int64_t aNow) -> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		auto next  = advanced(aDraft, aNow);
		auto found = std::ranges::find(next.Preparation.Proposals, aProposal, &local_proposal::Id);
		if (found == next.Preparation.Proposals.end()) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		if (found->State != local_proposal_state::pending && found->State != local_proposal_state::outside_candidate) {
			return std::unexpected{ preparation_refusal::decided };
		}
		found->State = local_proposal_state::rejected;
		return next;
	}

	// --- Conversations ---------------------------------------------------------------------

	auto current_conversation(const local_draft& aDraft, const std::string_view aThread, const std::string_view aAgent) -> const local_conversation*
	{
		const auto latest = latest_ordinal(aDraft);
		for (const auto& conversation : std::views::reverse(aDraft.Preparation.Conversations)) {
			if (!conversation.Ended && conversation.Candidate == latest && conversation.Thread == aThread && conversation.Agent == aAgent
				&& conversation.Turns.size() < maximum_local_turns) {
				return &conversation;
			}
		}
		return nullptr;
	}

	auto append_turn(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::string_view aThread, const std::string_view aAgent, const std::string_view aAgentName,
					 local_turn aTurn, const std::int64_t aNow) -> preparation_result<std::pair<local_draft, std::uint64_t>>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (aTurn.Text.size() > maximum_local_turn_bytes) {
			return std::unexpected{ preparation_refusal::too_long };
		}
		if (!thread_ok(aThread) || aAgent.empty() || !line_ok(aAgent, 256U) || !text_ok(aTurn.Text, maximum_local_turn_bytes)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto        next = advanced(aDraft, aNow);
		const auto* open = current_conversation(aDraft, aThread, aAgent);
		auto        id   = open == nullptr ? std::uint64_t{ 0U } : open->Id;
		auto&       prep = next.Preparation;
		if (id == 0U) {
			if (prep.Conversations.size() >= maximum_local_conversations) {
				return std::unexpected{ preparation_refusal::full };
			}
			id = prep.NextConversation++;
			prep.Conversations.push_back({ .Id        = id,
										   .Candidate = latest_ordinal(aDraft),
										   .Thread    = std::string{ aThread },
										   .Agent     = std::string{ aAgent },
										   .AgentName = std::string{ aAgentName.substr(0U, std::min<std::size_t>(aAgentName.size(), 256U)) } });
		}
		aTurn.At = aNow;
		std::ranges::find(prep.Conversations, id, &local_conversation::Id)->Turns.push_back(std::move(aTurn));
		if (!valid(prep)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		return std::pair{ std::move(next), id };
	}

	auto record_handle(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aConversation, std::string aHandle, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		if (!line_ok(aHandle, 1024U)) {
			return std::unexpected{ preparation_refusal::invalid };
		}
		auto next  = advanced(aDraft, aNow);
		auto found = std::ranges::find(next.Preparation.Conversations, aConversation, &local_conversation::Id);
		if (found == next.Preparation.Conversations.end()) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		found->Handle = std::move(aHandle);
		return next;
	}

	auto end_conversation(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aConversation, const std::int64_t aNow)
		-> preparation_result<local_draft>
	{
		if (aDraft.Version != aExpectedVersion) {
			return std::unexpected{ preparation_refusal::stale };
		}
		auto next  = advanced(aDraft, aNow);
		auto found = std::ranges::find(next.Preparation.Conversations, aConversation, &local_conversation::Id);
		if (found == next.Preparation.Conversations.end()) {
			return std::unexpected{ preparation_refusal::unknown };
		}
		found->Ended = true;
		return next;
	}

	auto replay_turns(const local_conversation& aConversation) -> local_replay
	{
		auto replay = local_replay{ };
		auto bytes  = std::size_t{ 0U };
		auto taken  = std::size_t{ 0U };
		for (const auto& turn : std::views::reverse(aConversation.Turns)) {
			if (taken == maximum_local_replay_turns || bytes + turn.Text.size() > maximum_local_replay_bytes) {
				break;
			}
			bytes += turn.Text.size();
			++taken;
		}
		replay.Omitted = aConversation.Turns.size() - taken;
		replay.Turns.assign(aConversation.Turns.end() - static_cast<std::ptrdiff_t>(taken), aConversation.Turns.end());
		return replay;
	}

	auto conversation_cost(const local_conversation& aConversation) -> std::optional<std::uint64_t>
	{
		auto total = std::uint64_t{ 0U };
		auto any   = false;
		for (const auto& turn : aConversation.Turns) {
			if (turn.Author) {
				continue;
			}
			if (!turn.CostMicros) {
				return std::nullopt;
			}
			total += *turn.CostMicros;
			any = true;
		}
		return any ? std::optional{ total } : std::nullopt;
	}

	// --- The request and the answer -------------------------------------------------------

	auto compose_local_request(const local_draft& aDraft, const local_question& aQuestion, const local_conversation* aConversation,
							   const std::span<const local_file_reading> aFiles) -> std::optional<local_request>
	{
		const auto* latest = latest_candidate(aDraft);
		if (latest == nullptr) {
			return std::nullopt;
		}
		const auto& prep     = aDraft.Preparation;
		auto        request  = local_request{ .FilesTotal = latest->Changes.size() };
		request.Resumed      = aConversation != nullptr && !aConversation->Handle.empty();
		request.Instructions = aQuestion.Instructions;
		request.Instructions += "\n\n## Answer format\n\nAnswer in prose. To propose a change, end with one fenced block whose info string is bpr-proposals, holding a JSON array of "
								"objects: {\"target\": \"title\"|\"description\"|\"problem\"|\"solution\", \"text\": the whole new text}, {\"target\": \"stop\", \"stop\": its number, "
								"\"title\", \"text\"} or {\"target\": \"new_stop\", \"path\", \"first\", \"last\" (lines of this candidate), \"title\", \"text\"}. "
								"The author accepts or rejects each.\n";
		auto& out = request.Text;
		out += "## The local draft\n\nTitle: " + aDraft.Title + "\nTarget: " + aDraft.Target.Target.Repository.Owner + "/" + aDraft.Target.Target.Repository.Name + " "
			+ aDraft.Target.Target.Branch + ", from " + aDraft.Branch + ".\nCandidate " + std::to_string(latest->Ordinal)
			+ (latest->Included.empty() ? std::string{ ", the committed branch." } : std::string{ ", the branch with uncommitted files the author included." })
			+ " No pull request exists yet.\n";
		if (!prep.Description.Body.empty()) {
			out += "\n## Description\n\n" + prep.Description.Body + "\n";
		}
		if (!prep.Tour.Problem.empty() || !prep.Tour.Solution.empty() || !prep.Tour.Stops.empty()) {
			out += "\n## Tour\n";
			if (!prep.Tour.Problem.empty()) {
				out += "\nProblem:\n" + prep.Tour.Problem + "\n";
			}
			if (!prep.Tour.Solution.empty()) {
				out += "\nSolution:\n" + prep.Tour.Solution + "\n";
			}
			for (const auto& stop : prep.Tour.Stops) {
				const auto exact = stop.Current.Correspondence == local_correspondence::exact;
				out += "\nStop " + std::to_string(stop.Id) + ": " + stop.Title + " (" + stop.Source.Path + ":"
					+ std::to_string(exact ? stop.Current.FirstLine : stop.Source.FirstLine) + "-" + std::to_string(exact ? stop.Current.LastLine : stop.Source.LastLine)
					+ (exact ? std::string{ } : " on an earlier candidate; " + std::string{ name(stop.Current.Correspondence) } + " now") + ")\n" + stop.Text + "\n";
			}
		}
		if (aConversation != nullptr && !aConversation->Turns.empty() && !request.Resumed) {
			const auto replay = replay_turns(*aConversation);
			request.Replayed  = replay.Turns.size();
			request.Omitted   = replay.Omitted;
			out += "\n## Earlier in this conversation\n";
			if (replay.Omitted != 0U) {
				out += "\n(" + std::to_string(replay.Omitted) + " earlier turns are left out.)\n";
			}
			for (const auto& turn : replay.Turns) {
				out += std::string{ turn.Author ? "\nAuthor: " : "\nAgent: " } + turn.Text + "\n";
			}
		}
		auto question = std::string{ "\n## The question\n\nIt is about " };
		switch (aQuestion.Subject) {
		case local_subject::whole_change: question += "the whole change."; break;
		case local_subject::title:        question += "the title."; break;
		case local_subject::description:  question += "the description."; break;
		case local_subject::problem:      question += "the tour's Problem."; break;
		case local_subject::solution:     question += "the tour's Solution."; break;
		case local_subject::stop:         question += "stop " + std::to_string(aQuestion.Stop) + "."; break;
		case local_subject::lines:
			question += aQuestion.Lines ? "these lines, " + aQuestion.Lines->Path + ":" + std::to_string(aQuestion.Lines->FirstLine) + "-" + std::to_string(aQuestion.Lines->LastLine)
					+ ":\n\n```\n" + aQuestion.Lines->Excerpt + (aQuestion.Lines->Excerpt.ends_with('\n') ? "" : "\n") + "```"
										: std::string{ "attached lines." };
			break;
		}
		question += "\n\n" + aQuestion.Text + "\n";
		const auto fixed = request.Instructions.size() + out.size() + question.size();
		if (fixed > maximum_local_request_bytes) {
			return std::nullopt;
		}
		// The resumed session read the candidate's files on its first turn
		// (a conversation is on one candidate); only the question goes now.
		if (request.Resumed) {
			out += "\n(The candidate's changed files are as sent earlier in this session.)\n";
			out += question;
			return request;
		}
		// The candidate's own bytes, the subject's file first; never the worktree.
		auto order = std::vector<const local_file_reading*>{ };
		auto first = std::string{ };
		if (aQuestion.Lines) {
			first = aQuestion.Lines->Path;
		} else if (const auto* stop = aQuestion.Stop == 0U ? nullptr : find_stop(aDraft, aQuestion.Stop); stop != nullptr) {
			first = stop->Source.Path;
		}
		for (const auto& file : aFiles) {
			if (file.Path == first) {
				order.insert(order.begin(), &file);
			} else {
				order.push_back(&file);
			}
		}
		auto files = std::string{ };
		for (const auto* file : order) {
			const auto change = std::ranges::find(latest->Changes, file->Path, &draft_path_change::Path);
			if (change == latest->Changes.end()) {
				continue;
			}
			auto block = "\n### " + file->Path + " (" + std::string{ name(change->Change) } + ")\n";
			if (file->Deleted || !file->Text) {
				block += file->Deleted ? "Deleted by this change.\n" : "Not text; not sent.\n";
			} else {
				block += "```\n" + *file->Text + (file->Text->ends_with('\n') ? "" : "\n") + "```\n";
			}
			if (fixed + files.size() + block.size() + 64U > maximum_local_request_bytes) {
				continue;
			}
			files += block;
			++request.FilesSent;
		}
		out += "\n## The candidate's changed files (" + std::to_string(request.FilesSent) + " of " + std::to_string(request.FilesTotal) + ", as captured)\n" + files;
		out += question;
		return request;
	}

	auto parse_local_answer(const std::string_view aText) -> local_answer
	{
		auto       answer = local_answer{ };
		const auto open   = aText.find("```bpr-proposals");
		if (open == std::string_view::npos) {
			answer.Prose = std::string{ aText };
			return answer;
		}
		const auto body  = aText.find('\n', open);
		const auto close = body == std::string_view::npos ? std::string_view::npos : aText.find("```", body + 1U);
		if (close == std::string_view::npos) {
			answer.Prose     = std::string{ aText };
			answer.Malformed = true;
			return answer;
		}
		auto items = std::vector<pw::answer_item>{ };
		if (glz::read<glz::opts{ .error_on_unknown_keys = false }>(items, aText.substr(body + 1U, close - body - 1U)) || items.size() > maximum_local_proposals) {
			answer.Prose     = std::string{ aText };
			answer.Malformed = true;
			return answer;
		}
		for (const auto& item : items) {
			const auto target = enum_from(item.Target, all_targets);
			if (!target) {
				answer.Malformed = true;
				continue;
			}
			answer.Proposals.push_back({ .Target    = *target,
										 .Stop      = item.Stop,
										 .Title     = item.Title,
										 .Text      = item.Text,
										 .Path      = item.Path,
										 .FirstLine = item.First,
										 .LastLine  = item.Last });
		}
		auto prose = std::string{ aText.substr(0U, open) } + std::string{ aText.substr(close + 3U) };
		while (!prose.empty() && (prose.back() == '\n' || prose.back() == ' ')) {
			prose.pop_back();
		}
		answer.Prose = std::move(prose);
		return answer;
	}

	// --- Publication ------------------------------------------------------------------------

	auto compose_local_publication(const local_draft& aDraft) -> local_publication_preview
	{
		auto        preview = local_publication_preview{ .Title = aDraft.Title, .Body = aDraft.Preparation.Description.Body };
		const auto& tour    = aDraft.Preparation.Tour;
		auto&       out     = preview.Tour;
		if (!tour.Problem.empty()) {
			out += "## Problem\n\n" + tour.Problem + "\n\n";
		}
		if (!tour.Solution.empty()) {
			out += "## Solution\n\n" + tour.Solution + "\n\n";
		}
		auto index = std::size_t{ 0U };
		for (const auto& stop : tour.Stops) {
			++index;
			if (decide_local_stop(stop).State != tour_acceptance_state::accepted) {
				preview.Unaccepted.push_back(stop.Id);
			}
			out += "### " + std::to_string(index) + ". " + stop.Title + "\n\n";
			out += "`" + stop.Source.Path + "` lines " + std::to_string(stop.Current.Correspondence == local_correspondence::exact ? stop.Current.FirstLine : stop.Source.FirstLine) + "–"
				+ std::to_string(stop.Current.Correspondence == local_correspondence::exact ? stop.Current.LastLine : stop.Source.LastLine)
				+ " (link pending until the pull request exists)\n\n";
			++preview.PendingLinks;
			out += stop.Text + "\n\n";
		}
		while (out.ends_with("\n\n")) {
			out.pop_back();
		}
		return preview;
	}

	auto map_local_publication(const local_draft& aDraft, const local_publication_result& aResult) -> local_publication_mapping
	{
		auto mapping = local_publication_mapping{ .DescriptionPreserved = aResult.Body == aDraft.Preparation.Description.Body };
		for (const auto& stop : aDraft.Preparation.Tour.Stops) {
			auto entry = local_stop_mapping{ .Stop = stop.Id };
			if (stop.Source.Side == diff_side::right) {
				const auto file = std::ranges::find(aResult.Files, stop.Source.Path, &local_file_reading::Path);
				if (file != aResult.Files.end()) {
					// A candidate the pull request was not made from cannot claim its lines.
					auto binding         = stop.Source;
					binding.Candidate    = 0U;
					const auto there     = correspond(binding, std::numeric_limits<std::uint64_t>::max(), *file);
					entry.Correspondence = there.Correspondence;
					if (there.Correspondence == local_correspondence::exact) {
						entry.Anchor    = tour_code_destination{ .Commit    = aResult.Head,
																 .Path      = stop.Source.Path,
																 .Side      = diff_side::right,
																 .FirstLine = there.FirstLine,
																 .LastLine  = there.LastLine };
						entry.Permalink = permalink(aResult, stop.Source.Path, there.FirstLine, there.LastLine);
					}
				}
			}
			mapping.Unresolved += entry.Anchor ? 0U : 1U;
			mapping.Stops.push_back(std::move(entry));
		}
		return mapping;
	}
}
