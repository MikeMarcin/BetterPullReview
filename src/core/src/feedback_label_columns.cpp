#include "bpr/core/feedback_label_columns.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto catalog_label(const feedback_label aLabel) noexcept -> bool
		{
			return static_cast<std::size_t>(aLabel) < feedback_label_count;
		}

		/// \brief The exact stable ID's label, or none.
		[[nodiscard]] auto label_named(const std::string_view aId) noexcept -> std::optional<feedback_label>
		{
			for (const auto& definition : feedback_label_catalog()) {
				if (definition.Id == aId) {
					return definition.Label;
				}
			}
			return std::nullopt;
		}

		void saturating_increment(std::uint8_t& aCount) noexcept
		{
			if (aCount < std::numeric_limits<std::uint8_t>::max()) {
				++aCount;
			}
		}
	}

	auto stable_name(const feedback_label_columns_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_label_columns_refusal::too_many:
			return "too_many";
		case feedback_label_columns_refusal::unknown_label:
			return "unknown_label";
		case feedback_label_columns_refusal::duplicate_label:
			return "duplicate_label";
		case feedback_label_columns_refusal::noncanonical:
			return "noncanonical";
		}
		return "unknown";
	}

	auto make_feedback_label_columns(const std::span<const feedback_label> aLabels)
		-> std::expected<feedback_label_columns, feedback_label_columns_refusal>
	{
		if (aLabels.size() > feedback_label_count) {
			return std::unexpected(feedback_label_columns_refusal::too_many);
		}
		feedback_label_columns columns;
		for (const auto label : aLabels) {
			if (!catalog_label(label)) {
				return std::unexpected(feedback_label_columns_refusal::unknown_label);
			}
			const auto taken = std::span{ columns.Labels }.first(columns.Count);
			if (std::ranges::find(taken, label) != taken.end()) {
				return std::unexpected(feedback_label_columns_refusal::duplicate_label);
			}
			columns.Labels[columns.Count] = label;
			++columns.Count;
		}
		return columns;
	}

	auto check_feedback_label_columns(const feedback_label_columns& aColumns) noexcept
		-> std::expected<void, feedback_label_columns_refusal>
	{
		if (aColumns.Count > feedback_label_count) {
			return std::unexpected(feedback_label_columns_refusal::too_many);
		}
		std::array<bool, feedback_label_count> seen{ };
		for (std::size_t index = 0U; index < aColumns.Count; ++index) {
			const auto label = aColumns.Labels[index];
			if (!catalog_label(label)) {
				return std::unexpected(feedback_label_columns_refusal::unknown_label);
			}
			if (seen[static_cast<std::size_t>(label)]) {
				return std::unexpected(feedback_label_columns_refusal::duplicate_label);
			}
			seen[static_cast<std::size_t>(label)] = true;
		}
		for (std::size_t index = aColumns.Count; index < feedback_label_count; ++index) {
			if (aColumns.Labels[index] != feedback_label{ }) {
				return std::unexpected(feedback_label_columns_refusal::noncanonical);
			}
		}
		return { };
	}

	auto labels_of(const feedback_label_columns& aColumns) noexcept -> std::span<const feedback_label>
	{
		return std::span{ aColumns.Labels }.first(std::min<std::size_t>(aColumns.Count, feedback_label_count));
	}

	auto feedback_label_columns_text(const feedback_label_columns& aColumns) -> std::string
	{
		std::string text;
		for (const auto label : labels_of(aColumns)) {
			if (!text.empty()) {
				text.push_back(',');
			}
			text.append(stable_name(label));
		}
		return text;
	}

	auto read_feedback_label_columns(std::string_view aText) -> feedback_label_columns_reading
	{
		feedback_label_columns_reading reading;
		if (aText.size() > maximum_feedback_label_columns_text) {
			const auto bounded = aText.substr(0U, maximum_feedback_label_columns_text);
			const auto comma   = bounded.rfind(',');
			aText              = comma == std::string_view::npos ? std::string_view{ } : bounded.substr(0U, comma);
			saturating_increment(reading.Unknown);
			if (aText.empty()) {
				return reading;
			}
		}
		if (aText.empty()) {
			return reading;
		}
		std::array<bool, feedback_label_count> taken{ };
		std::size_t                            start = 0U;
		for (;;) {
			const auto end   = aText.find(',', start);
			const auto entry = aText.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			if (const auto label = label_named(entry); !label.has_value()) {
				saturating_increment(reading.Unknown);
			} else if (taken[static_cast<std::size_t>(*label)]) {
				saturating_increment(reading.Repeated);
			} else {
				taken[static_cast<std::size_t>(*label)]       = true;
				reading.Columns.Labels[reading.Columns.Count] = *label;
				++reading.Columns.Count;
			}
			if (end == std::string_view::npos) {
				break;
			}
			start = end + 1U;
		}
		return reading;
	}
}
