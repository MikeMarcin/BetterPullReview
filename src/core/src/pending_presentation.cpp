#include "bpr/core/pending_presentation.hpp"

namespace bpr::core {
	namespace {
		[[nodiscard]] auto revealed(const pending_look aLook) noexcept -> bool
		{
			switch (aLook) {
			case pending_look::loading:
			case pending_look::loading_slow:
			case pending_look::partial:
			case pending_look::refreshing:
			case pending_look::refreshing_slow:
				return true;
			case pending_look::ready:
			case pending_look::placeholder:
			case pending_look::quiet:
			case pending_look::failed:
			case pending_look::failed_retained:
				return false;
			}
			return false;
		}

		[[nodiscard]] auto failing(const pending_look aLook) noexcept -> bool
		{
			return aLook == pending_look::failed || aLook == pending_look::failed_retained;
		}
	} // namespace

	auto decide_pending_look(const pending_facts& aFacts) noexcept -> pending_look
	{
		switch (aFacts.Activity) {
		case pending_activity::idle:
			return pending_look::ready;
		case pending_activity::failed:
			return aFacts.HasContent ? pending_look::failed_retained : pending_look::failed;
		case pending_activity::loading:
			break;
		}
		const auto shown = aFacts.ElapsedMilliseconds >= pending_reveal_milliseconds;
		const auto slow  = aFacts.ElapsedMilliseconds >= pending_slow_milliseconds;
		if (!aFacts.HasContent) {
			return !shown ? pending_look::placeholder : slow ? pending_look::loading_slow
															 : pending_look::loading;
		}
		// What has arrived is the newest content there is, so it is never
		// veiled; only content a read is replacing is.
		if (!shown) {
			return pending_look::quiet;
		}
		if (aFacts.Partial) {
			return pending_look::partial;
		}
		return slow ? pending_look::refreshing_slow : pending_look::refreshing;
	}

	auto pending_busy(const pending_look aLook) noexcept -> bool
	{
		return aLook != pending_look::ready && !failing(aLook);
	}

	auto pending_turning(const pending_look aLook) noexcept -> bool
	{
		return aLook == pending_look::loading || aLook == pending_look::loading_slow || aLook == pending_look::partial;
	}

	auto pending_veiled(const pending_look aLook) noexcept -> bool
	{
		return aLook == pending_look::refreshing || aLook == pending_look::refreshing_slow;
	}

	auto pending_retry(const pending_look aLook) noexcept -> bool
	{
		return failing(aLook);
	}

	auto pending_slow(const pending_look aLook) noexcept -> bool
	{
		return aLook == pending_look::loading_slow || aLook == pending_look::refreshing_slow;
	}

	auto pending_announcement_between(const pending_look aBefore, const pending_look aAfter) noexcept -> pending_announcement
	{
		if (aBefore == aAfter) {
			return pending_announcement::none;
		}
		if (failing(aAfter)) {
			return pending_announcement::failed;
		}
		if (pending_slow(aAfter) && !pending_slow(aBefore)) {
			return pending_announcement::slow;
		}
		if (revealed(aAfter) && !revealed(aBefore)) {
			return pending_announcement::started;
		}
		if (aAfter == pending_look::ready && revealed(aBefore)) {
			return pending_announcement::finished;
		}
		return pending_announcement::none;
	}

	auto pending_next_change_milliseconds(const pending_facts& aFacts) noexcept -> std::uint32_t
	{
		if (aFacts.Activity != pending_activity::loading) {
			return 0U;
		}
		if (aFacts.ElapsedMilliseconds < pending_reveal_milliseconds) {
			return pending_reveal_milliseconds - aFacts.ElapsedMilliseconds;
		}
		// A partial pane names what is still coming from the start; only a
		// pane with nothing new to show waits for the slow mark.
		if (!(aFacts.HasContent && aFacts.Partial) && aFacts.ElapsedMilliseconds < pending_slow_milliseconds) {
			return pending_slow_milliseconds - aFacts.ElapsedMilliseconds;
		}
		return 0U;
	}
} // namespace bpr::core
