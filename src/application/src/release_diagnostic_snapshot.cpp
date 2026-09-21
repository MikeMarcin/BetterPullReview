#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>

namespace bpr::application {
	namespace {
		auto field(std::string_view& aLine, std::string_view aKey) -> std::optional<std::string_view>
		{
			if (!aLine.starts_with(aKey)) {
				return std::nullopt;
			}
			aLine.remove_prefix(aKey.size());
			const auto end = aLine.find(' ');
			if (end == std::string_view::npos) {
				return std::nullopt;
			}
			auto result = aLine.substr(0U, end);
			aLine.remove_prefix(end + 1U);
			return result;
		}
		auto number(std::optional<std::string_view> aValue) -> std::optional<std::uint64_t>
		{
			if (!aValue || aValue->empty() || aValue->size() > 20U) {
				return std::nullopt;
			}
			std::uint64_t result = 0U;
			auto          parsed = std::from_chars(aValue->data(), aValue->data() + aValue->size(), result);
			return parsed.ec == std::errc{ } && parsed.ptr == aValue->data() + aValue->size() ? std::optional{ result } : std::nullopt;
		}
		struct metadata
		{
			std::size_t   Operation;
			std::size_t   Outcome;
			std::uint64_t HttpStatus;
			std::uint64_t Dropped;
		};
		auto classify(std::string_view aLine) -> std::optional<metadata>
		{
			constexpr std::array<std::string_view, 5> operations{ "application", "github.viewer", "github.capability", "github.pull_request", "epoch" };
			constexpr std::array<std::string_view, 6> outcomes{ "started", "succeeded", "failed", "cancelled", "response_received", "shutdown" };
			constexpr std::array<std::string_view, 5> summaries{ "absent", "sanitized", "omitted_invalid", "omitted_limit", "omitted_failure" };
			if (aLine.size() > 8192U) {
				return std::nullopt;
			}
			const auto timestamp = aLine.find(' ');
			if (timestamp == std::string_view::npos || timestamp > 64U) {
				return std::nullopt;
			}
			aLine.remove_prefix(timestamp + 1U);
			if (field(aLine, "v=") != "1") {
				return std::nullopt;
			}
			auto operation = field(aLine, "op=");
			auto outcome   = field(aLine, "outcome=");
			auto request   = number(field(aLine, "request="));
			auto http      = number(field(aLine, "http_status="));
			auto summary   = field(aLine, "summary=");
			auto dropped   = number(field(aLine, "dropped="));
			if (!operation || !outcome || !request || !http || !summary || !dropped || !aLine.starts_with("detail=")) {
				return std::nullopt;
			}
			auto op     = std::ranges::find(operations, *operation);
			auto result = std::ranges::find(outcomes, *outcome);
			if (op == operations.end() || result == outcomes.end() || std::ranges::find(summaries, *summary) == summaries.end()
				|| (*http != 0U && (*http < 100U || *http > 599U))) {
				return std::nullopt;
			}
			return metadata{ static_cast<std::size_t>(op - operations.begin()), static_cast<std::size_t>(result - outcomes.begin()), *http, *dropped };
		}
		void append_record(std::string& aBytes, const metadata& aRecord)
		{
			aBytes.append("operation=").append(std::to_string(aRecord.Operation));
			aBytes.append(" outcome=").append(std::to_string(aRecord.Outcome));
			aBytes.append(" http_status=").append(std::to_string(aRecord.HttpStatus));
			aBytes.append(" dropped=").append(std::to_string(aRecord.Dropped)).push_back('\n');
		}
		auto collect(std::string_view aLog, release_diagnostic_document& aDocument) -> bool
		{
			std::size_t inputRecords = 0U;
			while (!aLog.empty()) {
				if (++inputRecords > release_diagnostic_input_records) {
					return false;
				}
				const auto end  = aLog.find('\n');
				const auto line = aLog.substr(0U, end);
				// An incomplete tail may be a concurrent append; do not classify it.
				auto record = end == std::string_view::npos ? std::nullopt : classify(line);
				if (!record || aDocument.Descriptor.Records == core::release_diagnostic_record_limit) {
					++aDocument.Descriptor.OmittedRecords;
				} else {
					append_record(aDocument.Bytes, *record);
					++aDocument.Descriptor.Records;
				}
				if (aDocument.Bytes.size() > core::release_diagnostic_byte_limit) {
					return false;
				}
				if (end == std::string_view::npos) {
					break;
				}
				aLog.remove_prefix(end + 1U);
			}
			return true;
		}
	}
	auto prepare_release_diagnostics(std::string_view aLog, core::release_maintenance_revision aRevision)
		-> std::expected<release_diagnostic_document, core::release_maintenance_reason>
	{
		if (aRevision.value() == 0U) {
			return std::unexpected(core::release_maintenance_reason::invalid_selection);
		}
		if (aLog.size() > release_diagnostic_input_limit) {
			return std::unexpected(core::release_maintenance_reason::limit_exceeded);
		}
		release_diagnostic_document document{ { .Revision = aRevision }, "bpr-diagnostics-v1\n" };
		if (!collect(aLog, document)) {
			return std::unexpected(core::release_maintenance_reason::limit_exceeded);
		}
		document.Descriptor.Bytes  = document.Bytes.size();
		document.Descriptor.Digest = core::sha256({ reinterpret_cast<const std::uint8_t*>(document.Bytes.data()), document.Bytes.size() });
		return document;
	}
}
