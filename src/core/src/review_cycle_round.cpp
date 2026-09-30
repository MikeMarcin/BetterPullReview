#include "bpr/core/review_cycle_round.hpp"

#include "bpr/core/review_cycle_disclosure.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace bpr::core {
	namespace {
		using json = cycle_json_value;

		constexpr auto round_schema     = std::string_view{ "bpr.cycle.round.v1" };
		constexpr auto envelope_schema  = std::string_view{ "bpr.cycle.round-envelope.v1" };
		constexpr auto canonical_schema = std::string_view{ "bpr.cycle.canonical-json.v1" };
		constexpr auto draft_schema     = std::string_view{ "bpr.cycle.round-draft.v1" };

		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto digest_of(const std::string_view aText) -> cycle_material_digest
		{
			return cycle_material_digest{ sha256(bytes_of(aText)) };
		}

		// ---- JSON building ------------------------------------------------

		auto text(std::string aValue) -> json
		{
			return json{ std::move(aValue) };
		}

		auto integer(const std::uint64_t aValue) -> json
		{
			// A value beyond the exact range makes the document non-canonical.
			return json{ static_cast<std::int64_t>(std::min<std::uint64_t>(aValue, static_cast<std::uint64_t>(maximum_cycle_json_integer) + 1U)) };
		}

		auto optional_text(const std::optional<std::string>& aValue) -> json
		{
			return aValue ? text(*aValue) : json{ };
		}

		auto optional_integer(const std::optional<std::uint64_t>& aValue) -> json
		{
			return aValue ? integer(*aValue) : json{ };
		}

		auto texts(const std::vector<std::string>& aValues) -> json
		{
			auto items = json::array{ };
			items.reserve(aValues.size());
			for (const auto& value : aValues) {
				items.push_back(text(value));
			}
			return json{ std::move(items) };
		}

		auto hex(const cycle_material_digest& aDigest) -> std::string
		{
			return cycle_hex(aDigest.bytes());
		}

		auto byte_record(const std::string& aBytes) -> json
		{
			return json{ json::object{
				{ .Key = "length", .Value = integer(aBytes.size()) },
				{ .Key = "sha256", .Value = text(hex(digest_of(aBytes))) },
				{ .Key = "base64", .Value = text(encode_cycle_base64(aBytes)) },
			} };
		}

		auto anchor_json(const cycle_wire_anchor& aAnchor) -> json
		{
			return json{ json::object{
				{ .Key = "kind", .Value = text(std::string{ stable_name(aAnchor.Kind) }) },
				{ .Key = "path", .Value = optional_text(aAnchor.Path) },
				{ .Key = "source_revision_id", .Value = text(aAnchor.Revision) },
				{ .Key = "blob_sha256", .Value = aAnchor.Blob ? text(hex(*aAnchor.Blob)) : json{ } },
				{ .Key = "start_byte", .Value = optional_integer(aAnchor.Start) },
				{ .Key = "end_byte", .Value = optional_integer(aAnchor.End) },
			} };
		}

		auto suggestion_json(const cycle_wire_suggestion& aSuggestion) -> json
		{
			return json{ json::object{
				{ .Key = "id", .Value = text(aSuggestion.Id) },
				{ .Key = "revision", .Value = text(std::to_string(aSuggestion.Revision)) },
				{ .Key = "intent", .Value = text(std::string{ stable_name(aSuggestion.Intent) }) },
				{ .Key = "state", .Value = text(std::string{ stable_name(aSuggestion.State) }) },
				{ .Key = "anchor", .Value = anchor_json(aSuggestion.Anchor) },
				{ .Key = "original", .Value = byte_record(aSuggestion.Original) },
				{ .Key = "replacement", .Value = byte_record(aSuggestion.Replacement) },
				{ .Key = "already_applied_result_revision", .Value = optional_text(aSuggestion.AppliedIn) },
			} };
		}

		auto item_json(const cycle_wire_item& aItem) -> json
		{
			auto anchors = json::array{ };
			for (const auto& anchor : aItem.Anchors) {
				anchors.push_back(anchor_json(anchor));
			}
			auto suggestions = json::array{ };
			for (const auto& suggestion : aItem.Suggestions) {
				suggestions.push_back(suggestion_json(suggestion));
			}
			return json{ json::object{
				{ .Key = "id", .Value = text(aItem.Id) },
				{ .Key = "revision", .Value = text(std::to_string(aItem.Revision)) },
				{ .Key = "body", .Value = text(aItem.Body) },
				{ .Key = "action_override", .Value = aItem.ActionOverride ? text(std::string{ stable_name(*aItem.ActionOverride) }) : json{ } },
				{ .Key = "anchors", .Value = json{ std::move(anchors) } },
				{ .Key = "related_item_ids", .Value = texts(aItem.Related) },
				{ .Key = "suggestions", .Value = json{ std::move(suggestions) } },
			} };
		}

		auto source_json(const cycle_wire_source& aSource) -> json
		{
			return json{ json::object{
				{ .Key = "id", .Value = text(aSource.Id) },
				{ .Key = "path", .Value = text(aSource.Path) },
				{ .Key = "source_revision_id", .Value = text(aSource.Revision) },
				{ .Key = "kind", .Value = text(std::string{ stable_name(aSource.Kind) }) },
				{ .Key = "start_byte", .Value = integer(aSource.Start) },
				{ .Key = "end_byte", .Value = integer(aSource.End) },
				{ .Key = "blob_sha256", .Value = text(hex(aSource.Blob)) },
				{ .Key = "bytes", .Value = byte_record(aSource.Bytes) },
			} };
		}

		auto payload_json(const cycle_round_payload& aPayload) -> json
		{
			auto items = json::array{ };
			items.reserve(aPayload.Items.size());
			for (const auto& item : aPayload.Items) {
				items.push_back(item_json(item));
			}
			auto sources = json::array{ };
			sources.reserve(aPayload.Sources.size());
			for (const auto& source : aPayload.Sources) {
				sources.push_back(source_json(source));
			}
			return json{ json::object{
				{ .Key = "schema", .Value = text(std::string{ round_schema }) },
				{ .Key = "cycle_id", .Value = text(aPayload.CycleId) },
				{ .Key = "round_id", .Value = text(aPayload.RoundId) },
				{ .Key = "source_revision_id", .Value = text(aPayload.SourceRevision) },
				{ .Key = "source_consistency", .Value = text(std::string{ stable_name(aPayload.Consistency) }) },
				{ .Key = "project", .Value = json{ json::object{
										{ .Key = "id", .Value = text(aPayload.Project.Id) },
										{ .Key = "label", .Value = text(aPayload.Project.Label) },
										{ .Key = "scope_paths", .Value = texts(aPayload.Project.ScopePaths) },
									} } },
				{ .Key = "assignment", .Value = json{ json::object{
										   { .Key = "action", .Value = text(std::string{ stable_name(aPayload.Assignment.Action) }) },
										   { .Key = "goal", .Value = text(aPayload.Assignment.Goal) },
										   { .Key = "constraints", .Value = texts(aPayload.Assignment.Constraints) },
										   { .Key = "write_scope", .Value = texts(aPayload.Assignment.WriteScope) },
									   } } },
				{ .Key = "items", .Value = json{ std::move(items) } },
				{ .Key = "sources", .Value = json{ std::move(sources) } },
				{ .Key = "selection", .Value = json{ json::object{
										  { .Key = "item_count", .Value = integer(aPayload.Selection.ItemCount) },
										  { .Key = "source_count", .Value = integer(aPayload.Selection.SourceCount) },
										  { .Key = "intentional_omissions", .Value = texts(aPayload.Selection.Omissions) },
										  { .Key = "instruction_set_complete", .Value = json{ aPayload.Selection.Complete } },
									  } } },
			} };
		}

		// ---- Draft encoding ----------------------------------------------

		auto selection_json(const cycle_round_selection& aSelection) -> json
		{
			auto items = json::array{ };
			for (const auto& item : aSelection.Items) {
				auto pins = json::array{ };
				for (const auto& pin : item.Suggestions) {
					pins.push_back(json{ json::object{ { .Key = "id", .Value = integer(pin.Id.value()) }, { .Key = "revision", .Value = integer(pin.Revision) } } });
				}
				auto related = json::array{ };
				for (const auto& other : item.Related) {
					related.push_back(integer(other.value()));
				}
				items.push_back(json{ json::object{
					{ .Key = "item", .Value = integer(item.Item.value()) },
					{ .Key = "wording", .Value = integer(item.Wording.value()) },
					{ .Key = "action", .Value = item.Action ? text(std::string{ stable_name(*item.Action) }) : json{ } },
					{ .Key = "suggestions", .Value = json{ std::move(pins) } },
					{ .Key = "related", .Value = json{ std::move(related) } },
				} });
			}
			auto context = json::array{ };
			for (const auto& selected : aSelection.Context) {
				context.push_back(json{ json::object{
					{ .Key = "revision", .Value = integer(selected.Revision.value()) },
					{ .Key = "path", .Value = text(selected.Path) },
					{ .Key = "kind", .Value = text(std::string{ stable_name(selected.Kind) }) },
					{ .Key = "start", .Value = integer(selected.Start) },
					{ .Key = "end", .Value = integer(selected.End) },
				} });
			}
			return json{ json::object{
				{ .Key = "schema", .Value = text(std::string{ draft_schema }) },
				{ .Key = "source", .Value = integer(aSelection.Source.value()) },
				{ .Key = "action", .Value = text(std::string{ stable_name(aSelection.Action) }) },
				{ .Key = "goal", .Value = text(aSelection.Goal) },
				{ .Key = "constraints", .Value = texts(aSelection.Constraints) },
				{ .Key = "write_scope", .Value = texts(aSelection.WriteScope) },
				{ .Key = "items", .Value = json{ std::move(items) } },
				{ .Key = "context", .Value = json{ std::move(context) } },
				{ .Key = "omissions", .Value = texts(aSelection.Omissions) },
				{ .Key = "large_context", .Value = json{ aSelection.LargeContext } },
			} };
		}

		// ---- Enum names ---------------------------------------------------

		template <typename TEnum, std::size_t N>
		auto enum_from(const std::string_view aName, const std::array<TEnum, N>& aValues) -> std::optional<TEnum>
		{
			const auto found = std::ranges::find_if(aValues, [&](const TEnum aValue) { return stable_name(aValue) == aName; });
			return found == aValues.end() ? std::nullopt : std::optional{ *found };
		}

		constexpr auto actions       = std::array{ cycle_round_action::discuss, cycle_round_action::plan, cycle_round_action::implement };
		constexpr auto kinds         = std::array{ cycle_context_kind::full_file, cycle_context_kind::excerpt };
		constexpr auto anchor_kinds  = std::array{ cycle_anchor_kind::range, cycle_anchor_kind::file, cycle_anchor_kind::module, cycle_anchor_kind::cycle };
		constexpr auto consistencies = std::array{ cycle_source_consistency::observed_stable, cycle_source_consistency::immutable_source, cycle_source_consistency::incomplete };
		constexpr auto intents       = std::array{ cycle_suggestion_intent::exact_replacement, cycle_suggestion_intent::illustrative };
		constexpr auto states        = std::array{ cycle_suggestion_state::proposed, cycle_suggestion_state::observed_already_applied };

		// ---- Strict reading -----------------------------------------------

		auto wire_identifier(const std::string_view aText) noexcept -> bool
		{
			if (aText.empty() || aText.size() > 128U) {
				return false;
			}
			const auto letter = [](const char aSymbol) { return (aSymbol >= 'A' && aSymbol <= 'Z') || (aSymbol >= 'a' && aSymbol <= 'z'); };
			if (!letter(aText.front())) {
				return false;
			}
			return std::ranges::all_of(aText, [&](const char aSymbol) {
				return letter(aSymbol) || (aSymbol >= '0' && aSymbol <= '9') || aSymbol == '.' || aSymbol == '_' || aSymbol == ':' || aSymbol == '-';
			});
		}

		auto wire_revision(const std::string_view aText) noexcept -> std::optional<std::uint64_t>
		{
			if (aText.empty() || aText.size() > 19U || aText.front() == '0') {
				return std::nullopt;
			}
			auto        value = std::uint64_t{ 0 };
			const auto* first = aText.data();
			const auto* last  = aText.data() + aText.size();
			const auto  read  = std::from_chars(first, last, value);
			if (read.ec != std::errc{ } || read.ptr != last || value > maximum_cycle_identity) {
				return std::nullopt;
			}
			return value;
		}

		/// A relative, `/`-separated path with no traversal, drive, UNC,
		/// backslash, control character or repository internals.
		auto portable_relative(const std::string_view aPath) -> bool
		{
			return valid_cycle_path(aPath) && admit_workspace_path(aPath) == workspace_path_refusal::none
				&& std::ranges::none_of(aPath, [](const char aSymbol) { return static_cast<unsigned char>(aSymbol) < 0x20U || aSymbol == 0x7F; });
		}

		class payload_reader
		{
		public:
			std::optional<cycle_round_decode_error> Error;

			template <typename T = bool>
			auto fail(const cycle_round_decode_problem aProblem) -> std::optional<T>
			{
				if (!Error) {
					Error = cycle_round_decode_error{ .Problem = aProblem };
				}
				return std::nullopt;
			}

			auto object_field(const json* aValue, const std::span<const std::string_view> aKeys) -> const json::object*
			{
				const auto* members = aValue ? std::get_if<json::object>(&aValue->Value) : nullptr;
				if (!members || !has_exactly(*members, aKeys)) {
					fail(cycle_round_decode_problem::shape);
					return nullptr;
				}
				return members;
			}

			auto array_field(const json* aValue, const std::size_t aMaximum) -> const json::array*
			{
				const auto* items = aValue ? std::get_if<json::array>(&aValue->Value) : nullptr;
				if (!items) {
					fail(cycle_round_decode_problem::shape);
					return nullptr;
				}
				if (items->size() > aMaximum) {
					fail(cycle_round_decode_problem::bounds);
					return nullptr;
				}
				return items;
			}

			auto string_field(const json* aValue) -> const std::string*
			{
				const auto* value = aValue ? std::get_if<std::string>(&aValue->Value) : nullptr;
				if (!value) {
					fail(cycle_round_decode_problem::shape);
				}
				return value;
			}

			auto optional_string_field(const json* aValue) -> std::optional<std::optional<std::string>>
			{
				if (aValue && std::holds_alternative<std::monostate>(aValue->Value)) {
					return std::optional<std::string>{ };
				}
				const auto* value = string_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				return std::optional<std::string>{ *value };
			}

			auto unsigned_integer_field(const json* aValue) -> std::optional<std::uint64_t>
			{
				const auto* value = aValue ? std::get_if<std::int64_t>(&aValue->Value) : nullptr;
				if (!value || *value < 0) {
					return fail<std::uint64_t>(cycle_round_decode_problem::shape);
				}
				return static_cast<std::uint64_t>(*value);
			}

			auto optional_integer_field(const json* aValue) -> std::optional<std::optional<std::uint64_t>>
			{
				if (aValue && std::holds_alternative<std::monostate>(aValue->Value)) {
					return std::optional<std::uint64_t>{ };
				}
				auto value = unsigned_integer_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				return std::optional<std::optional<std::uint64_t>>{ std::in_place, value };
			}

			auto identifier_field(const json* aValue) -> std::optional<std::string>
			{
				const auto* value = string_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				if (!wire_identifier(*value)) {
					return fail<std::string>(cycle_round_decode_problem::identifier);
				}
				return *value;
			}

			auto revision_field(const json* aValue) -> std::optional<std::uint64_t>
			{
				const auto* value = string_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				const auto parsed = wire_revision(*value);
				if (!parsed) {
					return fail<std::uint64_t>(cycle_round_decode_problem::identifier);
				}
				return parsed;
			}

			auto digest_field(const json* aValue) -> std::optional<cycle_material_digest>
			{
				const auto* value = string_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				const auto parsed = parse_cycle_sha256(*value);
				if (!parsed) {
					return fail<cycle_material_digest>(cycle_round_decode_problem::identifier);
				}
				return cycle_material_digest{ *parsed };
			}

			template <typename TEnum, std::size_t N>
			auto enumerated_field(const json* aValue, const std::array<TEnum, N>& aValues) -> std::optional<TEnum>
			{
				const auto* value = string_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				const auto parsed = enum_from(*value, aValues);
				if (!parsed) {
					return fail<TEnum>(cycle_round_decode_problem::shape);
				}
				return parsed;
			}

			auto path_field(const json* aValue) -> std::optional<std::string>
			{
				const auto* value = string_field(aValue);
				if (!value) {
					return std::nullopt;
				}
				if (!portable_relative(*value)) {
					return fail<std::string>(cycle_round_decode_problem::path);
				}
				return *value;
			}

			auto texts_field(const json* aValue, const std::size_t aMaximum, const std::size_t aBytes) -> std::optional<std::vector<std::string>>
			{
				const auto* items = array_field(aValue, aMaximum);
				if (!items) {
					return std::nullopt;
				}
				auto result = std::vector<std::string>{ };
				for (const auto& item : *items) {
					const auto* value = string_field(&item);
					if (!value) {
						return std::nullopt;
					}
					if (value->size() > aBytes) {
						return fail<std::vector<std::string>>(cycle_round_decode_problem::bounds);
					}
					result.push_back(*value);
				}
				return result;
			}

			auto bytes_field(const json* aValue, const std::uint64_t aLimit) -> std::optional<std::string>
			{
				static constexpr auto keys    = std::array<std::string_view, 3>{ "length", "sha256", "base64" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return std::nullopt;
				}
				const auto  length  = unsigned_integer_field(find_member(*members, "length"));
				const auto  sha     = digest_field(find_member(*members, "sha256"));
				const auto* encoded = string_field(find_member(*members, "base64"));
				if (!length || !sha || !encoded) {
					return std::nullopt;
				}
				if (*length > aLimit || encoded->size() > (aLimit + 2U) / 3U * 4U) {
					return fail<std::string>(cycle_round_decode_problem::bounds);
				}
				auto raw = decode_cycle_base64(*encoded);
				if (!raw || raw->size() != *length || digest_of(*raw) != *sha) {
					return fail<std::string>(cycle_round_decode_problem::bytes);
				}
				return raw;
			}

			auto anchor_field(const json* aValue) -> std::optional<cycle_wire_anchor>
			{
				static constexpr auto keys    = std::array<std::string_view, 6>{ "kind", "path", "source_revision_id", "blob_sha256", "start_byte", "end_byte" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return std::nullopt;
				}
				const auto kind     = enumerated_field(find_member(*members, "kind"), anchor_kinds);
				const auto path     = optional_string_field(find_member(*members, "path"));
				const auto revision = identifier_field(find_member(*members, "source_revision_id"));
				const auto blob     = optional_string_field(find_member(*members, "blob_sha256"));
				const auto start    = optional_integer_field(find_member(*members, "start_byte"));
				const auto end      = optional_integer_field(find_member(*members, "end_byte"));
				if (!kind || !path || !revision || !blob || !start || !end) {
					return std::nullopt;
				}
				auto result = cycle_wire_anchor{ .Kind = *kind, .Path = *path, .Revision = *revision, .Start = *start, .End = *end };
				if (*blob) {
					const auto parsed = parse_cycle_sha256(**blob);
					if (!parsed) {
						return fail<cycle_wire_anchor>(cycle_round_decode_problem::identifier);
					}
					result.Blob = cycle_material_digest{ *parsed };
				}
				if (result.Path && !portable_relative(*result.Path)) {
					return fail<cycle_wire_anchor>(cycle_round_decode_problem::path);
				}
				const auto located = result.Path.has_value();
				const auto byBytes = result.Blob.has_value();
				const auto ranged  = result.Start.has_value() && result.End.has_value();
				const auto shaped  = result.Start.has_value() == result.End.has_value()
					&& ((*kind == cycle_anchor_kind::range && located && byBytes && ranged) || (*kind == cycle_anchor_kind::file && located && byBytes && !ranged)
						|| (*kind == cycle_anchor_kind::module && located && !byBytes && !ranged) || (*kind == cycle_anchor_kind::cycle && !located && !byBytes && !ranged));
				if (!shaped) {
					return fail<cycle_wire_anchor>(cycle_round_decode_problem::shape);
				}
				if (ranged && *result.Start > *result.End) {
					return fail<cycle_wire_anchor>(cycle_round_decode_problem::range);
				}
				return result;
			}

			auto suggestion_field(const json* aValue) -> std::optional<cycle_wire_suggestion>
			{
				static constexpr auto keys    = std::array<std::string_view, 8>{ "id", "revision", "intent", "state", "anchor", "original", "replacement", "already_applied_result_revision" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return std::nullopt;
				}
				auto id          = identifier_field(find_member(*members, "id"));
				auto revision    = revision_field(find_member(*members, "revision"));
				auto intent      = enumerated_field(find_member(*members, "intent"), intents);
				auto state       = enumerated_field(find_member(*members, "state"), states);
				auto anchor      = anchor_field(find_member(*members, "anchor"));
				auto original    = bytes_field(find_member(*members, "original"), maximum_round_suggestion_bytes);
				auto replacement = bytes_field(find_member(*members, "replacement"), maximum_round_suggestion_bytes);
				auto applied     = optional_string_field(find_member(*members, "already_applied_result_revision"));
				if (!id || !revision || !intent || !state || !anchor || !original || !replacement || !applied) {
					return std::nullopt;
				}
				if (*applied && !wire_identifier(**applied)) {
					return fail<cycle_wire_suggestion>(cycle_round_decode_problem::identifier);
				}
				if (anchor->Kind != cycle_anchor_kind::range || *anchor->End - *anchor->Start != original->size()) {
					return fail<cycle_wire_suggestion>(cycle_round_decode_problem::range);
				}
				if ((*state == cycle_suggestion_state::observed_already_applied) != applied->has_value()) {
					return fail<cycle_wire_suggestion>(cycle_round_decode_problem::shape);
				}
				return cycle_wire_suggestion{ .Id = std::move(*id), .Revision = *revision, .Intent = *intent, .State = *state, .Anchor = std::move(*anchor), .Original = std::move(*original), .Replacement = std::move(*replacement), .AppliedIn = std::move(*applied) };
			}

			auto item_field(const json* aValue) -> std::optional<cycle_wire_item>
			{
				static constexpr auto keys    = std::array<std::string_view, 7>{ "id", "revision", "body", "action_override", "anchors", "related_item_ids", "suggestions" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return std::nullopt;
				}
				auto        id          = identifier_field(find_member(*members, "id"));
				auto        revision    = revision_field(find_member(*members, "revision"));
				const auto* body        = string_field(find_member(*members, "body"));
				const auto  overridden  = optional_string_field(find_member(*members, "action_override"));
				const auto* anchors     = array_field(find_member(*members, "anchors"), maximum_round_anchors);
				const auto* related     = array_field(find_member(*members, "related_item_ids"), maximum_round_related_items);
				const auto* suggestions = array_field(find_member(*members, "suggestions"), maximum_round_suggestions);
				if (!id || !revision || !body || !overridden || !anchors || !related || !suggestions) {
					return std::nullopt;
				}
				if (body->size() > maximum_round_body_bytes) {
					return fail<cycle_wire_item>(cycle_round_decode_problem::bounds);
				}
				auto result = cycle_wire_item{ .Id = std::move(*id), .Revision = *revision, .Body = *body };
				if (*overridden) {
					result.ActionOverride = enum_from(**overridden, actions);
					if (!result.ActionOverride) {
						return fail<cycle_wire_item>(cycle_round_decode_problem::shape);
					}
				}
				for (const auto& value : *anchors) {
					auto decoded = anchor_field(&value);
					if (!decoded) {
						return std::nullopt;
					}
					result.Anchors.push_back(std::move(*decoded));
				}
				for (const auto& value : *related) {
					auto decoded = identifier_field(&value);
					if (!decoded) {
						return std::nullopt;
					}
					result.Related.push_back(std::move(*decoded));
				}
				for (const auto& value : *suggestions) {
					auto decoded = suggestion_field(&value);
					if (!decoded) {
						return std::nullopt;
					}
					result.Suggestions.push_back(std::move(*decoded));
				}
				return result;
			}

			auto source_field(const json* aValue) -> std::optional<cycle_wire_source>
			{
				static constexpr auto keys    = std::array<std::string_view, 8>{ "id", "path", "source_revision_id", "kind", "start_byte", "end_byte", "blob_sha256", "bytes" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return std::nullopt;
				}
				auto id       = identifier_field(find_member(*members, "id"));
				auto path     = path_field(find_member(*members, "path"));
				auto revision = identifier_field(find_member(*members, "source_revision_id"));
				auto kind     = enumerated_field(find_member(*members, "kind"), kinds);
				auto start    = unsigned_integer_field(find_member(*members, "start_byte"));
				auto end      = unsigned_integer_field(find_member(*members, "end_byte"));
				auto blob     = digest_field(find_member(*members, "blob_sha256"));
				auto raw      = bytes_field(find_member(*members, "bytes"), maximum_round_context_bytes);
				if (!id || !path || !revision || !kind || !start || !end || !blob || !raw) {
					return std::nullopt;
				}
				if (*start > *end || *end - *start != raw->size() || (*kind == cycle_context_kind::full_file && (*start != 0U || digest_of(*raw) != *blob))) {
					return fail<cycle_wire_source>(cycle_round_decode_problem::range);
				}
				return cycle_wire_source{ .Id = std::move(*id), .Path = std::move(*path), .Revision = std::move(*revision), .Kind = *kind, .Start = *start, .End = *end, .Blob = *blob, .Bytes = std::move(*raw) };
			}

			auto payload_field(const json* aValue) -> std::optional<cycle_round_payload>
			{
				static constexpr auto keys    = std::array<std::string_view, 10>{ "schema", "cycle_id", "round_id", "source_revision_id", "source_consistency", "project", "assignment", "items", "sources", "selection" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return std::nullopt;
				}
				const auto* schema = string_field(find_member(*members, "schema"));
				if (!schema) {
					return std::nullopt;
				}
				if (*schema != round_schema) {
					return fail<cycle_round_payload>(cycle_round_decode_problem::version);
				}
				auto result      = cycle_round_payload{ };
				auto cycle       = identifier_field(find_member(*members, "cycle_id"));
				auto round       = identifier_field(find_member(*members, "round_id"));
				auto revision    = identifier_field(find_member(*members, "source_revision_id"));
				auto consistency = enumerated_field(find_member(*members, "source_consistency"), consistencies);
				if (!cycle || !round || !revision || !consistency || !project_field(find_member(*members, "project"), result.Project)
					|| !assignment_field(find_member(*members, "assignment"), result.Assignment) || !selection_field(find_member(*members, "selection"), result.Selection)) {
					return std::nullopt;
				}
				result.CycleId        = std::move(*cycle);
				result.RoundId        = std::move(*round);
				result.SourceRevision = std::move(*revision);
				result.Consistency    = *consistency;
				const auto* items     = array_field(find_member(*members, "items"), maximum_round_items);
				const auto* sources   = array_field(find_member(*members, "sources"), maximum_round_sources);
				if (!items || !sources) {
					return std::nullopt;
				}
				for (const auto& value : *items) {
					auto decoded = item_field(&value);
					if (!decoded) {
						return std::nullopt;
					}
					result.Items.push_back(std::move(*decoded));
				}
				for (const auto& value : *sources) {
					auto decoded = source_field(&value);
					if (!decoded) {
						return std::nullopt;
					}
					result.Sources.push_back(std::move(*decoded));
				}
				return result;
			}

		private:
			auto project_field(const json* aValue, cycle_round_project& aOut) -> bool
			{
				static constexpr auto keys    = std::array<std::string_view, 3>{ "id", "label", "scope_paths" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return false;
				}
				auto        id    = identifier_field(find_member(*members, "id"));
				const auto* label = string_field(find_member(*members, "label"));
				auto        paths = texts_field(find_member(*members, "scope_paths"), maximum_cycle_scope_paths, maximum_cycle_scope_path_bytes);
				if (!id || !label || !paths) {
					return false;
				}
				if (label->size() > maximum_round_text_bytes) {
					return fail(cycle_round_decode_problem::bounds).has_value();
				}
				if (!std::ranges::all_of(*paths, [](const std::string& aPath) { return portable_relative(aPath); })) {
					return fail(cycle_round_decode_problem::path).has_value();
				}
				aOut = { .Id = std::move(*id), .Label = *label, .ScopePaths = std::move(*paths) };
				return true;
			}

			auto assignment_field(const json* aValue, cycle_round_assignment& aOut) -> bool
			{
				static constexpr auto keys    = std::array<std::string_view, 4>{ "action", "goal", "constraints", "write_scope" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return false;
				}
				const auto  action      = enumerated_field(find_member(*members, "action"), actions);
				const auto* goal        = string_field(find_member(*members, "goal"));
				auto        constraints = texts_field(find_member(*members, "constraints"), maximum_round_constraints, maximum_round_text_bytes);
				auto        scope       = texts_field(find_member(*members, "write_scope"), maximum_round_write_scope, maximum_cycle_scope_path_bytes);
				if (!action || !goal || !constraints || !scope) {
					return false;
				}
				if (goal->size() > maximum_round_text_bytes) {
					return fail(cycle_round_decode_problem::bounds).has_value();
				}
				if (!std::ranges::all_of(*scope, [](const std::string& aPath) { return portable_relative(aPath); })) {
					return fail(cycle_round_decode_problem::path).has_value();
				}
				aOut = { .Action = *action, .Goal = *goal, .Constraints = std::move(*constraints), .WriteScope = std::move(*scope) };
				return true;
			}

			auto selection_field(const json* aValue, cycle_round_completeness& aOut) -> bool
			{
				static constexpr auto keys    = std::array<std::string_view, 4>{ "item_count", "source_count", "intentional_omissions", "instruction_set_complete" };
				const auto*           members = object_field(aValue, keys);
				if (!members) {
					return false;
				}
				const auto  items     = unsigned_integer_field(find_member(*members, "item_count"));
				const auto  sources   = unsigned_integer_field(find_member(*members, "source_count"));
				auto        omissions = texts_field(find_member(*members, "intentional_omissions"), maximum_round_omissions, maximum_round_text_bytes);
				const auto* complete  = find_member(*members, "instruction_set_complete");
				const auto* flag      = complete ? std::get_if<bool>(&complete->Value) : nullptr;
				if (!items || !sources || !omissions) {
					return false;
				}
				if (!flag) {
					return fail(cycle_round_decode_problem::shape).has_value();
				}
				aOut = { .ItemCount = *items, .SourceCount = *sources, .Omissions = std::move(*omissions), .Complete = *flag };
				return true;
			}
		};

		/// Cross-record rules the field readers cannot see.
		auto check_payload(const cycle_round_payload& aPayload) -> std::optional<cycle_round_decode_problem>
		{
			if (aPayload.Selection.ItemCount != aPayload.Items.size() || aPayload.Selection.SourceCount != aPayload.Sources.size() || !aPayload.Selection.Complete) {
				return cycle_round_decode_problem::completeness;
			}
			auto itemIds       = std::set<std::string_view>{ };
			auto suggestionIds = std::set<std::string_view>{ };
			auto sourceIds     = std::set<std::string_view>{ };
			auto located       = std::set<std::pair<std::string_view, std::string_view>>{ };
			auto instruction   = std::uint64_t{ aPayload.Assignment.Goal.size() };
			auto context       = std::uint64_t{ 0 };
			for (const auto& item : aPayload.Items) {
				if (!itemIds.insert(item.Id).second) {
					return cycle_round_decode_problem::duplicate;
				}
				instruction += item.Body.size();
			}
			auto files = std::map<std::tuple<std::string_view, std::string_view, cycle_material_digest>, const std::string*>{ };
			for (const auto& source : aPayload.Sources) {
				if (!sourceIds.insert(source.Id).second || !located.insert({ source.Revision, source.Path }).second) {
					return cycle_round_decode_problem::duplicate;
				}
				context += source.Bytes.size();
				if (source.Kind == cycle_context_kind::full_file) {
					files.emplace(std::tuple{ std::string_view{ source.Revision }, std::string_view{ source.Path }, source.Blob }, &source.Bytes);
				}
			}
			for (const auto& item : aPayload.Items) {
				auto seen = std::set<std::string_view>{ };
				for (const auto& related : item.Related) {
					if (related == item.Id || !itemIds.contains(related) || !seen.insert(related).second) {
						return cycle_round_decode_problem::relation;
					}
				}
				for (const auto& suggestion : item.Suggestions) {
					if (!suggestionIds.insert(suggestion.Id).second) {
						return cycle_round_decode_problem::duplicate;
					}
					instruction += suggestion.Original.size() + suggestion.Replacement.size();
					const auto& anchor = suggestion.Anchor;
					const auto  file   = files.find({ std::string_view{ anchor.Revision }, std::string_view{ *anchor.Path }, *anchor.Blob });
					if (file != files.end() && (*anchor.End > file->second->size() || std::string_view{ *file->second }.substr(*anchor.Start, *anchor.End - *anchor.Start) != suggestion.Original)) {
						return cycle_round_decode_problem::range;
					}
				}
			}
			if (instruction > maximum_round_instruction_bytes || context > maximum_round_context_bytes) {
				return cycle_round_decode_problem::bounds;
			}
			return std::nullopt;
		}

		auto finish_decode(const json* aPayload, const std::optional<cycle_round_digest>& aDeclared) -> std::expected<cycle_decoded_round, cycle_round_decode_error>
		{
			auto reader  = payload_reader{ };
			auto payload = reader.payload_field(aPayload);
			if (!payload) {
				return std::unexpected(reader.Error.value_or(cycle_round_decode_error{ }));
			}
			auto canonical = canonical_cycle_json(*aPayload);
			if (!canonical) {
				return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::shape });
			}
			const auto digest = round_digest(*canonical);
			if (aDeclared && *aDeclared != digest) {
				return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::digest });
			}
			if (const auto problem = check_payload(*payload)) {
				return std::unexpected(cycle_round_decode_error{ .Problem = *problem });
			}
			// The typed reading must say exactly what the document says.
			if (encode_round_payload(*payload) != canonical) {
				return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::shape });
			}
			return cycle_decoded_round{ .Payload = std::move(*payload), .Digest = digest, .Canonical = std::move(*canonical) };
		}

		// ---- Draft reading ------------------------------------------------

		auto draft_id(const json* aValue) -> std::optional<std::uint64_t>
		{
			const auto* value = aValue ? std::get_if<std::int64_t>(&aValue->Value) : nullptr;
			if (!value || *value < 0) {
				return std::nullopt;
			}
			return static_cast<std::uint64_t>(*value);
		}

		auto draft_texts(const json* aValue) -> std::optional<std::vector<std::string>>
		{
			const auto* items = aValue ? std::get_if<json::array>(&aValue->Value) : nullptr;
			if (!items) {
				return std::nullopt;
			}
			auto result = std::vector<std::string>{ };
			for (const auto& item : *items) {
				const auto* value = std::get_if<std::string>(&item.Value);
				if (!value) {
					return std::nullopt;
				}
				result.push_back(*value);
			}
			return result;
		}

		auto draft_item(const json& aValue) -> std::optional<cycle_round_item_selection>
		{
			static constexpr auto keys    = std::array<std::string_view, 5>{ "item", "wording", "action", "suggestions", "related" };
			const auto*           members = std::get_if<json::object>(&aValue.Value);
			if (!members || !has_exactly(*members, keys)) {
				return std::nullopt;
			}
			const auto  item        = draft_id(find_member(*members, "item"));
			const auto  wording     = draft_id(find_member(*members, "wording"));
			const auto* action      = find_member(*members, "action");
			const auto* suggestions = std::get_if<json::array>(&find_member(*members, "suggestions")->Value);
			const auto* related     = std::get_if<json::array>(&find_member(*members, "related")->Value);
			if (!item || !wording || !suggestions || !related) {
				return std::nullopt;
			}
			auto result = cycle_round_item_selection{ .Item = cycle_feedback_item_id{ *item }, .Wording = private_review_turn_revision_id{ *wording } };
			if (const auto* name = std::get_if<std::string>(&action->Value)) {
				result.Action = enum_from(*name, actions);
				if (!result.Action) {
					return std::nullopt;
				}
			} else if (!std::holds_alternative<std::monostate>(action->Value)) {
				return std::nullopt;
			}
			static constexpr auto pinKeys = std::array<std::string_view, 2>{ "id", "revision" };
			for (const auto& pin : *suggestions) {
				const auto* fields = std::get_if<json::object>(&pin.Value);
				if (!fields || !has_exactly(*fields, pinKeys)) {
					return std::nullopt;
				}
				const auto id       = draft_id(find_member(*fields, "id"));
				const auto revision = draft_id(find_member(*fields, "revision"));
				if (!id || !revision) {
					return std::nullopt;
				}
				result.Suggestions.push_back({ .Id = cycle_suggestion_id{ *id }, .Revision = *revision });
			}
			for (const auto& other : *related) {
				const auto id = draft_id(&other);
				if (!id) {
					return std::nullopt;
				}
				result.Related.emplace_back(*id);
			}
			return result;
		}

		auto draft_context(const json& aValue) -> std::optional<cycle_context_selection>
		{
			static constexpr auto keys    = std::array<std::string_view, 5>{ "revision", "path", "kind", "start", "end" };
			const auto*           members = std::get_if<json::object>(&aValue.Value);
			if (!members || !has_exactly(*members, keys)) {
				return std::nullopt;
			}
			const auto  revision = draft_id(find_member(*members, "revision"));
			const auto* path     = std::get_if<std::string>(&find_member(*members, "path")->Value);
			const auto* kind     = std::get_if<std::string>(&find_member(*members, "kind")->Value);
			const auto  start    = draft_id(find_member(*members, "start"));
			const auto  end      = draft_id(find_member(*members, "end"));
			const auto  parsed   = kind ? enum_from(*kind, kinds) : std::nullopt;
			if (!revision || !path || !parsed || !start || !end) {
				return std::nullopt;
			}
			return cycle_context_selection{ .Revision = cycle_source_revision_id{ *revision }, .Path = *path, .Kind = *parsed, .Start = *start, .End = *end };
		}

		// ---- Assembly -----------------------------------------------------

		auto refusal(const cycle_round_problem aProblem, const std::optional<std::size_t> aItem = std::nullopt, const std::optional<std::size_t> aContext = std::nullopt)
			-> std::unexpected<cycle_round_refusal>
		{
			return std::unexpected(cycle_round_refusal{ .Problem = aProblem, .Item = aItem, .Context = aContext });
		}

		auto utf8_within(const std::string& aText, const std::size_t aBytes) -> bool
		{
			return aText.size() <= aBytes && valid_cycle_utf8(aText);
		}

		auto allocated(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> bool
		{
			return valid_cycle_id(aRevision) && aRevision.value() < aCycle.Next.SourceRevision;
		}

		auto wire_anchor(const cycle_feedback_target& aTarget, const cycle_source_revision_id aSource) -> cycle_wire_anchor
		{
			if (const auto* anchor = std::get_if<cycle_source_anchor>(&aTarget)) {
				return { .Kind = cycle_anchor_kind::range, .Path = anchor->Path, .Revision = cycle_wire_name(anchor->Revision), .Blob = anchor->Blob, .Start = anchor->Start, .End = anchor->End };
			}
			if (const auto* file = std::get_if<cycle_file_target>(&aTarget)) {
				return { .Kind = cycle_anchor_kind::file, .Path = file->Path, .Revision = cycle_wire_name(file->Revision), .Blob = file->Blob };
			}
			if (const auto* module = std::get_if<cycle_module_target>(&aTarget)) {
				return { .Kind = cycle_anchor_kind::module, .Path = module->Path, .Revision = cycle_wire_name(module->Revision.value_or(aSource)) };
			}
			return { .Kind = cycle_anchor_kind::cycle, .Revision = cycle_wire_name(aSource) };
		}

		auto wire_suggestion(const cycle_suggestion& aSuggestion) -> cycle_wire_suggestion
		{
			return {
				.Id          = cycle_wire_name(aSuggestion.Id),
				.Revision    = aSuggestion.Revision,
				.Intent      = aSuggestion.Intent,
				.State       = aSuggestion.State,
				.Anchor      = { .Kind = cycle_anchor_kind::range, .Path = aSuggestion.Path, .Revision = cycle_wire_name(aSuggestion.Source), .Blob = aSuggestion.OriginalBlob, .Start = aSuggestion.Start, .End = aSuggestion.End },
				.Original    = aSuggestion.Original,
				.Replacement = aSuggestion.Replacement,
				.AppliedIn   = aSuggestion.AppliedIn ? std::optional{ cycle_wire_name(*aSuggestion.AppliedIn) } : std::nullopt,
			};
		}

		auto check_assignment_text(const cycle_round_selection& aSelection) -> bool
		{
			const auto text = [](const std::string& aValue) { return utf8_within(aValue, maximum_round_text_bytes); };
			return text(aSelection.Goal) && aSelection.Constraints.size() <= maximum_round_constraints && std::ranges::all_of(aSelection.Constraints, text)
				&& aSelection.Omissions.size() <= maximum_round_omissions && std::ranges::all_of(aSelection.Omissions, text);
		}

		auto check_write_scope(const std::vector<std::string>& aScope) -> bool
		{
			auto seen = std::set<std::string_view>{ };
			return aScope.size() <= maximum_round_write_scope && std::ranges::all_of(aScope, [&](const std::string& aPath) {
					   return portable_relative(aPath) && valid_cycle_utf8(aPath) && seen.insert(aPath).second;
				   });
		}

		struct assembler
		{
			const cycle_round_inputs&    Inputs;
			const review_cycle_state&    Cycle;
			const cycle_round_selection& Selection;
			cycle_round_measure          Measure;

			auto material(const cycle_feedback_item_id aItem) const -> const cycle_round_item_material*
			{
				const auto found = std::ranges::find_if(Inputs.Items, [&](const cycle_round_item_material& aMaterial) { return aMaterial.Thread.Id.Id == aItem; });
				return found == Inputs.Items.end() ? nullptr : &*found;
			}

			auto file(const cycle_source_revision_id aRevision, const std::string_view aPath) const -> const cycle_round_file*
			{
				const auto found = std::ranges::find_if(Inputs.Files, [&](const cycle_round_file& aFile) { return aFile.Revision == aRevision && aFile.Path == aPath; });
				return found == Inputs.Files.end() ? nullptr : &*found;
			}

			auto item(const std::size_t aIndex) -> std::expected<cycle_wire_item, cycle_round_refusal>
			{
				const auto& selected = Selection.Items[aIndex];
				const auto* found    = material(selected.Item);
				if (!found || found->Thread.Id.Cycle != Cycle.Subject.Cycle) {
					return refusal(cycle_round_problem::unknown_item, aIndex);
				}
				if (found->Thread.Lineage.Lifecycle == private_thread_lifecycle::archived) {
					return refusal(cycle_round_problem::archived_item, aIndex);
				}
				const auto disclosed = disclose_feedback(found->Thread, found->History, { });
				if (!disclosed || disclosed->Revisions.empty()) {
					return refusal(cycle_round_problem::unknown_item, aIndex);
				}
				const auto& wording = disclosed->Revisions.front();
				if (wording.Revision != selected.Wording) {
					return refusal(cycle_round_problem::stale_wording, aIndex);
				}
				if (!utf8_within(wording.Body, maximum_round_body_bytes)) {
					return refusal(cycle_round_problem::invalid_text, aIndex);
				}
				auto result = cycle_wire_item{ .Id = cycle_wire_name(selected.Item), .Revision = wording.Revision.value(), .Body = wording.Body, .ActionOverride = selected.Action };
				result.Anchors.push_back(wire_anchor(disclosed->Primary, Selection.Source));
				for (const auto& related : disclosed->Related) {
					result.Anchors.push_back(wire_anchor(related.Target, Selection.Source));
				}
				Measure.InstructionBytes += wording.Body.size();
				if (auto problem = relations(aIndex, result)) {
					return std::unexpected(*problem);
				}
				if (auto problem = suggestions(aIndex, *found, result)) {
					return std::unexpected(*problem);
				}
				if (result.Anchors.size() > maximum_round_anchors) {
					return refusal(cycle_round_problem::invalid_selection, aIndex);
				}
				return result;
			}

			auto relations(const std::size_t aIndex, cycle_wire_item& aItem) const -> std::optional<cycle_round_refusal>
			{
				const auto& selected = Selection.Items[aIndex];
				auto        seen     = std::set<cycle_feedback_item_id>{ };
				for (const auto related : selected.Related) {
					const auto member = std::ranges::any_of(Selection.Items, [&](const cycle_round_item_selection& aOther) { return aOther.Item == related; });
					if (related == selected.Item || !member || !seen.insert(related).second || seen.size() > maximum_round_related_items) {
						return refusal(cycle_round_problem::invalid_relation, aIndex).error();
					}
					aItem.Related.push_back(cycle_wire_name(related));
				}
				return std::nullopt;
			}

			auto suggestions(const std::size_t aIndex, const cycle_round_item_material& aMaterial, cycle_wire_item& aItem) -> std::optional<cycle_round_refusal>
			{
				const auto& selected = Selection.Items[aIndex];
				auto        seen     = std::set<cycle_suggestion_id>{ };
				if (selected.Suggestions.size() > maximum_round_suggestions) {
					return refusal(cycle_round_problem::invalid_selection, aIndex).error();
				}
				for (const auto& pin : selected.Suggestions) {
					const auto found = std::ranges::find_if(aMaterial.Suggestions, [&](const cycle_suggestion& aSuggestion) { return aSuggestion.Id.Id == pin.Id; });
					if (!seen.insert(pin.Id).second) {
						return refusal(cycle_round_problem::invalid_selection, aIndex).error();
					}
					if (found == aMaterial.Suggestions.end() || found->Id.Item != aMaterial.Thread.Id || !valid(*found)) {
						return refusal(cycle_round_problem::unknown_suggestion, aIndex).error();
					}
					if (found->Revision != pin.Revision) {
						return refusal(cycle_round_problem::stale_suggestion, aIndex).error();
					}
					Measure.InstructionBytes += found->Original.size() + found->Replacement.size();
					aItem.Suggestions.push_back(wire_suggestion(*found));
				}
				return std::nullopt;
			}

			/// One source per file and revision, in first-selection order.
			auto sources() -> std::expected<std::vector<cycle_wire_source>, cycle_round_refusal>
			{
				auto result = std::vector<cycle_wire_source>{ };
				auto first  = std::vector<std::size_t>{ };
				for (std::size_t index = 0; index < Selection.Context.size(); ++index) {
					const auto& selected = Selection.Context[index];
					if (!allocated(Cycle, selected.Revision) || !portable_relative(selected.Path) || !valid_cycle_utf8(selected.Path)) {
						return refusal(cycle_round_problem::invalid_selection, std::nullopt, index);
					}
					const auto* held = file(selected.Revision, selected.Path);
					if (!held || digest_of(held->Bytes) != held->Blob) {
						return refusal(cycle_round_problem::unavailable_context, std::nullopt, index);
					}
					const auto whole = selected.Kind == cycle_context_kind::full_file;
					if (!whole && (selected.Start >= selected.End || selected.End > held->Bytes.size())) {
						return refusal(cycle_round_problem::unavailable_context, std::nullopt, index);
					}
					const auto start = whole ? 0U : selected.Start;
					const auto end   = whole ? held->Bytes.size() : selected.End;
					const auto same  = std::ranges::find_if(result, [&](const cycle_wire_source& aSource) {
						return aSource.Revision == cycle_wire_name(selected.Revision) && aSource.Path == selected.Path;
					});
					if (same == result.end()) {
						result.push_back({ .Id = { }, .Path = selected.Path, .Revision = cycle_wire_name(selected.Revision), .Kind = selected.Kind, .Start = start, .End = end, .Blob = held->Blob });
						first.push_back(index);
						continue;
					}
					same->Start = std::min(same->Start, static_cast<std::uint64_t>(start));
					same->End   = std::max(same->End, static_cast<std::uint64_t>(end));
					if (whole) {
						same->Kind = cycle_context_kind::full_file;
					}
				}
				for (std::size_t index = 0; index < result.size(); ++index) {
					auto&       source = result[index];
					const auto* held   = file(Selection.Context[first[index]].Revision, source.Path);
					source.Id          = "S" + std::to_string(index + 1U);
					source.Bytes       = held->Bytes.substr(source.Start, source.End - source.Start);
					Measure.ContextBytes += source.Bytes.size();
				}
				if (result.size() > maximum_round_sources) {
					return refusal(cycle_round_problem::invalid_selection);
				}
				return result;
			}
		};
	}

	auto stable_name(const cycle_round_action aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_round_action::discuss:   return "discuss";
		case cycle_round_action::plan:      return "plan";
		case cycle_round_action::implement: return "implement";
		}
		return "discuss";
	}

	auto stable_name(const cycle_context_kind aValue) noexcept -> std::string_view
	{
		return aValue == cycle_context_kind::excerpt ? "excerpt" : "full_file";
	}

	auto stable_name(const cycle_anchor_kind aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_anchor_kind::range:  return "range";
		case cycle_anchor_kind::file:   return "file";
		case cycle_anchor_kind::module: return "module";
		case cycle_anchor_kind::cycle:  return "cycle";
		}
		return "cycle";
	}

	auto stable_name(const cycle_source_consistency aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_source_consistency::observed_stable:  return "observed_stable";
		case cycle_source_consistency::immutable_source: return "immutable_source";
		case cycle_source_consistency::incomplete:       return "incomplete";
		}
		return "incomplete";
	}

	auto stable_name(const cycle_round_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_round_problem::invalid_selection:   return "invalid_selection";
		case cycle_round_problem::invalid_text:        return "invalid_text";
		case cycle_round_problem::empty:               return "empty";
		case cycle_round_problem::unknown_item:        return "unknown_item";
		case cycle_round_problem::archived_item:       return "archived_item";
		case cycle_round_problem::stale_wording:       return "stale_wording";
		case cycle_round_problem::unknown_suggestion:  return "unknown_suggestion";
		case cycle_round_problem::stale_suggestion:    return "stale_suggestion";
		case cycle_round_problem::invalid_relation:    return "invalid_relation";
		case cycle_round_problem::invalid_source:      return "invalid_source";
		case cycle_round_problem::unavailable_context: return "unavailable_context";
		case cycle_round_problem::oversized:           return "oversized";
		case cycle_round_problem::unflushed:           return "unflushed";
		case cycle_round_problem::stale_draft:         return "stale_draft";
		}
		return "invalid_selection";
	}

	auto stable_name(const cycle_round_decode_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_round_decode_problem::json:         return "json";
		case cycle_round_decode_problem::version:      return "version";
		case cycle_round_decode_problem::shape:        return "shape";
		case cycle_round_decode_problem::identifier:   return "identifier";
		case cycle_round_decode_problem::bytes:        return "bytes";
		case cycle_round_decode_problem::digest:       return "digest";
		case cycle_round_decode_problem::completeness: return "completeness";
		case cycle_round_decode_problem::duplicate:    return "duplicate";
		case cycle_round_decode_problem::path:         return "path";
		case cycle_round_decode_problem::range:        return "range";
		case cycle_round_decode_problem::bounds:       return "bounds";
		case cycle_round_decode_problem::relation:     return "relation";
		case cycle_round_decode_problem::noncanonical: return "noncanonical";
		}
		return "shape";
	}

	auto encode_round_selection(const cycle_round_selection& aSelection) -> std::optional<std::string>
	{
		return canonical_cycle_json(selection_json(aSelection));
	}

	auto decode_round_selection(const std::string_view aBody) -> std::optional<cycle_round_selection>
	{
		if (aBody.empty()) {
			return cycle_round_selection{ };
		}
		const auto parsed = parse_cycle_json(aBody, maximum_round_document_bytes);
		if (!parsed) {
			return std::nullopt;
		}
		static constexpr auto keys    = std::array<std::string_view, 10>{ "schema", "source", "action", "goal", "constraints", "write_scope", "items", "context", "omissions", "large_context" };
		const auto*           members = std::get_if<json::object>(&parsed->Value);
		if (!members || !has_exactly(*members, keys)) {
			return std::nullopt;
		}
		const auto* schema       = std::get_if<std::string>(&find_member(*members, "schema")->Value);
		const auto  source       = draft_id(find_member(*members, "source"));
		const auto* action       = std::get_if<std::string>(&find_member(*members, "action")->Value);
		const auto* goal         = std::get_if<std::string>(&find_member(*members, "goal")->Value);
		auto        constraints  = draft_texts(find_member(*members, "constraints"));
		auto        scope        = draft_texts(find_member(*members, "write_scope"));
		auto        omissions    = draft_texts(find_member(*members, "omissions"));
		const auto* items        = std::get_if<json::array>(&find_member(*members, "items")->Value);
		const auto* context      = std::get_if<json::array>(&find_member(*members, "context")->Value);
		const auto* large        = std::get_if<bool>(&find_member(*members, "large_context")->Value);
		const auto  parsedAction = action ? enum_from(*action, actions) : std::nullopt;
		if (!schema || *schema != draft_schema || !source || !parsedAction || !goal || !constraints || !scope || !omissions || !items || !context || !large) {
			return std::nullopt;
		}
		auto result = cycle_round_selection{
			.Source       = cycle_source_revision_id{ *source },
			.Action       = *parsedAction,
			.Goal         = *goal,
			.Constraints  = std::move(*constraints),
			.WriteScope   = std::move(*scope),
			.Omissions    = std::move(*omissions),
			.LargeContext = *large,
		};
		for (const auto& value : *items) {
			auto item = draft_item(value);
			if (!item) {
				return std::nullopt;
			}
			result.Items.push_back(std::move(*item));
		}
		for (const auto& value : *context) {
			auto selected = draft_context(value);
			if (!selected) {
				return std::nullopt;
			}
			result.Context.push_back(std::move(*selected));
		}
		return result;
	}

	auto cycle_wire_name(const review_cycle_id aCycle) -> std::string
	{
		return "RC-" + std::to_string(aCycle.value());
	}

	auto cycle_wire_name(const cycle_round_id aRound) -> std::string
	{
		return "F" + std::to_string(aRound.value());
	}

	auto cycle_wire_name(const cycle_source_revision_id aRevision) -> std::string
	{
		return "R" + std::to_string(aRevision.value());
	}

	auto cycle_wire_name(const cycle_feedback_item_id aItem) -> std::string
	{
		return "I" + std::to_string(aItem.value());
	}

	auto cycle_wire_name(const cycle_suggestion_ref& aSuggestion) -> std::string
	{
		return "SG" + std::to_string(aSuggestion.Item.Id.value()) + "." + std::to_string(aSuggestion.Id.value());
	}

	auto cycle_wire_project(const local_cycle_subject& aSubject) -> std::string
	{
		return "P" + cycle_hex(derive_workspace_project(aSubject).bytes());
	}

	auto freeze_readiness(const cycle_draft_editor_state& aEditor, const std::uint64_t aStoredRevision) noexcept -> std::optional<cycle_round_problem>
	{
		if (aEditor.Composing || aEditor.SavePending) {
			return cycle_round_problem::unflushed;
		}
		if (aEditor.ShownRevision != aStoredRevision) {
			return cycle_round_problem::stale_draft;
		}
		return std::nullopt;
	}

	auto assemble_round(const cycle_round_inputs& aInputs) -> std::expected<cycle_round_payload, cycle_round_refusal>
	{
		if (!aInputs.Cycle || !aInputs.Selection || !valid(aInputs.Cycle->Subject) || !valid_cycle_id(aInputs.Round)) {
			return refusal(cycle_round_problem::invalid_selection);
		}
		const auto& cycle     = *aInputs.Cycle;
		const auto& selection = *aInputs.Selection;
		if (!allocated(cycle, selection.Source) || !aInputs.SourceCapture) {
			return refusal(cycle_round_problem::invalid_source);
		}
		if (!check_assignment_text(selection) || !utf8_within(cycle.Title, maximum_round_text_bytes)) {
			return refusal(cycle_round_problem::invalid_text);
		}
		if (!check_write_scope(selection.WriteScope)) {
			return refusal(cycle_round_problem::invalid_selection);
		}
		if (selection.Items.empty()) {
			return refusal(cycle_round_problem::empty);
		}
		auto work                     = assembler{ .Inputs = aInputs, .Cycle = cycle, .Selection = selection };
		work.Measure.Items            = selection.Items.size();
		work.Measure.ContextBudget    = selection.LargeContext ? maximum_round_context_bytes : default_round_context_bytes;
		work.Measure.InstructionBytes = selection.Goal.size();
		for (const auto& text : selection.Constraints) {
			work.Measure.InstructionBytes += text.size();
		}
		for (const auto& text : selection.WriteScope) {
			work.Measure.InstructionBytes += text.size();
		}
		for (const auto& text : selection.Omissions) {
			work.Measure.InstructionBytes += text.size();
		}
		if (selection.Items.size() > maximum_round_items) {
			return std::unexpected(cycle_round_refusal{ .Problem = cycle_round_problem::oversized, .Measure = work.Measure });
		}
		auto seen  = std::set<cycle_feedback_item_id>{ };
		auto items = std::vector<cycle_wire_item>{ };
		items.reserve(selection.Items.size());
		for (std::size_t index = 0; index < selection.Items.size(); ++index) {
			if (!valid_cycle_id(selection.Items[index].Item) || !seen.insert(selection.Items[index].Item).second) {
				return refusal(cycle_round_problem::invalid_selection, index);
			}
			auto item = work.item(index);
			if (!item) {
				return std::unexpected(item.error());
			}
			items.push_back(std::move(*item));
		}
		auto sources = work.sources();
		if (!sources) {
			return std::unexpected(sources.error());
		}
		if (work.Measure.InstructionBytes > maximum_round_instruction_bytes || work.Measure.ContextBytes > work.Measure.ContextBudget) {
			return std::unexpected(cycle_round_refusal{ .Problem = cycle_round_problem::oversized, .Measure = work.Measure });
		}
		const auto& capture     = *aInputs.SourceCapture;
		const auto  consistency = capture.Coverage == cycle_capture_coverage::partial ? cycle_source_consistency::incomplete
			: capture.Quality == cycle_capture_quality::immutable_source              ? cycle_source_consistency::immutable_source
																					  : cycle_source_consistency::observed_stable;
		auto        payload     = cycle_round_payload{
			.CycleId        = cycle_wire_name(cycle.Subject.Cycle),
			.RoundId        = cycle_wire_name(aInputs.Round),
			.SourceRevision = cycle_wire_name(selection.Source),
			.Consistency    = consistency,
			.Project        = { .Id = cycle_wire_project(cycle.Subject), .Label = cycle.Title, .ScopePaths = cycle.Scope.Paths },
			.Assignment     = { .Action = selection.Action, .Goal = selection.Goal, .Constraints = selection.Constraints, .WriteScope = selection.WriteScope },
			.Items          = std::move(items),
			.Sources        = std::move(*sources),
		};
		payload.Selection = { .ItemCount = payload.Items.size(), .SourceCount = payload.Sources.size(), .Omissions = selection.Omissions, .Complete = true };
		return payload;
	}

	auto encode_round_payload(const cycle_round_payload& aPayload) -> std::optional<std::string>
	{
		return canonical_cycle_json(payload_json(aPayload));
	}

	auto round_digest(const std::string_view aCanonicalPayload) -> cycle_round_digest
	{
		return cycle_round_digest{ sha256(bytes_of(aCanonicalPayload)) };
	}

	auto encode_round_envelope(const cycle_round_payload& aPayload) -> std::optional<std::string>
	{
		const auto canonical = encode_round_payload(aPayload);
		if (!canonical) {
			return std::nullopt;
		}
		auto document = canonical_cycle_json(json{ json::object{
			{ .Key = "schema", .Value = text(std::string{ envelope_schema }) },
			{ .Key = "canonicalization", .Value = text(std::string{ canonical_schema }) },
			{ .Key = "payload_sha256", .Value = text(cycle_hex(round_digest(*canonical).bytes())) },
			{ .Key = "payload", .Value = payload_json(aPayload) },
		} });
		if (!document) {
			return std::nullopt;
		}
		document->push_back('\n');
		return document;
	}

	auto decode_round_envelope(const std::string_view aDocument) -> std::expected<cycle_decoded_round, cycle_round_decode_error>
	{
		const auto parsed = parse_cycle_json(aDocument, maximum_round_document_bytes);
		if (!parsed) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::json, .Json = parsed.error() });
		}
		static constexpr auto keys    = std::array<std::string_view, 4>{ "schema", "canonicalization", "payload_sha256", "payload" };
		const auto*           members = std::get_if<json::object>(&parsed->Value);
		if (!members || !has_exactly(*members, keys)) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::shape });
		}
		const auto* schema   = std::get_if<std::string>(&find_member(*members, "schema")->Value);
		const auto* profile  = std::get_if<std::string>(&find_member(*members, "canonicalization")->Value);
		const auto* declared = std::get_if<std::string>(&find_member(*members, "payload_sha256")->Value);
		if (!schema || !profile || !declared) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::shape });
		}
		if (*schema != envelope_schema || *profile != canonical_schema) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::version });
		}
		const auto digest = parse_cycle_sha256(*declared);
		if (!digest) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::identifier });
		}
		return finish_decode(find_member(*members, "payload"), cycle_round_digest{ *digest });
	}

	auto decode_round_payload(const std::string_view aCanonical) -> std::expected<cycle_decoded_round, cycle_round_decode_error>
	{
		const auto parsed = parse_cycle_json(aCanonical, maximum_round_document_bytes);
		if (!parsed) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::json, .Json = parsed.error() });
		}
		auto decoded = finish_decode(&*parsed, std::nullopt);
		if (decoded && decoded->Canonical != aCanonical) {
			return std::unexpected(cycle_round_decode_error{ .Problem = cycle_round_decode_problem::noncanonical });
		}
		return decoded;
	}
}
