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

		// v2 is a canonical flat JSON object. Only this exact field order and
		// scalar grammar are supported; other versions/unknown fields are omitted.
		// Views refer to bounded input and no free text is copied into the export.
		auto json_string(std::string_view& aInput) -> std::optional<std::string_view>
		{
			if (aInput.empty() || aInput.front() != '"') {
				return std::nullopt;
			}
			aInput.remove_prefix(1U);
			std::size_t size = 0U;
			while (size < aInput.size() && size <= 4096U) {
				const auto value = static_cast<unsigned char>(aInput[size]);
				if (value == '"') {
					const auto result = aInput.substr(0U, size);
					aInput.remove_prefix(size + 1U);
					return result;
				}
				if (value < 32U || value >= 127U) {
					return std::nullopt;
				}
				if (value == '\\') {
					++size;
					if (size == aInput.size() || (aInput[size] != '"' && aInput[size] != '\\')) {
						return std::nullopt;
					}
				}
				++size;
			}
			return std::nullopt;
		}
		auto json_fields(std::string_view aInput) -> std::optional<std::array<std::string_view, 30>>
		{
			constexpr std::array<std::string_view, 30> keys{
				"v", "event", "category", "severity", "utc_ms", "elapsed_us", "session", "sequence", "build",
				"op", "outcome", "request", "http_status", "summary", "dropped", "detail", "epoch_stage", "failure_domain",
				"failure", "pack_failure", "page_operation", "window_session", "relationship_detail", "transport_source", "git_step", "request_label", "window_label", "capture_privacy", "capture_detail", "event_detail"
			};
			std::array<std::string_view, 30> fields{ };
			if (!aInput.starts_with('{') || aInput.size() >= 16U * 1024U) {
				return std::nullopt;
			}
			const auto fieldCount = aInput.starts_with("{\"v\":3,") ? 30U : 25U;
			aInput.remove_prefix(1U);
			for (std::size_t index = 0U; index < fieldCount; ++index) {
				if (json_string(aInput) != keys[index] || !aInput.starts_with(':')) {
					return std::nullopt;
				}
				aInput.remove_prefix(1U);
				const bool numeric = index == 0U || index == 4U || index == 5U || index == 7U || index == 11U || index == 12U || index == 14U || index == 21U;
				if (numeric) {
					const auto end   = aInput.find(',');
					const auto value = aInput.substr(0U, end);
					if (end == std::string_view::npos || !number(value) || (value.size() > 1U && value.front() == '0')) {
						return std::nullopt;
					}
					fields[index] = value;
					aInput.remove_prefix(end);
				} else {
					const auto value = json_string(aInput);
					if (!value) {
						return std::nullopt;
					}
					fields[index] = *value;
				}
				if (index + 1U == fieldCount) {
					if (aInput != "}") {
						return std::nullopt;
					}
				} else if (!aInput.starts_with(',')) {
					return std::nullopt;
				} else {
					aInput.remove_prefix(1U);
				}
			}
			return fields;
		}
		auto classify_json(std::string_view aLine) -> std::optional<metadata>
		{
			const auto fields = json_fields(aLine);
			if (!fields || ((*fields)[0] != "2" && (*fields)[0] != "3")) {
				return std::nullopt;
			}
			constexpr std::array<std::string_view, 5> operations{ "application", "github.viewer", "github.capability", "github.pull_request", "epoch" };
			constexpr std::array<std::string_view, 6> outcomes{ "started", "succeeded", "failed", "cancelled", "response_received", "shutdown" };
			constexpr std::array<std::string_view, 5> summaries{ "absent", "sanitized", "omitted_invalid", "omitted_limit", "omitted_failure" };
			const auto&                               values  = *fields;
			const auto                                op      = std::ranges::find(operations, values[9]);
			const auto                                outcome = std::ranges::find(outcomes, values[10]);
			const auto                                http    = *number(values[12]);
			if (op == operations.end() || outcome == outcomes.end() || std::ranges::find(summaries, values[13]) == summaries.end()
				|| (http != 0U && (http < 100U || http > 599U))) {
				return std::nullopt;
			}
			return metadata{ static_cast<std::size_t>(op - operations.begin()), static_cast<std::size_t>(outcome - outcomes.begin()), http, *number(values[14]) };
		}
		auto classify(std::string_view aLine) -> std::optional<metadata>
		{
			if (aLine.starts_with('{')) {
				return classify_json(aLine);
			}
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
