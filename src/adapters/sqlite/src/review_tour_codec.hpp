#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_TOUR_CODEC_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_TOUR_CODEC_HPP
#include "bpr/core/review_tour.hpp"
#include <limits>
#include <span>
#include <type_traits>
namespace bpr::adapters::sqlite::detail {
	/// \brief Versioned bounded binary payload; lengths are admitted before allocation.
	class tour_codec
	{
	public:
		std::vector<std::uint8_t>     Bytes;
		std::span<const std::uint8_t> Input;
		bool                          Reading = false;
		bool                          Good    = true;
		std::uint8_t                  Version = 8;
		void                          byte(std::uint8_t& aValue)
		{
			if (!Good) {
				return;
			}
			if (!Reading) {
				Bytes.push_back(aValue);
				return;
			}
			if (Input.empty()) {
				Good = false;
				return;
			}
			aValue = Input.front();
			Input  = Input.subspan(1);
		}
		template <typename T>
		void number(T& aValue)
		{
			std::uint64_t result = 0;
			for (unsigned index = 0; index < 8; ++index) {
				auto value = static_cast<std::uint8_t>(static_cast<std::uint64_t>(aValue) >> ((7U - index) * 8U));
				byte(value);
				result = (result << 8U) | value;
			}
			if (Reading) {
				if (result > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
					Good = false;
				}
				aValue = static_cast<T>(result);
			}
		}
		template <typename T>
		void id(T& aValue)
		{
			auto value = aValue.value();
			number(value);
			aValue = T{ value };
		}
		template <typename T>
		void enumeration(T& aValue)
		{
			auto value = static_cast<std::uint8_t>(aValue);
			byte(value);
			aValue = static_cast<T>(value);
		}
		template <typename T>
		void digest(T& aValue)
		{
			auto bytes = aValue.bytes();
			for (auto& value : bytes) {
				byte(value);
			}
			aValue = T{ bytes };
		}
		template <typename T>
		void oid(T& aValue)
		{
			auto algorithm = aValue.algorithm();
			enumeration(algorithm);
			auto bytes = aValue.bytes();
			for (auto& value : bytes) {
				byte(value);
			}
			aValue = T{ algorithm, bytes };
		}
		void text(std::string& aValue, std::size_t aLimit = core::maximum_tour_field_bytes)
		{
			auto size = aValue.size();
			number(size);
			if (!Good || size > aLimit || (Reading && size > Input.size())) {
				Good = false;
				return;
			}
			if (Reading) {
				aValue.assign(reinterpret_cast<const char*>(Input.data()), size);
				Input = Input.subspan(size);
			} else {
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
		}
		template <typename T, typename F>
		void optional(std::optional<T>& aValue, F aFunction)
		{
			auto present = static_cast<std::uint8_t>(aValue.has_value());
			byte(present);
			if (!Good || present > 1) {
				Good = false;
				return;
			}
			if (!present) {
				aValue.reset();
				return;
			}
			if (!aValue) {
				aValue.emplace();
			}
			aFunction(*aValue);
		}
		template <typename T, typename F>
		void list(std::vector<T>& aValues, std::size_t aLimit, F aFunction)
		{
			auto count = aValues.size();
			number(count);
			if (!Good || count > aLimit) {
				Good = false;
				return;
			}
			if (Reading) {
				aValues.resize(count);
			}
			for (auto& value : aValues) {
				if (!Good) {
					break;
				}
				aFunction(value);
			}
		}
		void owner(core::tour_owner& aOwner)
		{
			id(aOwner.Profile);
			id(aOwner.Host);
			digest(aOwner.HostAuthority);
			id(aOwner.Account);
			id(aOwner.Repository);
			id(aOwner.PullRequest);
		}
		void source(core::tour_source_range& aSource)
		{
			digest(aSource.First.Comparison);
			enumeration(aSource.First.Side);
			number(aSource.First.Line);
			digest(aSource.Last.Comparison);
			enumeration(aSource.Last.Side);
			number(aSource.Last.Line);
			oid(aSource.Commit);
			optional(aSource.Blob, [&](auto& aBlob) { oid(aBlob); });
			text(aSource.Path, core::maximum_tour_path_bytes);
		}
		void baseline(core::tour_field_baseline& aValue)
		{
			text(aValue.Raw);
			text(aValue.RemoteEntity, 1024);
			text(aValue.ObservedVersion, 1024);
			optional(aValue.SourceSpan, [&](auto& aSpan) {number(aSpan.first);number(aSpan.second); });
			number(aValue.LocalGeneration);
			if (Version >= 5) {
				optional(aValue.DraftText, [&](auto& aDraftText) { text(aDraftText); });
			}
		}
		void remote(core::tour_remote_binding& aValue)
		{
			enumeration(aValue.Kind);
			number(aValue.DatabaseId);
			text(aValue.Entity, 1024);
			text(aValue.Actor, 1024);
			text(aValue.WebUrl, core::maximum_tour_path_bytes);
			text(aValue.Review, 1024);
			number(aValue.Own);
			number(aValue.Resolved);
			number(aValue.Outdated);
		}
		void destination(core::tour_code_destination& aValue)
		{
			oid(aValue.Commit);
			optional(aValue.Blob, [this](auto& aBlob) { oid(aBlob); });
			text(aValue.Path, core::maximum_tour_path_bytes);
			enumeration(aValue.Side);
			number(aValue.FirstLine);
			number(aValue.LastLine);
		}

		void stop(core::tour_stop& aStop)
		{
			id(aStop.Id);
			text(aStop.Title);
			text(aStop.Body);
			optional(aStop.Original, [&](auto& aSource) { source(aSource); });
			optional(aStop.Mapped, [&](auto& aSource) { source(aSource); });
			enumeration(aStop.Mapping);
			enumeration(aStop.Preparation);
			list(aStop.Related, core::maximum_tour_related_sources, [&](auto& aSource) { source(aSource); });
			optional(aStop.Baseline, [&](auto& aBaseline) { baseline(aBaseline); });
			if (Version >= 2) {
				optional(aStop.Remote, [&](auto& aValue) { remote(aValue); });
			}
			if (Version >= 3) {
				optional(aStop.Destination, [this](auto& aValue) { destination(aValue); });
			}
			if (Version >= 6) {
				optional(aStop.ReaffirmedHead, [&](auto& aValue) { oid(aValue); });
			}
			if (Version >= 7) {
				optional(aStop.Suggestion, [&](auto& aValue) { text(aValue.AnchorRef, 128); text(aValue.Reason, 240); text(aValue.Question, 240); number(aValue.Visited); });
			}
			if (Version >= 8) {
				enumeration(aStop.CompletionOrigin);
			}
		}
		void tour(core::review_tour& aTour)
		{
			byte(Version);
			if (Version != 1 && Version != 2 && Version != 3 && Version != 4 && Version != 5 && Version != 6 && Version != 7 && Version != 8) {
				Good = false;
				return;
			}
			id(aTour.Id);
			owner(aTour.Owner);
			oid(aTour.Base);
			oid(aTour.Head);
			number(aTour.Revision);
			number(aTour.LastStopId);
			text(aTour.Problem);
			text(aTour.Solution);
			optional(aTour.ProblemBaseline, [&](auto& aValue) { baseline(aValue); });
			optional(aTour.SolutionBaseline, [&](auto& aValue) { baseline(aValue); });
			list(aTour.Stops, core::maximum_tour_stops, [&](auto& aStop) { stop(aStop); });
			if (Version >= 2) {
				optional(aTour.Remote, [&](auto& aValue) { remote(aValue); });
				optional(aTour.IndexBaseline, [&](auto& aValue) { baseline(aValue); });
				text(aTour.ImportNotice);
				number(aTour.ReadOnly);
			}
			if (Version >= 4) {
				list(aTour.Retired, core::maximum_tour_stops, [&](auto& aValue) { stop(aValue); });
			}
			if (Version >= 5) {
				list(aTour.Conflicts, core::maximum_tour_stops + 3, [&](auto& aConflict) {enumeration(aConflict.Field);id(aConflict.Stop);text(aConflict.Baseline);text(aConflict.Local);text(aConflict.Remote);baseline(aConflict.Observation); });
				list(aTour.IncomingRoute, core::maximum_tour_stops, [&](auto& aValue) { stop(aValue); });
				number(aTour.RemoteStale);
			}
			if (Version >= 6) {
				optional(aTour.ObservedHead, [&](auto& aValue) { oid(aValue); });
				number(aTour.HasPriorObservation);
				list(aTour.ObservedChanges, core::maximum_tour_stops * 4 + 2, [&](auto& aValue) {enumeration(aValue.Kind);id(aValue.Stop);text(aValue.Title); });
			}
		}
	};
}
#endif
