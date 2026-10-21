#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/core/discovery_diagnosis.hpp"
#include "bpr/core/review_submission_unknown.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

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
			constexpr std::array<std::string_view, 8> operations{ "application", "github.viewer", "github.capability", "github.pull_request", "epoch", "github.review_comment", "github.review_submission", "github.discovery" };
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
			constexpr std::array<std::string_view, 8> operations{ "application", "github.viewer", "github.capability", "github.pull_request", "epoch", "github.review_comment", "github.review_submission", "github.discovery" };
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
			std::size_t           inputRecords = 0U;
			std::vector<metadata> records;
			while (!aLog.empty()) {
				if (++inputRecords > release_diagnostic_input_records) {
					return false;
				}
				const auto end  = aLog.find('\n');
				const auto line = aLog.substr(0U, end);
				// An incomplete tail may be a concurrent append; do not classify it.
				auto record = end == std::string_view::npos ? std::nullopt : classify(line);
				if (record) {
					records.push_back(*record);
				} else {
					++aDocument.Descriptor.OmittedRecords;
				}
				if (end == std::string_view::npos) {
					break;
				}
				aLog.remove_prefix(end + 1U);
			}
			// [[BPR-1717]]. Input runs oldest to newest; over the limit the
			// oldest are left out, so the session being exported is never lost.
			const auto kept                          = std::min<std::size_t>(records.size(), core::release_diagnostic_record_limit);
			aDocument.Descriptor.LimitOmittedRecords = static_cast<std::uint32_t>(records.size() - kept);
			aDocument.Descriptor.OmittedRecords += aDocument.Descriptor.LimitOmittedRecords;
			for (auto record = records.end() - static_cast<std::ptrdiff_t>(kept); record != records.end(); ++record) {
				append_record(aDocument.Bytes, *record);
				++aDocument.Descriptor.Records;
			}
			return aDocument.Bytes.size() <= core::release_diagnostic_byte_limit;
		}
		using export_policy = core::diagnostic_export_policy;
		using export_scope  = core::diagnostic_export_scope;
		using json_values   = std::array<std::string_view, 30>;

		template <std::size_t N>
		auto member(std::string_view aValue, const std::array<std::string_view, N>& aNames) -> bool
		{
			return std::ranges::find(aNames, aValue) != aNames.end();
		}
		auto hex_identity(std::string_view aValue, std::size_t aSize) -> bool
		{
			return aValue.size() == aSize && std::ranges::all_of(aValue, [](char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); });
		}
		auto capture_label(std::string_view aLabel, std::string_view aSession, std::string_view aPrefix) -> bool
		{
			if (aLabel == "none" || aLabel == "omitted_limit") {
				return true;
			}
			if (!aLabel.starts_with(aPrefix)) {
				return false;
			}
			aLabel.remove_prefix(aPrefix.size());
			if (!aLabel.starts_with(aSession) || aLabel.size() <= aSession.size() || aLabel[aSession.size()] != '-') {
				return false;
			}
			return number(aLabel.substr(aSession.size() + 1U)).value_or(0U) != 0U;
		}
		auto build_identity(std::string_view aBuild) -> bool
		{
			const auto first = aBuild.find('-');
			if (first == std::string_view::npos || first == 0U || first > 32U) {
				return false;
			}
			const auto version = aBuild.substr(0U, first);
			if (!std::ranges::all_of(version, [](char aByte) { return (aByte >= '0' && aByte <= '9') || aByte == '.'; })) {
				return false;
			}
			aBuild.remove_prefix(first + 1U);
			constexpr std::array<std::string_view, 4> configurations{ "-Debug", "-Release", "-RelWithDebInfo", "-MinSizeRel" };
			return aBuild.size() > 64U && hex_identity(aBuild.substr(0U, 64U), 64U) && member(aBuild.substr(64U), configurations);
		}
		auto classified_values(const json_values& aValues) -> bool
		{
			constexpr std::array<std::string_view, 10>  stages{ "none", "catalog_open", "catalog_recovery", "catalog_prepare", "cache_open", "pack_sink", "smart_acquire", "protocol_acquire", "cache_inventory", "revision_prepare" };
			constexpr std::array<std::string_view, 9>   domains{ "none", "catalog", "cache", "transport", "protocol", "epoch", "github", "validation", "review_submission" };
			constexpr std::array<std::string_view, 17>  gitSteps{ "none", "admission", "temporary_store", "process_runner", "identity_before", "cli_version", "git_version", "cli_qualification", "git_qualification", "clone", "object_check", "identity_after", "local_store", "local_pack", "pack_read", "git_capability_probe", "cli_capability_probe" };
			constexpr std::array<std::string_view, 4>   transportSources{ "none", "native_transport", "http_response", "network_reply" };
			constexpr std::array<std::string_view, 14>  pagedOperations{ "none", "GetViewerIdentity", "GetRepository", "GetPullRequest", "ListPullRequestFiles", "ListPullRequestCommits", "ListReviews", "ListReviewThreads", "ListIssueComments", "ListChecks", "SearchPullRequests", "ListReviewComments", "ListPullRequestTimeline", "GetMergeBase" };
			constexpr std::array<std::string_view, 5>   summaries{ "absent", "sanitized", "omitted_invalid", "omitted_limit", "omitted_failure" };
			constexpr std::array<std::string_view, 125> failures{ "allocation", "allocation_limit", "api_version", "authentication", "authority_mismatch", "authorization_mismatch", "busy", "cancelled", "capability", "catalog_conflict", "catalog_mismatch", "certificate", "cli_unavailable", "collection_too_large", "collision_detected", "consumer_rejected", "duplicate_identity", "duplicate_ref", "identity_mismatch", "incomplete_response", "input_limit", "integrity_failure", "invalid_authority", "invalid_capability_revision", "invalid_commit_oid", "invalid_context", "invalid_enum", "invalid_limits", "invalid_line", "invalid_manifest", "invalid_path", "invalid_permissions", "invalid_pull_request_number", "invalid_rate_limit", "invalid_relationship", "invalid_repository", "invalid_request", "invalid_state", "invalid_text", "inventory_mismatch", "lock_unavailable", "malformed_observation", "malformed_pack", "malformed_packet", "malformed_response", "missing_capability", "missing_identity", "missing_required_text", "network", "none", "object_format_mismatch", "object_mismatch", "oid_mismatch", "operation_conflict", "operation_identity_conflict", "ordinal_exhausted", "pack_limit", "pack_sink", "permission", "phase_conflict", "platform_unsupported", "process_failure", "progress_limit", "proxy", "quota_exceeded", "rate_limit", "redirect", "ref_conflict", "relationship_actor_login", "relationship_file_previous_path", "relationship_file_viewed_capability", "relationship_review_comments_availability", "relationship_review_group_reply", "relationship_review_group_root", "relationship_review_threads_capability", "relationship_search_availability", "relationship_search_repository", "relationship_search_target_identity", "relationship_search_target_metadata", "relationship_thread_comment_path", "relationship_thread_permission_capability", "relationship_thread_reply", "relationship_timeline_availability", "relationship_timeline_event", "repair_required", "request_limit", "response_limit", "response_too_large", "revision_conflict", "schema_mismatch", "sideband_error", "source_observation_conflict", "spool_failure", "storage", "text_too_long", "timeout", "total_text_too_large", "unavailable", "unexpected_ref", "unexpected_section", "unknown", "unsupported_object", "unsupported_version", "vault", "git_tool_unsupported", "git_tool_probe_failed", "gh_tool_unsupported", "gh_tool_probe_failed", "review_not_listed", "several_reviews_listed", "review_list_unreadable", "review_list_too_large", "review_list_paged", "comment_page_unreadable", "comment_page_too_large", "comment_page_link_refused", "comment_collection_too_long", "comments_mismatch", "comment_moved_without_original_position", "http_status", "rate_limited", "account_mismatch", "pull_request_mismatch", "connection_unavailable", "local_record_unavailable" };
			constexpr std::array<std::string_view, 8>   packFailures{ "none", "unknown", "invalid_request", "invalid_state", "input_limit", "storage", "malformed_pack", "cancelled" };
			constexpr std::array<std::string_view, 37>  validationFailures{ "none", "invalid_context", "invalid_limits", "missing_identity", "identity_mismatch", "missing_required_text", "invalid_text", "text_too_long", "invalid_repository", "invalid_pull_request_number", "invalid_commit_oid", "invalid_line", "invalid_enum", "invalid_capability_revision", "invalid_rate_limit", "collection_too_large", "duplicate_identity", "invalid_relationship", "allocation_limit", "total_text_too_large", "relationship_file_previous_path", "relationship_thread_comment_path", "relationship_review_group_root", "relationship_review_group_reply", "relationship_actor_login", "relationship_thread_reply", "relationship_search_repository", "relationship_search_target_identity", "relationship_search_target_metadata", "relationship_timeline_availability", "relationship_timeline_event", "relationship_review_comments_availability", "relationship_review_threads_capability", "relationship_file_viewed_capability", "relationship_thread_permission_capability", "relationship_search_availability", "unknown" };
			if (!hex_identity(aValues[6], 32U) || !build_identity(aValues[8]) || number(aValues[7]).value_or(0U) == 0U
				|| number(aValues[4]).value_or(0U) > 253402300799999ULL) {
				return false;
			}
			if (!member(aValues[13], summaries) || !member(aValues[16], stages) || !member(aValues[17], domains)
				|| !member(aValues[18], failures) || !member(aValues[19], packFailures) || !member(aValues[20], pagedOperations)
				|| !member(aValues[22], validationFailures) || !member(aValues[23], transportSources) || !member(aValues[24], gitSteps)) {
				return false;
			}
			const auto category = aValues[9] == "application" ? "application" : aValues[9] == "epoch" ? "epoch"
																									  : "github";
			const auto severity = aValues[10] == "failed" ? "error" : *number(aValues[12]) >= 400U ? "warning"
																								   : "info";
			if (aValues[1] != std::string{ aValues[9] } + "." + std::string{ aValues[10] } || aValues[2] != category || aValues[3] != severity) {
				return false;
			}
			if (aValues[0] == "3") {
				if ((aValues[27] != "anonymized" && aValues[27] != "include_locally")
					|| (aValues[28] != "standard" && aValues[28] != "verbose") || (aValues[29] != "standard" && aValues[29] != "verbose")
					|| !capture_label(aValues[25], aValues[6], "request-") || !capture_label(aValues[26], aValues[6], "window-")
					|| (aValues[27] == "anonymized" && (aValues[11] != "0" || aValues[21] != "0"))) {
					return false;
				}
			}
			return true;
		}
		auto public_symbol(std::string_view aSymbol) -> bool
		{
			// Exact symbols from the shipped capability query vocabulary, never identifier shape.
			constexpr std::array<std::string_view, 84> symbols{ "Actor", "BetterPullReviewCapabilityProbeV1Part0", "BetterPullReviewCapabilityProbeV1Part1", "BetterPullReviewCapabilityProbeV1Part2", "BetterPullReviewCapabilityProbeV1Part3", "BetterPullReviewCapabilityProbeV1Part4", "BetterPullReviewCapabilityProbeV1Part5", "BetterPullReviewCapabilityProbeV1Part6", "BetterPullReviewCapabilityProbeV1Part7", "BetterPullReviewCapabilityProbeV1Part8", "DiffSide", "EnumType", "FieldType", "FileViewedState", "InputType", "InterfaceType", "MarkFileAsViewedInput", "MarkFileAsViewedPayload", "Node", "PageInfo", "PossibleType", "PullRequest", "PullRequestChangedFile", "PullRequestChangedFileConnection", "PullRequestReviewComment", "PullRequestReviewCommentConnection", "PullRequestReviewThread", "PullRequestReviewThreadConnection", "PullRequestReviewThreadSubjectType", "Repository", "ResolveReviewThreadInput", "ResolveReviewThreadPayload", "TypeRef", "UnmarkFileAsViewedInput", "UnmarkFileAsViewedPayload", "UnresolveReviewThreadInput", "UnresolveReviewThreadPayload", "__Type", "__schema", "__type", "actor", "actorPossibleTypes", "args", "changedFile", "changedFileConnection", "commentConnection", "defaultValue", "diffSide", "enumValues", "fields", "fileViewedState", "fragment", "includeDeprecated", "inputFields", "interfaces", "kind", "markInput", "markPayload", "mutationType", "name", "node", "nodePossibleTypes", "ofType", "on", "pageInfo", "possibleTypes", "pullRequest", "query", "queryType", "repository", "resolveInput", "resolvePayload", "reviewComment", "reviewThread", "reviewThreadInterfaces", "schemaRoot", "threadConnection", "threadSubject", "true", "type", "unmarkInput", "unmarkPayload", "unresolveInput", "unresolvePayload" };
			return aSymbol == "[omitted:unclassified-symbol]" || member(aSymbol, symbols);
		}
		auto safe_message(std::string_view aMessage) -> bool
		{
			constexpr std::string_view omission = "[omitted:unclassified-content]";
			if (aMessage == omission) {
				return true;
			}
			if (aMessage.ends_with(" [omitted:unclassified-content]")) {
				aMessage.remove_suffix(omission.size() + 1U);
			}
			if (aMessage.starts_with("Unknown schema field ")) {
				return public_symbol(aMessage.substr(21U));
			}
			if (!aMessage.starts_with("Field ")) {
				return false;
			}
			aMessage.remove_prefix(6U);
			constexpr std::string_view absent = " does not exist on type ";
			const auto                 split  = aMessage.find(absent);
			if (split != std::string_view::npos) {
				return public_symbol(aMessage.substr(0U, split)) && public_symbol(aMessage.substr(split + absent.size()));
			}
			constexpr std::string_view repeated = " exceeds a service repetition limit";
			return aMessage.ends_with(repeated) && public_symbol(aMessage.substr(0U, aMessage.size() - repeated.size()));
		}
		auto safe_actor_conflict_detail(std::string_view aDetail) -> bool
		{
			constexpr std::array<std::string_view, 8U> origins{ "viewer", "pull_request", "commit", "review", "thread_comment", "review_comment_root", "review_comment_reply", "issue_comment" };
			constexpr std::string_view                 prefix = "actor_conflict=";
			if (!aDetail.starts_with(prefix)) {
				return false;
			}
			aDetail.remove_prefix(prefix.size());
			const auto first = aDetail.find(',');
			if (first == std::string_view::npos || !member(aDetail.substr(0U, first), origins)) {
				return false;
			}
			aDetail.remove_prefix(first + 1U);
			const auto conflicting = aDetail.find(',');
			return conflicting != std::string_view::npos && member(aDetail.substr(0U, conflicting), origins)
				&& (aDetail.substr(conflicting + 1U) == "ascii_case_only" || aDetail.substr(conflicting + 1U) == "different");
		}
		auto safe_detail(std::string_view aDetail) -> bool
		{
			if (aDetail.starts_with("actor_conflict=")) {
				return safe_actor_conflict_detail(aDetail);
			}
			// [[BPR-PENDING-AD]]. Exactly a known submission ending's line.
			if (aDetail.starts_with("review_submission_ending=")) {
				return core::parse_review_submission_ending(aDetail).has_value();
			}
			// [[BPR-1540]]. Exactly a counts-only review submission reason.
			if (aDetail.starts_with("review_submission=")) {
				return core::parse_review_submission_unknown(aDetail).has_value();
			}
			// [[BPR-1680]]. Exactly a Home discovery line of enums and counts.
			if (aDetail.starts_with("discovery=")) {
				return core::parse_discovery_diagnosis(aDetail).has_value();
			}
			constexpr std::array<std::string_view, 19> codes{ "FORBIDDEN", "UNAUTHORIZED", "NOT_FOUND", "MAX_NODE_LIMIT_EXCEEDED", "INTERNAL", "SERVICE_UNAVAILABLE", "GRAPHQL_PARSE_FAILED", "GRAPHQL_VALIDATION_FAILED", "undefinedField", "argumentNotAccepted", "undefinedType", "selectionMismatch", "variableMismatch", "variableNotUsed", "variableRequiresValidType", "variableNotDefined", "RATE_LIMITED", "[omitted:unclassified-code]", "[omitted:unclassified-content]" };
			if (aDetail.size() > 4096U) {
				return false;
			}
			while (!aDetail.empty()) {
				const bool message = aDetail.starts_with("message=\\\"");
				const auto prefix  = message ? 10U : (aDetail.starts_with("type=\\\"") || aDetail.starts_with("code=\\\"")) ? 7U
																															: 0U;
				if (prefix == 0U) {
					return false;
				}
				aDetail.remove_prefix(prefix);
				const auto end = aDetail.find("\\\" ");
				if (end == std::string_view::npos) {
					return false;
				}
				const auto value = aDetail.substr(0U, end);
				if (message ? !safe_message(value) : !member(value, codes)) {
					return false;
				}
				aDetail.remove_prefix(end + 3U);
			}
			return true;
		}

		// [[BPR-1717]]. One admitted record, kept as a view until the
		// newest records that fit are chosen.
		struct export_candidate
		{
			std::string_view Line;
			std::string_view Session;
			std::uint64_t    UtcMs    = 0U;
			std::uint64_t    Sequence = 0U;
			bool             Recent   = false;
			bool             Legacy   = false;
		};
		// Room left for the header and the Insights section that follow the records.
		constexpr std::size_t export_reserve = 4096U;

		struct export_builder
		{
			release_diagnostic_document                     Document;
			std::string                                     Namespace;
			std::map<std::string, std::string>              Labels;
			std::set<std::pair<std::string, std::uint64_t>> Seen;
			std::set<std::string>                           Sessions;
			std::vector<export_candidate>                   Candidates;
			std::uint32_t                                   DuplicateRecords = 0U;
			std::uint32_t                                   DetailOmissions  = 0U;
			std::uint32_t                                   LegacyRecords    = 0U;
			std::uint32_t                                   Unclassified     = 0U;
			std::size_t                                     InputRecords     = 0U;
			bool                                            Dated            = false;

			auto label(std::string_view aKind, std::string_view aSession, std::string_view aIdentity, bool aMeasure) -> std::string
			{
				if (aIdentity == "none" || aIdentity == "0" || aIdentity == "omitted_limit") {
					return std::string{ aIdentity };
				}
				if (aMeasure) {
					// Wider than any real ordinal: at most three labels per kept record.
					return std::string{ aKind } + "-" + Namespace + "-99999";
				}
				const auto key            = std::string{ aKind } + ":" + std::string{ aSession } + ":" + std::string{ aIdentity };
				auto [position, inserted] = Labels.try_emplace(key);
				if (inserted) {
					position->second = std::string{ aKind } + "-" + Namespace + "-" + std::to_string(Labels.size());
				}
				return position->second;
			}
			/// \brief The exported line; with `aMeasure`, an upper bound on its size that assigns no label.
			auto render(const json_values& aValues, bool aRecent, bool aMeasure) -> std::string
			{
				const bool  identifying = Document.Descriptor.Options.Policy == export_policy::identifying;
				std::string bytes       = "{\"record\":\"event\"";
				const auto  text        = [&](std::string_view aKey, std::string_view aValue) { bytes.append(",\"").append(aKey).append("\":\"").append(aValue).append("\""); };
				const auto  numeric     = [&](std::string_view aKey, std::string_view aValue) { bytes.append(",\"").append(aKey).append("\":").append(aValue); };
				text("event", aValues[1]);
				text("category", aValues[2]);
				text("severity", aValues[3]);
				numeric("utc_ms", aValues[4]);
				numeric("elapsed_us", aValues[5]);
				text("session", identifying ? std::string{ aValues[6] } : label("session", "", aValues[6], aMeasure));
				numeric("sequence", aValues[7]);
				text("build", aValues[8]);
				text("op", aValues[9]);
				text("outcome", aValues[10]);
				numeric("http_status", aValues[12]);
				text("summary", aValues[13]);
				numeric("dropped", aValues[14]);
				const bool safe = (aValues[15].empty() || aValues[13] == "sanitized") && safe_detail(aValues[15]);
				text("detail", safe ? aValues[15] : "[omitted:unclassified-content]");
				if (!safe && !aMeasure) {
					++DetailOmissions;
				}
				constexpr std::array<std::string_view, 9> keys{ "epoch_stage", "failure_domain", "failure", "pack_failure", "page_operation", "window_session", "relationship_detail", "transport_source", "git_step" };
				for (std::size_t index = 0U; index < keys.size(); ++index) {
					if (index != 5U) {
						text(keys[index], aValues[16U + index]);
					}
				}
				const auto request = aValues[0] == "3" ? aValues[25] : aValues[11];
				const auto window  = aValues[0] == "3" ? aValues[26] : aValues[21];
				text("request_label", label("request", aValues[6], request, aMeasure));
				text("window_label", label("window", aValues[6], window, aMeasure));
				if (identifying) {
					numeric("request", aValues[11]);
					numeric("window_session", aValues[21]);
				}
				text("capture_privacy", aValues[0] == "3" ? aValues[27] : "legacy_identifying");
				text("capture_detail", aValues[0] == "3" ? aValues[28] : "legacy_unknown");
				text("event_detail", aValues[0] == "3" ? aValues[29] : "standard");
				text("source", aRecent ? "recent_context" : "file");
				return bytes + "}\n";
			}
			static auto legacy_line(const metadata& aRecord) -> std::string
			{
				return "{\"record\":\"legacy_metadata\",\"operation\":" + std::to_string(aRecord.Operation) + ",\"outcome\":" + std::to_string(aRecord.Outcome) + ",\"http_status\":" + std::to_string(aRecord.HttpStatus) + ",\"dropped\":" + std::to_string(aRecord.Dropped) + "}\n";
			}
			/// \brief Classifies one record and admits it as a candidate; nothing is rendered yet.
			auto add(std::string_view aLine, bool aRecent, const diagnostic_export_capture& aCapture) -> bool
			{
				if (++InputRecords > release_diagnostic_input_records) {
					return false;
				}
				const auto record = classify(aLine);
				const auto values = json_fields(aLine);
				if (!record || (aLine.starts_with('{') && (!values || !classified_values(*values)))) {
					++Unclassified;
					return true;
				}
				if (!values) {
					if (Document.Descriptor.Options.Scope != export_scope::current_session && !aRecent) {
						Candidates.push_back({ .Line = aLine, .Legacy = true });
					}
					return true;
				}
				const auto& fields   = *values;
				const auto  sequence = *number(fields[7]);
				if ((Document.Descriptor.Options.Scope == export_scope::current_session && fields[6] != aCapture.Session)
					|| (fields[6] == aCapture.Session && sequence > aCapture.Sequence)) {
					return true;
				}
				if (!Seen.emplace(std::string{ fields[6] }, sequence).second) {
					++DuplicateRecords;
					return true;
				}
				Candidates.push_back({ .Line = aLine, .Session = fields[6], .UtcMs = *number(fields[4]), .Sequence = sequence, .Recent = aRecent });
				return true;
			}
			auto collect(std::string_view aLog, bool aRecent, const diagnostic_export_capture& aCapture) -> bool
			{
				while (!aLog.empty()) {
					const auto end = aLog.find('\n');
					if (end == std::string_view::npos) {
						++Unclassified;
						return ++InputRecords <= release_diagnostic_input_records;
					}
					if (!add(aLog.substr(0U, end), aRecent, aCapture)) {
						return false;
					}
					aLog.remove_prefix(end + 1U);
				}
				return true;
			}
			/// \brief [[BPR-1717]]. Orders every candidate oldest first and
			/// keeps the newest run that fits the record and byte limits.
			/// \details Input order says nothing about age: files arrive in the
			/// reader's order and recent context after them. Version-one lines
			/// carry no time and count as the oldest. Events order by time, then
			/// session and sequence, which the duplicate check makes unique.
			auto select() -> std::size_t
			{
				std::ranges::stable_sort(Candidates, [](const export_candidate& aLeft, const export_candidate& aRight) {
					if (aLeft.Legacy || aRight.Legacy) {
						return aLeft.Legacy && !aRight.Legacy;
					}
					return std::tie(aLeft.UtcMs, aLeft.Session, aLeft.Sequence) < std::tie(aRight.UtcMs, aRight.Session, aRight.Sequence);
				});
				constexpr auto budget = static_cast<std::size_t>(core::release_diagnostic_byte_limit) - export_reserve;
				std::size_t    used   = 0U;
				auto           first  = Candidates.size();
				while (first != 0U && Candidates.size() - first < core::release_diagnostic_record_limit) {
					const auto& candidate = Candidates[first - 1U];
					const auto  size      = candidate.Legacy ? legacy_line(*classify(candidate.Line)).size() : render(*json_fields(candidate.Line), candidate.Recent, true).size();
					if (size > budget - used) {
						break;
					}
					used += size;
					--first;
				}
				return first;
			}
			void emit(std::size_t aFirst)
			{
				auto& descriptor               = Document.Descriptor;
				descriptor.LimitOmittedRecords = static_cast<std::uint32_t>(aFirst);
				descriptor.OmittedRecords      = Unclassified + descriptor.LimitOmittedRecords;
				for (auto index = aFirst; index < Candidates.size(); ++index) {
					const auto& candidate = Candidates[index];
					++descriptor.Records;
					if (candidate.Legacy) {
						const auto record = *classify(candidate.Line);
						Document.Bytes += legacy_line(record);
						descriptor.Categories |= record.Operation == 0U ? 1U : record.Operation == 4U ? 4U
																									  : 2U;
						++LegacyRecords;
						continue;
					}
					const auto fields = *json_fields(candidate.Line);
					Document.Bytes += render(fields, candidate.Recent, false);
					descriptor.RecentRecords += candidate.Recent ? 1U : 0U;
					descriptor.CaptureRedactedRecords += fields[27] == "anonymized" ? 1U : 0U;
					descriptor.Categories |= fields[2] == "application" ? 1U : fields[2] == "github" ? 2U
																									 : 4U;
					Sessions.emplace(fields[6]);
					descriptor.FirstUtcMs = Dated ? std::min(descriptor.FirstUtcMs, candidate.UtcMs) : candidate.UtcMs;
					descriptor.LastUtcMs  = std::max(descriptor.LastUtcMs, candidate.UtcMs);
					Dated                 = true;
				}
			}
		};

	}
	auto format_diagnostic_export_time(const diagnostic_export_stamp& aStamp) -> std::string
	{
		constexpr std::int32_t widestOffset = 18 * 60;
		if (aStamp.UtcMs == 0U || aStamp.OffsetMinutes < -widestOffset || aStamp.OffsetMinutes > widestOffset || aStamp.UtcMs > 253402300799999ULL) {
			return "unknown";
		}
		const auto local  = std::chrono::sys_seconds{ std::chrono::seconds{ static_cast<std::int64_t>(aStamp.UtcMs / 1000U) + (std::int64_t{ aStamp.OffsetMinutes } * 60) } };
		const auto day    = std::chrono::floor<std::chrono::days>(local);
		const auto date   = std::chrono::year_month_day{ day };
		const auto time   = std::chrono::hh_mm_ss{ local - day };
		const auto offset = aStamp.OffsetMinutes < 0 ? -aStamp.OffsetMinutes : aStamp.OffsetMinutes;
		const auto padded = [](std::int64_t aValue, std::size_t aWidth) {
			auto text = std::to_string(aValue);
			return std::string(aWidth > text.size() ? aWidth - text.size() : 0U, '0') + text;
		};
		auto result = padded(static_cast<int>(date.year()), 4U) + "-" + padded(static_cast<unsigned>(date.month()), 2U) + "-" + padded(static_cast<unsigned>(date.day()), 2U);
		result += "T" + padded(time.hours().count(), 2U) + ":" + padded(time.minutes().count(), 2U) + ":" + padded(time.seconds().count(), 2U);
		result += (aStamp.OffsetMinutes < 0 ? "-" : "+") + padded(offset / 60, 2U) + ":" + padded(offset % 60, 2U);
		return result;
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
	auto prepare_diagnostic_export(std::string_view aLog, core::release_maintenance_revision aRevision,
								   core::diagnostic_export_options aOptions, const diagnostic_export_capture& aCapture, std::string_view aNamespace, std::uint32_t aUnreadFiles, const diagnostic_export_stamp& aStamp)
		-> std::expected<release_diagnostic_document, core::release_maintenance_reason>
	{
		using reason = core::release_maintenance_reason;
		if (!core::valid_diagnostic_export_options(aOptions) || aOptions.Policy == export_policy::metadata || aRevision.value() == 0U || !hex_identity(aNamespace, 32U)
			|| (aOptions.Scope == export_scope::current_session && !hex_identity(aCapture.Session, 32U))) {
			return std::unexpected(reason::invalid_selection);
		}
		if (aLog.size() > release_diagnostic_input_limit || aCapture.Recent.size() > 512U) {
			return std::unexpected(reason::limit_exceeded);
		}
		// Filled member by member: GCC 15 at -O3 reads the nested designated
		// aggregate as leaving the document's strings uninitialized.
		export_builder builder{ };
		builder.Document.Descriptor.Revision    = aRevision;
		builder.Document.Descriptor.Options     = aOptions;
		builder.Document.Descriptor.UnreadFiles = aUnreadFiles;
		builder.Namespace                       = std::string{ aNamespace };
		if (!builder.collect(aLog, false, aCapture)) {
			return std::unexpected(reason::limit_exceeded);
		}
		std::size_t recentBytes = 0U;
		if (aOptions.IncludeRecent) {
			for (const auto& record : aCapture.Recent) {
				if (record.size() > 2U * 1024U * 1024U - recentBytes) {
					return std::unexpected(reason::limit_exceeded);
				}
				recentBytes += record.size();
				if (!builder.add(record, true, aCapture)) {
					return std::unexpected(reason::limit_exceeded);
				}
			}
		}
		builder.emit(builder.select());
		auto& document = builder.Document;
		document.Descriptor.Fields |= core::release_diagnostic_classified_fields | (aOptions.Policy == export_policy::identifying ? static_cast<std::uint32_t>(core::release_diagnostic_field::captured_identity) : 0U);
		document.Descriptor.Sessions = static_cast<std::uint32_t>(builder.Sessions.size());
		// [[BPR-PENDING-AD]]. When it was taken and by which build, so a saved
		// export says so without being renamed.
		const bool  built  = build_identity(aCapture.Build);
		std::string header = "{\"format\":\"bpr-diagnostics-v2\",\"exported_at\":\"" + format_diagnostic_export_time(aStamp) + "\",\"app_version\":\"";
		header += built ? aCapture.Build.substr(0U, aCapture.Build.find('-')) : std::string{ "unknown" };
		header += "\",\"app_build\":\"";
		header += built ? aCapture.Build : std::string{ "unknown" };
		header += "\",\"policy\":\"";
		header += aOptions.Policy == export_policy::identifying ? "identifying" : "anonymized";
		header += "\",\"scope\":\"";
		header += aOptions.Scope == export_scope::current_session ? "current_session" : "retained_sessions";
		header += "\",\"include_recent\":";
		header += aOptions.IncludeRecent ? "true" : "false";
		header += ",\"records\":" + std::to_string(document.Descriptor.Records) + ",\"omitted_records\":" + std::to_string(document.Descriptor.OmittedRecords)
			+ ",\"unclassified_records\":" + std::to_string(builder.Unclassified) + ",\"limit_omitted_records\":" + std::to_string(document.Descriptor.LimitOmittedRecords)
			+ ",\"limit_keeps\":\"newest\",\"unread_files\":" + std::to_string(aUnreadFiles)
			+ ",\"duplicate_records\":" + std::to_string(builder.DuplicateRecords) + ",\"detail_omissions\":" + std::to_string(builder.DetailOmissions)
			+ ",\"legacy_metadata_records\":" + std::to_string(builder.LegacyRecords)
			+ ",\"capture_redacted_records\":" + std::to_string(document.Descriptor.CaptureRedactedRecords)
			+ ",\"sessions\":" + std::to_string(document.Descriptor.Sessions) + ",\"categories\":" + std::to_string(document.Descriptor.Categories)
			+ ",\"first_utc_ms\":" + std::to_string(document.Descriptor.FirstUtcMs) + ",\"last_utc_ms\":" + std::to_string(document.Descriptor.LastUtcMs)
			+ ",\"recent_records\":" + std::to_string(document.Descriptor.RecentRecords)
			+ ",\"notice\":\"Capture-time omissions cannot be recovered. Over the record or byte limit the oldest records are left out and the newest kept. Sanitized explanations and operational facts may still reveal context. Local file only; nothing is uploaded.\"}\n";
		document.Bytes.insert(0U, header);
		if (document.Bytes.size() > core::release_diagnostic_byte_limit) {
			return std::unexpected(reason::limit_exceeded);
		}
		document.Descriptor.Bytes  = document.Bytes.size();
		document.Descriptor.Digest = core::sha256({ reinterpret_cast<const std::uint8_t*>(document.Bytes.data()), document.Bytes.size() });
		return std::move(document);
	}

}
