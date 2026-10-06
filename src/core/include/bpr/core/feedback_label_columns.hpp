#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_LABEL_COLUMNS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_LABEL_COLUMNS_HPP

#include "bpr/core/feedback_convention.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1088]]. The reviewer's chosen Overview label columns: an
/// ordered subset of the twelve catalog labels, each at most once
/// (RFC-BPR-0049 R20, R28; RFC-BPR-0051 R41).
/// \details The selection is a preference, not a count: it names which labels
/// get a dedicated chip and in which order, and `project_feedback_counters`
/// answers it from a kept tally. Empty is the default, so the catch-all
/// counts everything until a label is chosen.
///
/// A stored selection is text of stable catalog IDs separated by commas,
/// `issue,question`, so a file read by a later build, or edited by hand,
/// still names labels rather than positions. Reading it never fails: an ID
/// the catalog does not know and an ID named a second time are left out,
/// counted, and reported, while every valid ID keeps its place. Nothing that
/// was valid is lost, and the caller can say exactly what was dropped.
namespace bpr::core {
	/// \brief An ordered subset of the catalog, each label at most once.
	/// \details Fixed-size and trivially copyable, so a settings record that
	/// carries it needs no allocation. Entries past `Count` are always the
	/// first catalog label; `make_feedback_label_columns` keeps that, so two
	/// selections are equal exactly when their labels and order are.
	struct feedback_label_columns
	{
		std::array<feedback_label, feedback_label_count> Labels{ };
		std::uint8_t                                     Count = 0U;

		auto operator<=>(const feedback_label_columns&) const = default;
	};

	/// \brief Why a list of labels is not a column selection.
	enum class feedback_label_columns_refusal : std::uint8_t {
		/// More entries than there are labels.
		too_many,
		/// A value outside the twelve labels.
		unknown_label,
		/// The same label twice.
		duplicate_label,
		/// Storage past `Count` that is not the first label.
		noncanonical,
	};
	[[nodiscard]] auto stable_name(feedback_label_columns_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Builds a selection from labels in the order given, refusing an
	/// unknown or repeated label rather than dropping it.
	[[nodiscard]] auto make_feedback_label_columns(std::span<const feedback_label> aLabels)
		-> std::expected<feedback_label_columns, feedback_label_columns_refusal>;
	/// \brief Checks a selection someone else built: the first refusal, if any.
	[[nodiscard]] auto check_feedback_label_columns(const feedback_label_columns& aColumns) noexcept
		-> std::expected<void, feedback_label_columns_refusal>;
	/// \brief The selected labels in their order.
	[[nodiscard]] auto labels_of(const feedback_label_columns& aColumns) noexcept -> std::span<const feedback_label>;

	/// \brief The longest stored selection text a reader accepts, in bytes.
	/// \details All twelve IDs and their commas take 84 bytes; the bound
	/// leaves room for a hand-edited file to be read and explained rather
	/// than refused, and bounds the work of reading one.
	inline constexpr std::size_t maximum_feedback_label_columns_text = 256U;

	/// \brief The stored text of a selection: stable IDs and commas.
	[[nodiscard]] auto feedback_label_columns_text(const feedback_label_columns& aColumns) -> std::string;

	/// \brief What reading a stored selection found.
	/// \details `Columns` holds every catalog ID the text named, once, at its
	/// first position. `Unknown` counts entries that are not a catalog ID,
	/// empty entries included; `Repeated` counts later copies of an ID
	/// already taken. Both zero means the text was exactly a selection.
	struct feedback_label_columns_reading
	{
		feedback_label_columns Columns;
		std::uint8_t           Unknown  = 0U;
		std::uint8_t           Repeated = 0U;

		auto operator<=>(const feedback_label_columns_reading&) const = default;
	};

	/// \brief Reads stored selection text, keeping every valid ID in order.
	/// \details Only exact stable IDs are labels; there is no alias and no
	/// case folding, because only this build's writer produces the text. Text
	/// longer than `maximum_feedback_label_columns_text` is read only up to
	/// the last comma inside that bound, and the unread rest counts as one
	/// unknown entry. Counts saturate at 255.
	[[nodiscard]] auto read_feedback_label_columns(std::string_view aText) -> feedback_label_columns_reading;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_LABEL_COLUMNS_HPP
