#include "bpr/core/file_progress_mark.hpp"

namespace bpr::core {
	auto name(const file_progress_mark aMark) noexcept -> std::string_view
	{
		switch (aMark) {
		case file_progress_mark::unread:
			return "unread";
		case file_progress_mark::seen:
			return "seen";
		case file_progress_mark::read:
			return "read";
		case file_progress_mark::accepted:
			return "accepted";
		case file_progress_mark::excluded:
			return "excluded";
		}
		return "unread";
	}

	auto progress_mark(const file_receipt& aReceipt) noexcept -> file_progress_mark
	{
		if (aReceipt.Excluded) {
			return file_progress_mark::excluded;
		}
		if (aReceipt.Accepted) {
			return file_progress_mark::accepted;
		}
		switch (aReceipt.Read) {
		case file_read_state::unread:
			return file_progress_mark::unread;
		case file_read_state::seen:
			return file_progress_mark::seen;
		case file_read_state::read:
			return file_progress_mark::read;
		}
		return file_progress_mark::unread;
	}
} // namespace bpr::core
