#include "bpr/core/view_filter_memory.hpp"

namespace bpr::core {
	namespace {
		[[nodiscard]] auto slot(view_filter_state& aState, const filter_view aView) noexcept -> view_filter_choice&
		{
			switch (aView) {
			case filter_view::threads:
				return aState.Threads;
			case filter_view::review_comments:
				return aState.ReviewComments;
			case filter_view::discuss:
				break;
			}
			return aState.Discuss;
		}

		[[nodiscard]] auto slot(view_filter_defaults& aDefaults, const filter_view aView) noexcept -> std::optional<view_filter_choice>&
		{
			switch (aView) {
			case filter_view::threads:
				return aDefaults.Threads;
			case filter_view::review_comments:
				return aDefaults.ReviewComments;
			case filter_view::discuss:
				break;
			}
			return aDefaults.Discuss;
		}

		[[nodiscard]] auto known(const filter_view aView) noexcept -> bool
		{
			return static_cast<std::uint8_t>(aView) <= static_cast<std::uint8_t>(filter_view::review_comments);
		}

		/// One view's stored value: none stays none; an unknown value is
		/// left out and reported.
		[[nodiscard]] auto read_one(const std::optional<std::int64_t>& aStored, bool& aRecovered) noexcept -> std::optional<view_filter_choice>
		{
			if (!aStored) {
				return std::nullopt;
			}
			auto choice = read_stored_view_filter(*aStored);
			aRecovered  = aRecovered || !choice;
			return choice;
		}

		[[nodiscard]] auto write_one(const std::optional<view_filter_choice>& aChoice) noexcept -> std::optional<std::int64_t>
		{
			return aChoice ? std::optional{ stored_view_filter(*aChoice) } : std::nullopt;
		}
	}

	auto filters_of(const view_filter_state& aState, const filter_view aView) noexcept -> view_filter_choice
	{
		switch (aView) {
		case filter_view::threads:
			return aState.Threads;
		case filter_view::review_comments:
			return aState.ReviewComments;
		case filter_view::discuss:
			break;
		}
		return aState.Discuss;
	}

	auto open_view_filters(const view_filter_defaults& aDefaults, const view_filter_choice aReviewCommentsFirst) noexcept -> view_filter_state
	{
		auto state           = view_filter_state{ };
		state.Discuss        = aDefaults.Discuss.value_or(view_filter_choice{ });
		state.Threads        = aDefaults.Threads.value_or(view_filter_choice{ });
		state.ReviewComments = aDefaults.ReviewComments.value_or(aReviewCommentsFirst);
		return state;
	}

	auto choose_view_filter(const view_filter_state& aState, const filter_view aView, const view_filter_choice aChoice) noexcept -> view_filter_transition
	{
		if (!known(aView)) {
			return { .State = aState, .Remember = std::nullopt };
		}
		return { .State = present_view_filter(aState, aView, aChoice), .Remember = view_filter_remember{ .View = aView, .Choice = aChoice } };
	}

	auto present_view_filter(const view_filter_state& aState, const filter_view aView, const view_filter_choice aChoice) noexcept -> view_filter_state
	{
		auto next = aState;
		if (known(aView)) {
			slot(next, aView) = aChoice;
		}
		return next;
	}

	auto remember(const view_filter_defaults& aDefaults, const view_filter_remember& aChoice) noexcept -> view_filter_defaults
	{
		auto next = aDefaults;
		if (known(aChoice.View)) {
			slot(next, aChoice.View) = aChoice.Choice;
		}
		return next;
	}

	auto stored_view_filter(const view_filter_choice aChoice) noexcept -> std::int64_t
	{
		return (aChoice.OnlyNew ? 1 : 0) | (aChoice.HideResolved ? 2 : 0);
	}

	auto read_stored_view_filter(const std::int64_t aStored) noexcept -> std::optional<view_filter_choice>
	{
		if (aStored < 0 || aStored > 3) {
			return std::nullopt;
		}
		return view_filter_choice{ .OnlyNew = (aStored & 1) != 0, .HideResolved = (aStored & 2) != 0 };
	}

	auto read_view_filter_memory(const std::optional<stored_view_filter_row>& aRow) noexcept -> view_filter_memory_read
	{
		auto read = view_filter_memory_read{ };
		if (!aRow) {
			return read;
		}
		if (aRow->Version > view_filter_memory_version) {
			read.Writable = false;
			return read;
		}
		if (aRow->Version < view_filter_memory_version) {
			read.Recovered = true;
			return read;
		}
		read.Defaults.Discuss        = read_one(aRow->Discuss, read.Recovered);
		read.Defaults.Threads        = read_one(aRow->Threads, read.Recovered);
		read.Defaults.ReviewComments = read_one(aRow->ReviewComments, read.Recovered);
		return read;
	}

	auto stored_view_filter_memory(const view_filter_defaults& aDefaults) noexcept -> stored_view_filter_row
	{
		return {
			.Version        = view_filter_memory_version,
			.Discuss        = write_one(aDefaults.Discuss),
			.Threads        = write_one(aDefaults.Threads),
			.ReviewComments = write_one(aDefaults.ReviewComments),
		};
	}
}
