#include "github_capability_probe.hpp"
#include "bpr/core/json_preflight.hpp"
#include "github_graphql_operation_policy.hpp"
#include "github_http_helpers.hpp"

#include <QRegularExpression>
#include <QSet>
#include <QString>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		// Introspection is a type-dependent tree. Its shape validators below decide
		// which members are required/null for each selected kind before facts leave
		// this adapter. Keep full-width integer storage for extension values too.
		using capability_json   = glz::generic_sorted_u64;
		using capability_object = capability_json::object_t;
		using capability_array  = capability_json::array_t;

		struct capability_json_options : glz::opts
		{
			// These are the option names Glaze discovers, not BPR member names.
			// NOLINTBEGIN(readability-identifier-naming)
			bool escape_control_characters    = true;
			bool validate_skipped             = true;
			bool validate_trailing_whitespace = true;
			// NOLINTEND(readability-identifier-naming)
			constexpr capability_json_options()
			{
				null_terminated = false;
				// Null members are part of the introspection shape, not omissions.
				skip_null_members = false;
			}
		};

		struct capability_query_json
		{
			std::string_view Query;
			struct glaze
			{
				static constexpr auto value = glz::object("query", &capability_query_json::Query);
			};
		};

		[[nodiscard]] auto json_object(const capability_json& aValue) -> const capability_object&
		{
			static const capability_object empty;
			return aValue.is_object() ? aValue.get_object() : empty;
		}
		[[nodiscard]] auto json_array(const capability_json& aValue) -> const capability_array&
		{
			static const capability_array empty;
			return aValue.is_array() ? aValue.get_array() : empty;
		}
		[[nodiscard]] auto json_member(const capability_object& aObject, const std::string_view aKey) -> const capability_json&
		{
			static const capability_json absent;
			const auto                   found = aObject.find(aKey);
			return found == aObject.end() ? absent : found->second;
		}
		[[nodiscard]] auto json_member(const capability_object& aObject, const QString& aKey) -> const capability_json&
		{
			return json_member(aObject, std::string_view{ aKey.toStdString() });
		}
		[[nodiscard]] auto json_member(const capability_object& aObject, const char* aKey) -> const capability_json&
		{
			return json_member(aObject, std::string_view{ aKey });
		}
		[[nodiscard]] auto has_member(const capability_object& aObject, const QString& aKey) -> bool
		{
			return aObject.contains(aKey.toStdString());
		}
		[[nodiscard]] auto has_member(const capability_object& aObject, const char* aKey) -> bool
		{
			return aObject.contains(aKey);
		}
		[[nodiscard]] auto json_text(const capability_json& aValue) -> QString
		{
			return aValue.is_string() ? QString::fromStdString(aValue.get_string()) : QString{ };
		}
		[[nodiscard]] auto parse_json_object(const std::string_view aBody) -> std::optional<capability_object>
		{
			auto object = capability_object{ };
			if (glz::read<capability_json_options{ }>(object, aBody)) {
				return std::nullopt;
			}
			return object;
		}

		constexpr std::array<std::string_view, capability_probe_part_count> graphql_documents{
			"query BetterPullReviewCapabilityProbeV1Part0{schemaRoot:__schema{queryType{...FieldType}mutationType{...FieldType}}markInput:__type(name:\"MarkFileAsViewedInput\"){...InputType}unmarkInput:__type(name:\"UnmarkFileAsViewedInput\"){...InputType}threadSubject:__type(name:\"PullRequestReviewThreadSubjectType\"){...EnumType}diffSide:__type(name:\"DiffSide\"){...EnumType}reviewThreadInterfaces:__type(name:\"PullRequestReviewThread\"){...InterfaceType}actorPossibleTypes:__type(name:\"Actor\"){...PossibleType}nodePossibleTypes:__type(name:\"Node\"){...PossibleType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}fragment InputType on __Type{kind name inputFields{name defaultValue type{...TypeRef}}}fragment EnumType on __Type{kind name enumValues(includeDeprecated:true){name}}fragment InterfaceType on __Type{kind name interfaces{kind name}}fragment PossibleType on __Type{kind name possibleTypes{kind name}}",
			"query BetterPullReviewCapabilityProbeV1Part1{repository:__type(name:\"Repository\"){...FieldType}pullRequest:__type(name:\"PullRequest\"){...FieldType}resolveInput:__type(name:\"ResolveReviewThreadInput\"){...InputType}unresolveInput:__type(name:\"UnresolveReviewThreadInput\"){...InputType}fileViewedState:__type(name:\"FileViewedState\"){...EnumType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}fragment InputType on __Type{kind name inputFields{name defaultValue type{...TypeRef}}}fragment EnumType on __Type{kind name enumValues(includeDeprecated:true){name}}",
			"query BetterPullReviewCapabilityProbeV1Part2{threadConnection:__type(name:\"PullRequestReviewThreadConnection\"){...FieldType}reviewThread:__type(name:\"PullRequestReviewThread\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
			"query BetterPullReviewCapabilityProbeV1Part3{commentConnection:__type(name:\"PullRequestReviewCommentConnection\"){...FieldType}reviewComment:__type(name:\"PullRequestReviewComment\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
			"query BetterPullReviewCapabilityProbeV1Part4{changedFileConnection:__type(name:\"PullRequestChangedFileConnection\"){...FieldType}changedFile:__type(name:\"PullRequestChangedFile\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
			"query BetterPullReviewCapabilityProbeV1Part5{pageInfo:__type(name:\"PageInfo\"){...FieldType}actor:__type(name:\"Actor\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
			"query BetterPullReviewCapabilityProbeV1Part6{node:__type(name:\"Node\"){...FieldType}markPayload:__type(name:\"MarkFileAsViewedPayload\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
			"query BetterPullReviewCapabilityProbeV1Part7{unmarkPayload:__type(name:\"UnmarkFileAsViewedPayload\"){...FieldType}resolvePayload:__type(name:\"ResolveReviewThreadPayload\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
			"query BetterPullReviewCapabilityProbeV1Part8{unresolvePayload:__type(name:\"UnresolveReviewThreadPayload\"){...FieldType}}fragment TypeRef on __Type{kind name ofType{kind name}}fragment FieldType on __Type{kind name fields(includeDeprecated:true){name args{name defaultValue type{...TypeRef}} type{...TypeRef}}}",
		};

		constexpr std::array<std::array<std::string_view, 8>, capability_probe_part_count> part_aliases{ {
			{ { "schemaRoot", "markInput", "unmarkInput", "threadSubject", "diffSide", "reviewThreadInterfaces", "actorPossibleTypes", "nodePossibleTypes" } },
			{ { "repository", "pullRequest", "resolveInput", "unresolveInput", "fileViewedState" } },
			{ { "threadConnection", "reviewThread" } },
			{ { "commentConnection", "reviewComment" } },
			{ { "changedFileConnection", "changedFile" } },
			{ { "pageInfo", "actor" } },
			{ { "node", "markPayload" } },
			{ { "unmarkPayload", "resolvePayload" } },
			{ { "unresolvePayload" } },
		} };

		constexpr capability_probe_limits hard_limits{ };

		constexpr std::array<std::string_view, 26> response_aliases{
			"schemaRoot",
			"repository",
			"pullRequest",
			"threadConnection",
			"reviewThread",
			"commentConnection",
			"reviewComment",
			"changedFileConnection",
			"changedFile",
			"pageInfo",
			"actor",
			"node",
			"reviewThreadInterfaces",
			"actorPossibleTypes",
			"nodePossibleTypes",
			"threadSubject",
			"diffSide",
			"fileViewedState",
			"markInput",
			"markPayload",
			"unmarkInput",
			"unmarkPayload",
			"resolveInput",
			"resolvePayload",
			"unresolveInput",
			"unresolvePayload",
		};

		using json_preflight_result = core::json_preflight_result;

		/// [[BPR-1433]]. The shared core check with the probe's ceilings.
		class json_structural_preflight
		{
		public:
			json_structural_preflight(const std::string_view aInput, const capability_probe_limits& aLimits) noexcept
				: Input{ aInput }
				, Limits{ aLimits }
			{
			}

			[[nodiscard]] auto run() noexcept -> json_preflight_result
			{
				const auto report = core::json_preflight(Input, { .Depth = Limits.MaximumDepth, .ObjectMembers = Limits.MaximumObjectMembers, .ArrayElements = Limits.MaximumArrayElements, .Values = Limits.MaximumTotalValues, .StringBytes = Limits.MaximumStringBytes });
				Values            = report.Values;
				return report.Result;
			}
			[[nodiscard]] auto value_count() const noexcept -> std::size_t { return Values; }

		private:
			std::string_view               Input;
			const capability_probe_limits& Limits;
			std::size_t                    Values = 0U;
		};

		enum class selected_type_shape : std::uint8_t {
			fields,
			input_fields,
			enum_values,
			interfaces,
			possible_types,
		};

		enum class type_wrapper : std::uint8_t {
			named,
			non_null,
			list,
		};

		struct type_requirement
		{
			type_wrapper     Wrapper = type_wrapper::named;
			std::string_view Kind;
			std::string_view Name;
		};

		struct argument_requirement
		{
			std::string_view Name;
			type_requirement Type;
		};

		struct selected_schema_type
		{
			bool             Missing = false;
			QString          Kind;
			QString          Name;
			bool             MembersAvailable = false;
			capability_array Members;
		};

		[[nodiscard]] auto kind_has_selected_members(const QString&            aKind,
													 const selected_type_shape aShape)
			-> bool
		{
			switch (aShape) {
			case selected_type_shape::fields:
				return aKind == QStringLiteral("OBJECT")
					|| aKind == QStringLiteral("INTERFACE");
			case selected_type_shape::input_fields:
				return aKind == QStringLiteral("INPUT_OBJECT");
			case selected_type_shape::enum_values:
				return aKind == QStringLiteral("ENUM");
			case selected_type_shape::interfaces:
				return aKind == QStringLiteral("OBJECT")
					|| aKind == QStringLiteral("INTERFACE");
			case selected_type_shape::possible_types:
				return aKind == QStringLiteral("INTERFACE")
					|| aKind == QStringLiteral("UNION");
			}
			return false;
		}

		[[nodiscard]] auto fact_index(const capability_schema_fact aFact) noexcept
			-> std::size_t
		{
			return static_cast<std::size_t>(aFact);
		}

		[[nodiscard]] auto
		unavailable(const capability_probe_unavailability           aReason,
					const application::github_capability_diagnostic aDetail = application::github_capability_diagnostic::unavailable_detail) noexcept
			-> capability_probe_observation
		{
			return {
				.Capabilities     = { },
				.Facts            = { },
				.Unavailability   = aReason,
				.Complete         = false,
				.CapabilityDetail = aDetail,
			};
		}

		[[nodiscard]] auto graphql_error_code_detail(const QString& aCode)
			-> application::github_capability_diagnostic
		{
			if (aCode == QStringLiteral("FORBIDDEN")) {
				return application::github_capability_diagnostic::graphql_code_forbidden;
			}
			if (aCode == QStringLiteral("UNAUTHORIZED")) {
				return application::github_capability_diagnostic::graphql_code_unauthorized;
			}
			if (aCode == QStringLiteral("NOT_FOUND")) {
				return application::github_capability_diagnostic::graphql_code_not_found;
			}
			if (aCode == QStringLiteral("MAX_NODE_LIMIT_EXCEEDED")) {
				return application::github_capability_diagnostic::graphql_code_max_node_limit_exceeded;
			}
			if (aCode == QStringLiteral("INTERNAL")) {
				return application::github_capability_diagnostic::graphql_code_internal;
			}
			if (aCode == QStringLiteral("SERVICE_UNAVAILABLE")) {
				return application::github_capability_diagnostic::graphql_code_service_unavailable;
			}
			if (aCode == QStringLiteral("GRAPHQL_PARSE_FAILED")) {
				return application::github_capability_diagnostic::graphql_code_parse_failed;
			}
			if (aCode == QStringLiteral("GRAPHQL_VALIDATION_FAILED")) {
				return application::github_capability_diagnostic::graphql_code_validation_failed;
			}
			if (aCode == QStringLiteral("undefinedField")) {
				return application::github_capability_diagnostic::graphql_code_undefined_field;
			}
			if (aCode == QStringLiteral("argumentNotAccepted")) {
				return application::github_capability_diagnostic::graphql_code_argument_not_accepted;
			}
			if (aCode == QStringLiteral("undefinedType")) {
				return application::github_capability_diagnostic::graphql_code_undefined_type;
			}
			if (aCode == QStringLiteral("selectionMismatch")) {
				return application::github_capability_diagnostic::graphql_code_selection_mismatch;
			}
			if (aCode == QStringLiteral("variableMismatch")) {
				return application::github_capability_diagnostic::graphql_code_variable_mismatch;
			}
			if (aCode == QStringLiteral("variableNotUsed")) {
				return application::github_capability_diagnostic::graphql_code_variable_not_used;
			}
			if (aCode == QStringLiteral("variableRequiresValidType")) {
				return application::github_capability_diagnostic::graphql_code_variable_requires_valid_type;
			}
			if (aCode == QStringLiteral("variableNotDefined")) {
				return application::github_capability_diagnostic::graphql_code_variable_not_defined;
			}
			return application::github_capability_diagnostic::graphql_errors;
		}

		// This refines presentation only after the existing non-rate refusal.
		// Neither message/location/path nor unrecognized strings leave this parser.
		[[nodiscard]] auto graphql_error_detail(const capability_json& aErrors) noexcept
			-> application::github_capability_diagnostic
		{
			try {
				using diagnostic = application::github_capability_diagnostic;
				if (!aErrors.is_array()) {
					return diagnostic::graphql_errors_not_array;
				}
				const auto& errors = json_array(aErrors);
				if (errors.empty()) {
					return diagnostic::graphql_errors_empty;
				}
				std::optional<diagnostic> selected;
				bool                      unknown       = false;
				bool                      mixed         = false;
				std::size_t               markerCount   = 0U;
				const auto                observeMarker = [&](const capability_json& aMarker) {
					++markerCount;
					if (!aMarker.is_string()) {
						return false;
					}
					const auto detail = graphql_error_code_detail(json_text(aMarker));
					if (detail == diagnostic::graphql_errors) {
						unknown = true;
					} else if (selected && *selected != detail) {
						mixed = true;
					} else {
						selected = detail;
					}
					return true;
				};
				for (const auto& value : errors) {
					if (!value.is_object()) {
						return diagnostic::graphql_errors_malformed;
					}
					const auto& entry  = json_object(value);
					const auto  before = markerCount;
					if (has_member(entry, "type") && !observeMarker(json_member(entry, "type"))) {
						return diagnostic::graphql_errors_malformed;
					}
					if (has_member(entry, "extensions")) {
						if (!json_member(entry, "extensions").is_object()) {
							return diagnostic::graphql_errors_malformed;
						}
						const auto& extensions = json_object(json_member(entry, "extensions"));
						for (const auto* key : { "type", "code" }) {
							if (has_member(extensions, key) && !observeMarker(json_member(extensions, key))) {
								return diagnostic::graphql_errors_malformed;
							}
						}
					}
					unknown = unknown || markerCount == before;
				}
				if (mixed || (unknown && (selected || markerCount > 1U || errors.size() > 1))) {
					return diagnostic::graphql_errors_mixed;
				}
				return selected.value_or(diagnostic::graphql_errors);
			} catch (...) {
				// Diagnostic allocation/inspection must not change refusal policy.
				return application::github_capability_diagnostic::graphql_errors;
			}
		}

		[[nodiscard]] auto valid_limits(const capability_probe_limits& aLimits) noexcept
			-> bool
		{
			const auto within = [](const std::size_t aValue, const std::size_t aMaximum) {
				return aValue != 0U && aValue <= aMaximum;
			};
			return within(aLimits.MaximumRequestBytes, hard_limits.MaximumRequestBytes)
				&& within(aLimits.MaximumResponseBytes,
						  hard_limits.MaximumResponseBytes)
				&& within(aLimits.MaximumDepth, hard_limits.MaximumDepth)
				&& within(aLimits.MaximumObjectMembers,
						  hard_limits.MaximumObjectMembers)
				&& within(aLimits.MaximumArrayElements,
						  hard_limits.MaximumArrayElements)
				&& within(aLimits.MaximumQueryFields, hard_limits.MaximumQueryFields)
				&& within(aLimits.MaximumMutationFields,
						  hard_limits.MaximumMutationFields)
				&& within(aLimits.MaximumRepositoryFields,
						  hard_limits.MaximumRepositoryFields)
				&& within(aLimits.MaximumPullRequestFields,
						  hard_limits.MaximumPullRequestFields)
				&& within(aLimits.MaximumOtherTypeFields,
						  hard_limits.MaximumOtherTypeFields)
				&& within(aLimits.MaximumNamedTypes, hard_limits.MaximumNamedTypes)
				&& within(
					   aLimits.MaximumArguments,
					   hard_limits.MaximumArguments)
				&& within(
					   aLimits.MaximumInputFields,
					   hard_limits.MaximumInputFields)
				&& within(
					   aLimits.MaximumEnumValues,
					   hard_limits.MaximumEnumValues)
				&& within(
					   aLimits.MaximumTotalValues,
					   hard_limits.MaximumTotalValues)
				&& within(
					   aLimits.MaximumStringBytes,
					   hard_limits.MaximumStringBytes);
		}

		[[nodiscard]] auto exact_keys(
			const capability_object&             aObject,
			const std::initializer_list<QString> aKeys) -> bool
		{
			if (aObject.size() != static_cast<std::size_t>(aKeys.size())) {
				return false;
			}
			return std::ranges::all_of(
				aKeys,
				[&aObject](const QString& aKey) { return has_member(aObject, aKey); });
		}

		[[nodiscard]] auto valid_kind(const QString& aKind) -> bool
		{
			static const QSet<QString> kinds{
				QStringLiteral("SCALAR"),
				QStringLiteral("OBJECT"),
				QStringLiteral("INTERFACE"),
				QStringLiteral("UNION"),
				QStringLiteral("ENUM"),
				QStringLiteral("INPUT_OBJECT"),
				QStringLiteral("LIST"),
				QStringLiteral("NON_NULL"),
			};
			return kinds.contains(aKind);
		}

		[[nodiscard]] auto valid_type_reference(const capability_json& aValue) -> bool
		{
			if (!aValue.is_object()) {
				return false;
			}
			const auto& object = json_object(aValue);
			if (
				!exact_keys(
					object,
					{
						QStringLiteral("kind"),
						QStringLiteral("name"),
						QStringLiteral("ofType"),
					})
				|| !json_member(object, "kind").is_string()) {
				return false;
			}

			const auto kind = json_text(json_member(object, "kind"));
			if (!valid_kind(kind)) {
				return false;
			}
			const bool wrapper = kind == QStringLiteral("LIST")
				|| kind == QStringLiteral("NON_NULL");
			if (wrapper) {
				if (!json_member(object, "name").is_null()) {
					return false;
				}
				const auto& inner = json_member(object, "ofType");
				if (!inner.is_object()) {
					return false;
				}
				const auto& innerObject = json_object(inner);
				if (
					!exact_keys(
						innerObject,
						{ QStringLiteral("kind"), QStringLiteral("name") })
					|| !json_member(innerObject, "kind").is_string()
					|| !valid_kind(json_text(json_member(innerObject, "kind")))) {
					return false;
				}
				const auto innerKind    = json_text(json_member(innerObject, "kind"));
				const bool innerWrapper = innerKind == QStringLiteral("LIST")
					|| innerKind == QStringLiteral("NON_NULL");
				return innerWrapper
					? json_member(innerObject, "name").is_null()
					: json_member(innerObject, "name").is_string()
						&& !json_text(json_member(innerObject, "name")).isEmpty();
			}
			return json_member(object, "name").is_string()
				&& !json_text(json_member(object, "name")).isEmpty()
				&& json_member(object, "ofType").is_null();
		}

		[[nodiscard]] auto exact_type(
			const capability_json&  aValue,
			const type_requirement& aRequirement) -> bool
		{
			if (!valid_type_reference(aValue)) {
				return false;
			}
			const auto& object       = json_object(aValue);
			const auto  expectedKind = QString::fromUtf8(
				aRequirement.Kind.data(),
				static_cast<qsizetype>(aRequirement.Kind.size()));
			const auto expectedName = QString::fromUtf8(
				aRequirement.Name.data(),
				static_cast<qsizetype>(aRequirement.Name.size()));

			switch (aRequirement.Wrapper) {
			case type_wrapper::named:
				return json_text(json_member(object, "kind")) == expectedKind
					&& json_text(json_member(object, "name")) == expectedName
					&& json_member(object, "ofType").is_null();
			case type_wrapper::non_null:
			case type_wrapper::list:     {
				const auto wrapper = aRequirement.Wrapper
						== type_wrapper::non_null
					? QStringLiteral("NON_NULL")
					: QStringLiteral("LIST");
				if (json_text(json_member(object, "kind")) != wrapper) {
					return false;
				}
				const auto& inner = json_object(json_member(object, "ofType"));
				return json_text(json_member(inner, "kind")) == expectedKind
					&& json_text(json_member(inner, "name")) == expectedName;
			}
			}
			return false;
		}

		[[nodiscard]] auto required_argument(const capability_object& aArgument) -> bool
		{
			return json_text(json_member(json_object(json_member(aArgument, "type")), "kind"))
				== QStringLiteral("NON_NULL")
				&& json_member(aArgument, "defaultValue").is_null();
		}

		[[nodiscard]] auto validate_arguments(
			const capability_array&        aArguments,
			const capability_probe_limits& aLimits) -> bool
		{
			if (
				aArguments.size()
				> static_cast<std::size_t>(aLimits.MaximumArguments)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aArguments) {
				if (!value.is_object()) {
					return false;
				}
				const auto& object = json_object(value);
				if (
					!exact_keys(
						object,
						{
							QStringLiteral("name"),
							QStringLiteral("defaultValue"),
							QStringLiteral("type"),
						})
					|| !json_member(object, "name").is_string()
					|| json_text(json_member(object, "name")).isEmpty()
					|| (!json_member(object, "defaultValue").is_null()
						&& !json_member(object, "defaultValue").is_string())
					|| !valid_type_reference(json_member(object, "type"))) {
					return false;
				}
				const auto name = json_text(json_member(object, "name"));
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto validate_field_members(
			const capability_array&        aFields,
			const std::size_t              aMaximum,
			const capability_probe_limits& aLimits) -> bool
		{
			if (aFields.size() > static_cast<std::size_t>(aMaximum)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aFields) {
				if (!value.is_object()) {
					return false;
				}
				const auto& object = json_object(value);
				if (
					!exact_keys(
						object,
						{
							QStringLiteral("name"),
							QStringLiteral("args"),
							QStringLiteral("type"),
						})
					|| !json_member(object, "name").is_string()
					|| json_text(json_member(object, "name")).isEmpty()
					|| !json_member(object, "args").is_array()
					|| !validate_arguments(
						json_array(json_member(object, "args")),
						aLimits)
					|| !valid_type_reference(json_member(object, "type"))) {
					return false;
				}
				const auto name = json_text(json_member(object, "name"));
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto validate_input_members(
			const capability_array&        aFields,
			const capability_probe_limits& aLimits) -> bool
		{
			if (
				aFields.size()
				> static_cast<std::size_t>(aLimits.MaximumInputFields)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aFields) {
				if (!value.is_object()) {
					return false;
				}
				const auto& object = json_object(value);
				if (
					!exact_keys(
						object,
						{
							QStringLiteral("name"),
							QStringLiteral("defaultValue"),
							QStringLiteral("type"),
						})
					|| !json_member(object, "name").is_string()
					|| json_text(json_member(object, "name")).isEmpty()
					|| (!json_member(object, "defaultValue").is_null()
						&& !json_member(object, "defaultValue").is_string())
					|| !valid_type_reference(json_member(object, "type"))) {
					return false;
				}
				const auto name = json_text(json_member(object, "name"));
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto validate_enum_members(
			const capability_array&        aValues,
			const capability_probe_limits& aLimits) -> bool
		{
			if (
				aValues.size()
				> static_cast<std::size_t>(aLimits.MaximumEnumValues)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aValues) {
				if (!value.is_object()) {
					return false;
				}
				const auto& object = json_object(value);
				if (
					!exact_keys(object, { QStringLiteral("name") })
					|| !json_member(object, "name").is_string()
					|| json_text(json_member(object, "name")).isEmpty()) {
					return false;
				}
				const auto name = json_text(json_member(object, "name"));
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto
		validate_named_type_members(const capability_array&        aValues,
									const capability_probe_limits& aLimits) -> bool
		{
			if (aValues.size() > static_cast<std::size_t>(aLimits.MaximumNamedTypes)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aValues) {
				if (!value.is_object()) {
					return false;
				}
				const auto& object = json_object(value);
				if (!exact_keys(object,
								{
									QStringLiteral("kind"),
									QStringLiteral("name"),
								})
					|| !json_member(object, "kind").is_string()
					|| !valid_kind(json_text(json_member(object, "kind")))
					|| !json_member(object, "name").is_string()
					|| json_text(json_member(object, "name")).isEmpty()) {
					return false;
				}
				const auto name = json_text(json_member(object, "name"));
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto parse_selected_type(const capability_json&         aValue,
											   const std::string_view         aExpectedName,
											   const selected_type_shape      aShape,
											   const std::size_t              aMaximumFields,
											   const capability_probe_limits& aLimits,
											   selected_schema_type&          aOutput) -> bool
		{
			if (aValue.is_null()) {
				aOutput = {
					.Missing          = true,
					.Kind             = { },
					.Name             = { },
					.MembersAvailable = false,
					.Members          = { },
				};
				return true;
			}
			if (!aValue.is_object()) {
				return false;
			}

			const auto& object     = json_object(aValue);
			const auto  memberName = aShape == selected_type_shape::fields
				? QStringLiteral("fields")
				: aShape == selected_type_shape::input_fields
				? QStringLiteral("inputFields")
				: aShape == selected_type_shape::enum_values
				? QStringLiteral("enumValues")
				: aShape == selected_type_shape::interfaces
				? QStringLiteral("interfaces")
				: QStringLiteral("possibleTypes");
			if (
				!exact_keys(
					object,
					{
						QStringLiteral("kind"),
						QStringLiteral("name"),
						memberName,
					})
				|| !json_member(object, "kind").is_string()
				|| !valid_kind(json_text(json_member(object, "kind")))
				|| !json_member(object, "name").is_string()
				|| json_text(json_member(object, "name")).isEmpty()) {
				return false;
			}
			if (!aExpectedName.empty()) {
				const auto expected = QString::fromUtf8(
					aExpectedName.data(),
					static_cast<qsizetype>(aExpectedName.size()));
				if (json_text(json_member(object, "name")) != expected) {
					return false;
				}
			}

			aOutput = {
				.Missing          = false,
				.Kind             = json_text(json_member(object, "kind")),
				.Name             = json_text(json_member(object, "name")),
				.MembersAvailable = false,
				.Members          = { },
			};
			const auto& members = json_member(object, memberName);
			if (members.is_null()) {
				return !kind_has_selected_members(aOutput.Kind, aShape);
			}
			if (!members.is_array()) {
				return false;
			}
			aOutput.MembersAvailable = true;
			aOutput.Members          = json_array(members);
			switch (aShape) {
			case selected_type_shape::fields:
				return validate_field_members(
					aOutput.Members,
					aMaximumFields,
					aLimits);
			case selected_type_shape::input_fields:
				return validate_input_members(aOutput.Members, aLimits);
			case selected_type_shape::enum_values:
				return validate_enum_members(aOutput.Members, aLimits);
			case selected_type_shape::interfaces:
			case selected_type_shape::possible_types:
				return validate_named_type_members(aOutput.Members, aLimits);
			}
			return false;
		}

		[[nodiscard]] auto find_named_member(
			const capability_array& aMembers,
			const std::string_view  aName) -> std::optional<capability_object>
		{
			const auto expected = QString::fromUtf8(
				aName.data(),
				static_cast<qsizetype>(aName.size()));
			for (const auto& value : aMembers) {
				const auto& object = json_object(value);
				if (json_text(json_member(object, "name")) == expected) {
					return object;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto field_requirement(
			const selected_schema_type&                       aType,
			const std::string_view                            aExpectedKind,
			const std::string_view                            aFieldName,
			const type_requirement&                           aReturnType,
			const std::initializer_list<argument_requirement> aExpectedArguments = { })
			-> capability_schema_fact_state
		{
			if (aType.Missing) {
				return capability_schema_fact_state::missing;
			}
			if (
				aType.Kind
					!= QString::fromUtf8(
						aExpectedKind.data(),
						static_cast<qsizetype>(aExpectedKind.size()))
				|| !aType.MembersAvailable) {
				return capability_schema_fact_state::incompatible;
			}
			const auto field = find_named_member(aType.Members, aFieldName);
			if (!field.has_value()) {
				return capability_schema_fact_state::missing;
			}
			if (!exact_type(json_member(*field, "type"), aReturnType)) {
				return capability_schema_fact_state::incompatible;
			}

			const auto&   arguments = json_array(json_member(*field, "args"));
			QSet<QString> expectedNames;
			for (const auto& expected : aExpectedArguments) {
				expectedNames.insert(
					QString::fromUtf8(
						expected.Name.data(),
						static_cast<qsizetype>(expected.Name.size())));
				const auto argument = find_named_member(arguments, expected.Name);
				if (
					!argument.has_value()
					|| !exact_type(json_member(*argument, "type"), expected.Type)) {
					return capability_schema_fact_state::incompatible;
				}
			}
			for (const auto& value : arguments) {
				const auto& argument = json_object(value);
				if (
					!expectedNames.contains(
						json_text(json_member(argument, "name")))
					&& required_argument(argument)) {
					return capability_schema_fact_state::incompatible;
				}
			}
			return capability_schema_fact_state::matched;
		}

		[[nodiscard]] auto input_requirement(
			const selected_schema_type&                       aType,
			const std::initializer_list<argument_requirement> aExpectedFields)
			-> capability_schema_fact_state
		{
			if (aType.Missing) {
				return capability_schema_fact_state::missing;
			}
			if (
				aType.Kind != QStringLiteral("INPUT_OBJECT")
				|| !aType.MembersAvailable) {
				return capability_schema_fact_state::incompatible;
			}

			QSet<QString> expectedNames;
			for (const auto& expected : aExpectedFields) {
				expectedNames.insert(
					QString::fromUtf8(
						expected.Name.data(),
						static_cast<qsizetype>(expected.Name.size())));
				const auto field = find_named_member(aType.Members, expected.Name);
				if (
					!field.has_value()
					|| !exact_type(json_member(*field, "type"), expected.Type)) {
					return capability_schema_fact_state::incompatible;
				}
			}
			for (const auto& value : aType.Members) {
				const auto& field = json_object(value);
				if (
					!expectedNames.contains(json_text(json_member(field, "name")))
					&& required_argument(field)) {
					return capability_schema_fact_state::incompatible;
				}
			}
			return capability_schema_fact_state::matched;
		}

		[[nodiscard]] auto enum_requirement(
			const selected_schema_type&                   aType,
			const std::initializer_list<std::string_view> aRequiredValues)
			-> capability_schema_fact_state
		{
			if (aType.Missing) {
				return capability_schema_fact_state::missing;
			}
			if (aType.Kind != QStringLiteral("ENUM") || !aType.MembersAvailable) {
				return capability_schema_fact_state::incompatible;
			}
			for (const auto expected : aRequiredValues) {
				if (!find_named_member(aType.Members, expected).has_value()) {
					return capability_schema_fact_state::incompatible;
				}
			}
			return capability_schema_fact_state::matched;
		}

		[[nodiscard]] auto named_type_requirement(const selected_schema_type& aType,
												  const std::string_view      aExpectedKind,
												  const std::string_view      aMemberKind,
												  const std::string_view      aMemberName)
			-> capability_schema_fact_state
		{
			if (aType.Missing) {
				return capability_schema_fact_state::missing;
			}
			if (aType.Kind
					!= QString::fromUtf8(aExpectedKind.data(),
										 static_cast<qsizetype>(aExpectedKind.size()))
				|| !aType.MembersAvailable) {
				return capability_schema_fact_state::incompatible;
			}
			const auto expectedMemberKind = QString::fromUtf8(
				aMemberKind.data(), static_cast<qsizetype>(aMemberKind.size()));
			const auto member = find_named_member(aType.Members, aMemberName);
			return member.has_value()
					&& json_text(json_member(*member, "kind")) == expectedMemberKind
				? capability_schema_fact_state::matched
				: capability_schema_fact_state::incompatible;
		}

		[[nodiscard]] auto
		named_type_subset_requirement(const selected_schema_type& aLeft,
									  const selected_schema_type& aRight)
			-> capability_schema_fact_state
		{
			if (aLeft.Missing || aRight.Missing) {
				return capability_schema_fact_state::missing;
			}
			if (aLeft.Kind != QStringLiteral("INTERFACE")
				|| aRight.Kind != QStringLiteral("INTERFACE") || !aLeft.MembersAvailable
				|| !aRight.MembersAvailable) {
				return capability_schema_fact_state::incompatible;
			}
			if (aLeft.Members.empty()) {
				return capability_schema_fact_state::incompatible;
			}
			for (const auto& value : aLeft.Members) {
				const auto& member = json_object(value);
				if (json_text(json_member(member, "kind")) != QStringLiteral("OBJECT")) {
					return capability_schema_fact_state::incompatible;
				}
				const auto name     = json_text(json_member(member, "name")).toStdString();
				const auto matching = find_named_member(aRight.Members, name);
				if (!matching.has_value()
					|| json_text(json_member(*matching, "kind")) != QStringLiteral("OBJECT")) {
					return capability_schema_fact_state::incompatible;
				}
			}
			return capability_schema_fact_state::matched;
		}

		[[nodiscard]] auto merge_fact(
			const capability_schema_fact_state aLeft,
			const capability_schema_fact_state aRight) noexcept
			-> capability_schema_fact_state
		{
			if (
				aLeft == capability_schema_fact_state::incompatible
				|| aRight == capability_schema_fact_state::incompatible) {
				return capability_schema_fact_state::incompatible;
			}
			if (
				aLeft == capability_schema_fact_state::missing
				|| aRight == capability_schema_fact_state::missing) {
				return capability_schema_fact_state::missing;
			}
			if (
				aLeft == capability_schema_fact_state::unknown
				|| aRight == capability_schema_fact_state::unknown) {
				return capability_schema_fact_state::unknown;
			}
			return capability_schema_fact_state::matched;
		}

		template <typename... States>
		[[nodiscard]] auto combined_fact(
			const capability_schema_fact_state aFirst,
			const States... aRemaining) noexcept
			-> capability_schema_fact_state
		{
			auto result = aFirst;
			((result = merge_fact(result, aRemaining)), ...);
			return result;
		}

		[[nodiscard]] auto capability_state(
			const std::initializer_list<capability_schema_fact_state> aFacts) noexcept -> core::github_capability_state
		{
			if (std::ranges::all_of(aFacts, [](const auto aFact) {
					return aFact == capability_schema_fact_state::matched;
				})) {
				return core::github_capability_state::supported;
			}
			if (std::ranges::any_of(aFacts, [](const auto aFact) {
					return aFact == capability_schema_fact_state::missing
						|| aFact == capability_schema_fact_state::incompatible;
				})) {
				return core::github_capability_state::unsupported;
			}
			return core::github_capability_state::unknown;
		}

		[[nodiscard]] auto scalar(
			const std::string_view aName,
			const bool             aRequired = false) -> type_requirement
		{
			return {
				.Wrapper = aRequired ? type_wrapper::non_null : type_wrapper::named,
				.Kind    = "SCALAR",
				.Name    = aName,
			};
		}

		[[nodiscard]] auto object_type(
			const std::string_view aName,
			const bool             aRequired = false) -> type_requirement
		{
			return {
				.Wrapper = aRequired ? type_wrapper::non_null : type_wrapper::named,
				.Kind    = "OBJECT",
				.Name    = aName,
			};
		}

		[[nodiscard]] auto interface_type(
			const std::string_view aName) -> type_requirement
		{
			return {
				.Wrapper = type_wrapper::named,
				.Kind    = "INTERFACE",
				.Name    = aName,
			};
		}

		[[nodiscard]] auto enum_type(
			const std::string_view aName,
			const bool             aRequired = false) -> type_requirement
		{
			return {
				.Wrapper = aRequired ? type_wrapper::non_null : type_wrapper::named,
				.Kind    = "ENUM",
				.Name    = aName,
			};
		}

		[[nodiscard]] auto input_type(
			const std::string_view aName) -> type_requirement
		{
			return {
				.Wrapper = type_wrapper::non_null,
				.Kind    = "INPUT_OBJECT",
				.Name    = aName,
			};
		}

		[[nodiscard]] auto list_of_objects(
			const std::string_view aName) -> type_requirement
		{
			return {
				.Wrapper = type_wrapper::list,
				.Kind    = "OBJECT",
				.Name    = aName,
			};
		}

		[[nodiscard]] auto response_has_exact_aliases(
			const capability_object& aData) -> bool
		{
			return aData.size()
				== static_cast<std::size_t>(response_aliases.size())
				&& std::ranges::all_of(
					   response_aliases,
					   [&aData](const auto aAlias) {
						   return has_member(aData, QString::fromUtf8(aAlias.data(), static_cast<qsizetype>(aAlias.size())));
					   });
		}

		[[nodiscard]] auto field_array_exceeds_limits(
			const capability_json&         aValue,
			const std::size_t              aMaximumFields,
			const capability_probe_limits& aLimits) -> bool
		{
			if (!aValue.is_object()) {
				return false;
			}
			const auto& fields = json_member(json_object(aValue), "fields");
			if (!fields.is_array()) {
				return false;
			}
			const auto& array = json_array(fields);
			if (array.size() > static_cast<std::size_t>(aMaximumFields)) {
				return true;
			}
			return std::ranges::any_of(array, [&aLimits](const auto& aField) {
				return aField.is_object()
					&& json_member(json_object(aField), "args").is_array()
					&& json_array(json_member(json_object(aField), "args")).size()
					> static_cast<std::size_t>(aLimits.MaximumArguments);
			});
		}

		[[nodiscard]] auto schema_specific_limits_exceeded(
			const capability_object&       aData,
			const capability_probe_limits& aLimits) -> bool
		{
			const auto& schema = json_member(aData, "schemaRoot");
			if (
				schema.is_object()
				&& (field_array_exceeds_limits(json_member(json_object(schema), "queryType"),
											   aLimits.MaximumQueryFields, aLimits)
					|| field_array_exceeds_limits(
						json_member(json_object(schema), "mutationType"),
						aLimits.MaximumMutationFields, aLimits))) {
				return true;
			}
			if (field_array_exceeds_limits(json_member(aData, "repository"),
										   aLimits.MaximumRepositoryFields,
										   aLimits)) {
				return true;
			}
			if (
				field_array_exceeds_limits(
					json_member(aData, "pullRequest"),
					aLimits.MaximumPullRequestFields,
					aLimits)) {
				return true;
			}

			constexpr std::array<std::string_view, 13> fieldAliases{
				"threadConnection",
				"reviewThread",
				"commentConnection",
				"reviewComment",
				"changedFileConnection",
				"changedFile",
				"pageInfo",
				"actor",
				"node",
				"markPayload",
				"unmarkPayload",
				"resolvePayload",
				"unresolvePayload",
			};
			for (const auto alias : fieldAliases) {
				if (field_array_exceeds_limits(
						json_member(aData, QString::fromUtf8(alias.data(), static_cast<qsizetype>(alias.size()))),
						aLimits.MaximumOtherTypeFields,
						aLimits)) {
					return true;
				}
			}

			constexpr std::array<std::string_view, 4> inputAliases{
				"markInput",
				"unmarkInput",
				"resolveInput",
				"unresolveInput",
			};
			for (const auto alias : inputAliases) {
				const auto& value = json_member(aData, alias);
				if (
					value.is_object()
					&& json_member(json_object(value), "inputFields").is_array()
					&& json_array(json_member(json_object(value), "inputFields"))
							.size()
						> static_cast<std::size_t>(aLimits.MaximumInputFields)) {
					return true;
				}
			}

			for (const auto alias : { "threadSubject", "fileViewedState" }) {
				const auto& value = json_member(aData, alias);
				if (
					value.is_object()
					&& json_member(json_object(value), "enumValues").is_array()
					&& json_array(json_member(json_object(value), "enumValues"))
							.size()
						> static_cast<std::size_t>(aLimits.MaximumEnumValues)) {
					return true;
				}
			}
			for (const auto alias : {
					 "reviewThreadInterfaces",
					 "actorPossibleTypes",
					 "nodePossibleTypes",
				 }) {
				const auto& value = json_member(aData, alias);
				if (!value.is_object()) {
					continue;
				}
				const auto& object  = json_object(value);
				const auto  members = has_member(object, "interfaces")
					? json_member(object, "interfaces")
					: json_member(object, "possibleTypes");
				if (members.is_array()
					&& json_array(members).size()
						> static_cast<std::size_t>(aLimits.MaximumNamedTypes)) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto evaluate_schema(const capability_object&       aData,
										   const capability_probe_limits& aLimits,
										   capability_probe_observation&  aOutput) -> bool
		{
			const auto parseFields = [&aData, &aLimits](const std::string_view aAlias,
														const std::string_view aTypeName,
														const std::size_t      aMaximum,
														selected_schema_type&  aResult) {
				return parse_selected_type(
					json_member(aData, QString::fromUtf8(aAlias.data(), static_cast<qsizetype>(aAlias.size()))),
					aTypeName, selected_type_shape::fields, aMaximum, aLimits, aResult);
			};
			const auto parseInput = [&aData, &aLimits](const std::string_view aAlias,
													   const std::string_view aTypeName,
													   selected_schema_type&  aResult) {
				return parse_selected_type(
					json_member(aData, QString::fromUtf8(aAlias.data(), static_cast<qsizetype>(aAlias.size()))),
					aTypeName, selected_type_shape::input_fields,
					aLimits.MaximumInputFields, aLimits, aResult);
			};
			const auto parseEnum = [&aData, &aLimits](const std::string_view aAlias,
													  const std::string_view aTypeName,
													  selected_schema_type&  aResult) {
				return parse_selected_type(
					json_member(aData, QString::fromUtf8(aAlias.data(), static_cast<qsizetype>(aAlias.size()))),
					aTypeName, selected_type_shape::enum_values,
					aLimits.MaximumEnumValues, aLimits, aResult);
			};
			const auto parseNamed = [&aData, &aLimits](const std::string_view    aAlias,
													   const std::string_view    aTypeName,
													   const selected_type_shape aShape,
													   selected_schema_type&     aResult) {
				return parse_selected_type(
					json_member(aData, QString::fromUtf8(aAlias.data(), static_cast<qsizetype>(aAlias.size()))),
					aTypeName, aShape, aLimits.MaximumNamedTypes, aLimits, aResult);
			};

			selected_schema_type query;
			selected_schema_type mutation;
			const auto&          schemaRoot = json_member(aData, "schemaRoot");
			if (!schemaRoot.is_object()) {
				return false;
			}
			const auto& schemaObject = json_object(schemaRoot);
			if (!exact_keys(schemaObject,
							{
								QStringLiteral("queryType"),
								QStringLiteral("mutationType"),
							})
				|| !parse_selected_type(json_member(schemaObject, "queryType"), { },
										selected_type_shape::fields,
										aLimits.MaximumQueryFields, aLimits, query)
				|| !parse_selected_type(json_member(schemaObject, "mutationType"), { },
										selected_type_shape::fields,
										aLimits.MaximumMutationFields, aLimits, mutation)) {
				return false;
			}

			selected_schema_type repository;
			selected_schema_type pullRequest;
			selected_schema_type threadConnection;
			selected_schema_type reviewThread;
			selected_schema_type commentConnection;
			selected_schema_type reviewComment;
			selected_schema_type changedFileConnection;
			selected_schema_type changedFile;
			selected_schema_type pageInfo;
			selected_schema_type actor;
			selected_schema_type node;
			selected_schema_type reviewThreadInterfaces;
			selected_schema_type actorPossibleTypes;
			selected_schema_type nodePossibleTypes;
			selected_schema_type threadSubject;
			selected_schema_type diffSide;
			selected_schema_type viewedState;
			selected_schema_type markInput;
			selected_schema_type markPayload;
			selected_schema_type unmarkInput;
			selected_schema_type unmarkPayload;
			selected_schema_type resolveInput;
			selected_schema_type resolvePayload;
			selected_schema_type unresolveInput;
			selected_schema_type unresolvePayload;

			if (!parseFields("repository", "Repository", aLimits.MaximumRepositoryFields,
							 repository)
				|| !parseFields("pullRequest", "PullRequest",
								aLimits.MaximumPullRequestFields, pullRequest)
				|| !parseFields("threadConnection", "PullRequestReviewThreadConnection",
								aLimits.MaximumOtherTypeFields, threadConnection)
				|| !parseFields("reviewThread", "PullRequestReviewThread",
								aLimits.MaximumOtherTypeFields, reviewThread)
				|| !parseFields("commentConnection",
								"PullRequestReviewCommentConnection",
								aLimits.MaximumOtherTypeFields, commentConnection)
				|| !parseFields("reviewComment", "PullRequestReviewComment",
								aLimits.MaximumOtherTypeFields, reviewComment)
				|| !parseFields("changedFileConnection",
								"PullRequestChangedFileConnection",
								aLimits.MaximumOtherTypeFields, changedFileConnection)
				|| !parseFields("changedFile", "PullRequestChangedFile",
								aLimits.MaximumOtherTypeFields, changedFile)
				|| !parseFields("pageInfo", "PageInfo", aLimits.MaximumOtherTypeFields,
								pageInfo)
				|| !parseFields("actor", "Actor", aLimits.MaximumOtherTypeFields, actor)
				|| !parseFields("node", "Node", aLimits.MaximumOtherTypeFields, node)
				|| !parseNamed("reviewThreadInterfaces", "PullRequestReviewThread",
							   selected_type_shape::interfaces,
							   reviewThreadInterfaces)
				|| !parseNamed("actorPossibleTypes", "Actor",
							   selected_type_shape::possible_types,
							   actorPossibleTypes)
				|| !parseNamed("nodePossibleTypes", "Node",
							   selected_type_shape::possible_types, nodePossibleTypes)
				|| !parseEnum("threadSubject", "PullRequestReviewThreadSubjectType",
							  threadSubject)
				|| !parseEnum("diffSide", "DiffSide", diffSide)
				|| !parseEnum("fileViewedState", "FileViewedState", viewedState)
				|| !parseInput("markInput", "MarkFileAsViewedInput", markInput)
				|| !parseFields("markPayload", "MarkFileAsViewedPayload",
								aLimits.MaximumOtherTypeFields, markPayload)
				|| !parseInput("unmarkInput", "UnmarkFileAsViewedInput", unmarkInput)
				|| !parseFields("unmarkPayload", "UnmarkFileAsViewedPayload",
								aLimits.MaximumOtherTypeFields, unmarkPayload)
				|| !parseInput("resolveInput", "ResolveReviewThreadInput",
							   resolveInput)
				|| !parseFields("resolvePayload", "ResolveReviewThreadPayload",
								aLimits.MaximumOtherTypeFields, resolvePayload)
				|| !parseInput("unresolveInput", "UnresolveReviewThreadInput",
							   unresolveInput)
				|| !parseFields("unresolvePayload", "UnresolveReviewThreadPayload",
								aLimits.MaximumOtherTypeFields, unresolvePayload)) {
				return false;
			}

			const argument_requirement after{
				.Name = "after",
				.Type = scalar("String"),
			};
			const argument_requirement first{
				.Name = "first",
				.Type = scalar("Int"),
			};
			const argument_requirement id{
				.Name = "id",
				.Type = scalar("ID", true),
			};
			const argument_requirement name{
				.Name = "name",
				.Type = scalar("String", true),
			};
			const argument_requirement number{
				.Name = "number",
				.Type = scalar("Int", true),
			};
			const argument_requirement owner{
				.Name = "owner",
				.Type = scalar("String", true),
			};

			const auto repositoryPullRequestRead = combined_fact(
				field_requirement(query, "OBJECT", "repository",
								  object_type("Repository"), { name, owner }),
				field_requirement(repository, "OBJECT", "id", scalar("ID", true)),
				field_requirement(repository, "OBJECT", "pullRequest",
								  object_type("PullRequest"), { number }),
				field_requirement(pullRequest, "OBJECT", "id", scalar("ID", true)),
				field_requirement(pullRequest, "OBJECT", "headRefOid",
								  scalar("GitObjectID", true)));

			const auto reviewRead = combined_fact(
				repositoryPullRequestRead,
				field_requirement(query, "OBJECT", "node", interface_type("Node"),
								  { id }),
				named_type_requirement(reviewThreadInterfaces, "OBJECT", "INTERFACE",
									   "Node"),
				named_type_subset_requirement(actorPossibleTypes,
											  nodePossibleTypes),
				field_requirement(
					pullRequest, "OBJECT", "reviewThreads",
					object_type("PullRequestReviewThreadConnection", true),
					{ after, first }),
				field_requirement(threadConnection, "OBJECT", "nodes",
								  list_of_objects("PullRequestReviewThread")),
				field_requirement(threadConnection, "OBJECT", "pageInfo",
								  object_type("PageInfo", true)),
				field_requirement(threadConnection, "OBJECT", "totalCount",
								  scalar("Int", true)),
				field_requirement(reviewThread, "OBJECT", "id", scalar("ID", true)),
				field_requirement(reviewThread, "OBJECT", "isResolved",
								  scalar("Boolean", true)),
				field_requirement(reviewThread, "OBJECT", "isOutdated",
								  scalar("Boolean", true)),
				field_requirement(
					reviewThread, "OBJECT", "subjectType",
					enum_type("PullRequestReviewThreadSubjectType", true)),
				field_requirement(reviewThread, "OBJECT", "path",
								  scalar("String", true)),
				field_requirement(reviewThread, "OBJECT", "diffSide",
								  enum_type("DiffSide", true)),
				enum_requirement(diffSide, { "LEFT", "RIGHT" }),
				field_requirement(reviewThread, "OBJECT", "line", scalar("Int")),
				field_requirement(reviewThread, "OBJECT", "originalLine",
								  scalar("Int")),
				field_requirement(
					reviewThread, "OBJECT", "comments",
					object_type("PullRequestReviewCommentConnection", true),
					{ after, first }),
				field_requirement(commentConnection, "OBJECT", "nodes",
								  list_of_objects("PullRequestReviewComment")),
				field_requirement(commentConnection, "OBJECT", "pageInfo",
								  object_type("PageInfo", true)),
				field_requirement(commentConnection, "OBJECT", "totalCount",
								  scalar("Int", true)),
				field_requirement(reviewComment, "OBJECT", "id", scalar("ID", true)),
				field_requirement(reviewComment, "OBJECT", "bodyText",
								  scalar("String", true)),
				field_requirement(reviewComment, "OBJECT", "path",
								  scalar("String", true)),
				field_requirement(reviewComment, "OBJECT", "line", scalar("Int")),
				field_requirement(reviewComment, "OBJECT", "originalLine",
								  scalar("Int")),
				field_requirement(reviewComment, "OBJECT", "outdated",
								  scalar("Boolean", true)),
				field_requirement(reviewComment, "OBJECT", "author",
								  interface_type("Actor")),
				field_requirement(reviewComment, "OBJECT", "replyTo",
								  object_type("PullRequestReviewComment")),
				field_requirement(pageInfo, "OBJECT", "hasNextPage",
								  scalar("Boolean", true)),
				field_requirement(pageInfo, "OBJECT", "endCursor", scalar("String")),
				field_requirement(actor, "INTERFACE", "login", scalar("String", true)),
				field_requirement(node, "INTERFACE", "id", scalar("ID", true)),
				enum_requirement(threadSubject, { "FILE", "LINE" }));

			const auto viewedRead = combined_fact(
				repositoryPullRequestRead,
				field_requirement(pullRequest, "OBJECT", "files",
								  object_type("PullRequestChangedFileConnection"),
								  { after, first }),
				field_requirement(changedFileConnection, "OBJECT", "nodes",
								  list_of_objects("PullRequestChangedFile")),
				field_requirement(changedFileConnection, "OBJECT", "pageInfo",
								  object_type("PageInfo", true)),
				field_requirement(changedFileConnection, "OBJECT", "totalCount",
								  scalar("Int", true)),
				field_requirement(
					changedFile,
					"OBJECT",
					"path",
					scalar("String", true)),
				field_requirement(
					changedFile,
					"OBJECT",
					"viewerViewedState",
					enum_type("FileViewedState", true)),
				enum_requirement(
					viewedState,
					{ "DISMISSED", "UNVIEWED", "VIEWED" }));

			const argument_requirement clientMutationId{
				.Name = "clientMutationId",
				.Type = scalar("String"),
			};
			const argument_requirement path{
				.Name = "path",
				.Type = scalar("String", true),
			};
			const argument_requirement pullRequestId{
				.Name = "pullRequestId",
				.Type = scalar("ID", true),
			};
			const argument_requirement threadId{
				.Name = "threadId",
				.Type = scalar("ID", true),
			};

			const auto mark = combined_fact(
				field_requirement(
					mutation,
					"OBJECT",
					"markFileAsViewed",
					object_type("MarkFileAsViewedPayload"),
					{ {
						.Name = "input",
						.Type = input_type("MarkFileAsViewedInput"),
					} }),
				input_requirement(
					markInput,
					{ clientMutationId, path, pullRequestId }),
				field_requirement(
					markPayload,
					"OBJECT",
					"clientMutationId",
					scalar("String")),
				field_requirement(
					markPayload,
					"OBJECT",
					"pullRequest",
					object_type("PullRequest")));

			const auto unmark = combined_fact(
				field_requirement(
					mutation,
					"OBJECT",
					"unmarkFileAsViewed",
					object_type("UnmarkFileAsViewedPayload"),
					{ {
						.Name = "input",
						.Type = input_type("UnmarkFileAsViewedInput"),
					} }),
				input_requirement(
					unmarkInput,
					{ clientMutationId, path, pullRequestId }),
				field_requirement(
					unmarkPayload,
					"OBJECT",
					"clientMutationId",
					scalar("String")),
				field_requirement(
					unmarkPayload,
					"OBJECT",
					"pullRequest",
					object_type("PullRequest")));

			const auto resolvePermission = field_requirement(
				reviewThread,
				"OBJECT",
				"viewerCanResolve",
				scalar("Boolean", true));
			const auto unresolvePermission = field_requirement(
				reviewThread,
				"OBJECT",
				"viewerCanUnresolve",
				scalar("Boolean", true));

			const auto resolve = combined_fact(
				field_requirement(
					mutation,
					"OBJECT",
					"resolveReviewThread",
					object_type("ResolveReviewThreadPayload"),
					{ {
						.Name = "input",
						.Type = input_type("ResolveReviewThreadInput"),
					} }),
				input_requirement(
					resolveInput,
					{ clientMutationId, threadId }),
				field_requirement(
					resolvePayload,
					"OBJECT",
					"clientMutationId",
					scalar("String")),
				field_requirement(
					resolvePayload,
					"OBJECT",
					"thread",
					object_type("PullRequestReviewThread")));

			const auto unresolve = combined_fact(
				field_requirement(
					mutation,
					"OBJECT",
					"unresolveReviewThread",
					object_type("UnresolveReviewThreadPayload"),
					{ {
						.Name = "input",
						.Type = input_type("UnresolveReviewThreadInput"),
					} }),
				input_requirement(
					unresolveInput,
					{ clientMutationId, threadId }),
				field_requirement(
					unresolvePayload,
					"OBJECT",
					"clientMutationId",
					scalar("String")),
				field_requirement(
					unresolvePayload,
					"OBJECT",
					"thread",
					object_type("PullRequestReviewThread")));

			aOutput.Facts[fact_index(
				capability_schema_fact::review_threads_read)]     = reviewRead;
			aOutput.Facts[fact_index(
				capability_schema_fact::viewed_file_state_read)]  = viewedRead;
			aOutput.Facts[fact_index(
				capability_schema_fact::mark_file_as_viewed)]     = mark;
			aOutput.Facts[fact_index(
				capability_schema_fact::unmark_file_as_viewed)]   = unmark;
			aOutput.Facts[fact_index(
				capability_schema_fact::resolve_permission)]      = resolvePermission;
			aOutput.Facts[fact_index(
				capability_schema_fact::resolve_review_thread)]   = resolve;
			aOutput.Facts[fact_index(
				capability_schema_fact::unresolve_permission)]    = unresolvePermission;
			aOutput.Facts[fact_index(
				capability_schema_fact::unresolve_review_thread)] = unresolve;

			aOutput.Capabilities = {
				.ReviewThreads       = capability_state({ reviewRead }),
				.ViewedFileMutations = capability_state({ viewedRead, mark, unmark }),
				.ResolveThread       = capability_state(
					{ reviewRead, resolvePermission, resolve }),
				.UnresolveThread = capability_state(
					{ reviewRead, unresolvePermission, unresolve }),
			};
			aOutput.Unavailability = capability_probe_unavailability::none;
			aOutput.Complete       = true;
			return true;
		}

		[[nodiscard]] auto ascii_fold(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z'
				? static_cast<char>(aCharacter - 'A' + 'a')
				: aCharacter;
		}

		[[nodiscard]] auto ascii_equal_fold(
			const std::string_view aLeft,
			const std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size()
				&& std::ranges::equal(
					   aLeft,
					   aRight,
					   [](const char aLeftValue, const char aRightValue) {
						   return ascii_fold(aLeftValue) == ascii_fold(aRightValue);
					   });
		}

		[[nodiscard]] auto trim_http_ows(std::string_view aValue) noexcept
			-> std::string_view
		{
			while (
				!aValue.empty()
				&& (aValue.front() == ' ' || aValue.front() == '\t')) {
				aValue.remove_prefix(1U);
			}
			while (
				!aValue.empty()
				&& (aValue.back() == ' ' || aValue.back() == '\t')) {
				aValue.remove_suffix(1U);
			}
			return aValue;
		}

		[[nodiscard]] auto decimal_header_value(std::string_view aValue) noexcept
			-> std::optional<std::uint64_t>
		{
			constexpr std::size_t maximumDecimalHeaderBytes = 32U;
			if (aValue.size() > maximumDecimalHeaderBytes) {
				return std::nullopt;
			}
			aValue = trim_http_ows(aValue);
			if (aValue.empty()) {
				return std::nullopt;
			}

			std::uint64_t result = 0U;
			for (const auto character : aValue) {
				if (character < '0' || character > '9') {
					return std::nullopt;
				}
				const auto digit = static_cast<std::uint64_t>(character - '0');
				if (
					result
					> (std::numeric_limits<std::uint64_t>::max() - digit)
						/ 10U) {
					return std::nullopt;
				}
				result = result * 10U + digit;
			}
			return result;
		}

		enum class rate_limit_header_kind : std::uint8_t {
			limit,
			remaining,
			reset,
			resource,
			retry_after,
			count,
		};

		struct rate_limit_response_facts
		{
			std::optional<std::uint64_t> Limit;
			std::optional<std::uint64_t> Remaining;
			std::optional<std::uint64_t> ResetUnixSeconds;
			std::optional<bool>          GraphqlResource;
			std::optional<std::uint64_t> RetryAfterSeconds;

			[[nodiscard]] auto signals_throttling() const noexcept -> bool
			{
				return (Remaining.has_value() && *Remaining == 0U)
					|| RetryAfterSeconds.has_value();
			}
		};

		[[nodiscard]] auto rate_limit_header(const std::string_view aName) noexcept
			-> std::optional<rate_limit_header_kind>
		{
			if (ascii_equal_fold(aName, "x-ratelimit-limit")) {
				return rate_limit_header_kind::limit;
			}
			if (ascii_equal_fold(aName, "x-ratelimit-remaining")) {
				return rate_limit_header_kind::remaining;
			}
			if (ascii_equal_fold(aName, "x-ratelimit-reset")) {
				return rate_limit_header_kind::reset;
			}
			if (ascii_equal_fold(aName, "x-ratelimit-resource")) {
				return rate_limit_header_kind::resource;
			}
			if (ascii_equal_fold(aName, "retry-after")) {
				return rate_limit_header_kind::retry_after;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto parse_rate_limit_response_facts(
			const capability_probe_response& aResponse) noexcept
			-> std::optional<rate_limit_response_facts>
		{
			constexpr std::size_t maximumHeaderNameBytes = 32U;
			if (aResponse.RateLimitHeaderCount > aResponse.RateLimitHeaders.size()) {
				return std::nullopt;
			}

			auto facts = rate_limit_response_facts{ };
			auto seen  = std::array<bool,
									static_cast<std::size_t>(rate_limit_header_kind::count)>{ };
			for (std::size_t index = 0U; index < aResponse.RateLimitHeaders.size();
				 ++index) {
				const auto& header = aResponse.RateLimitHeaders[index];
				if (index >= aResponse.RateLimitHeaderCount) {
					if (!header.Name.empty() || !header.Value.empty()) {
						return std::nullopt;
					}
					continue;
				}
				if (header.Name.empty()
					|| header.Name.size() > maximumHeaderNameBytes) {
					return std::nullopt;
				}
				const auto kind = rate_limit_header(header.Name);
				if (!kind.has_value()) {
					return std::nullopt;
				}
				const auto kindIndex = static_cast<std::size_t>(*kind);
				if (seen[kindIndex]) {
					return std::nullopt;
				}
				seen[kindIndex] = true;
				switch (*kind) {
				case rate_limit_header_kind::limit: {
					const auto parsed = decimal_header_value(header.Value);
					if (!parsed.has_value()) {
						return std::nullopt;
					}
					facts.Limit = parsed;
					break;
				}
				case rate_limit_header_kind::remaining: {
					const auto parsed = decimal_header_value(header.Value);
					if (!parsed.has_value()) {
						return std::nullopt;
					}
					facts.Remaining = parsed;
					break;
				}
				case rate_limit_header_kind::reset: {
					const auto parsed = decimal_header_value(header.Value);
					if (!parsed.has_value()) {
						return std::nullopt;
					}
					facts.ResetUnixSeconds = parsed;
					break;
				}
				case rate_limit_header_kind::resource:
					if (header.Value != "graphql") {
						return std::nullopt;
					}
					facts.GraphqlResource = true;
					break;
				case rate_limit_header_kind::retry_after: {
					const auto parsed = decimal_header_value(header.Value);
					if (!parsed.has_value()) {
						return std::nullopt;
					}
					facts.RetryAfterSeconds = parsed;
					break;
				}
				case rate_limit_header_kind::count:
					return std::nullopt;
				}
			}
			const auto tupleCount = static_cast<std::size_t>(facts.Limit.has_value())
				+ static_cast<std::size_t>(facts.Remaining.has_value())
				+ static_cast<std::size_t>(facts.ResetUnixSeconds.has_value())
				+ static_cast<std::size_t>(facts.GraphqlResource.has_value());
			if ((tupleCount != 0U && tupleCount != 4U)
				|| (facts.Limit.has_value() && facts.Remaining.has_value()
					&& *facts.Remaining > *facts.Limit)) {
				return std::nullopt;
			}
			return facts;
		}

		[[nodiscard]] auto graphql_error_rate_limited(
			const capability_json& aErrors) -> bool
		{
			if (!aErrors.is_array()) {
				return false;
			}
			for (const auto& value : json_array(aErrors)) {
				if (!value.is_object()) {
					continue;
				}
				const auto& error = json_object(value);
				if (
					json_member(error, "type").is_string()
					&& json_text(json_member(error, "type"))
						== QStringLiteral("RATE_LIMITED")) {
					return true;
				}
				const auto& extensions = json_member(error, "extensions");
				if (!extensions.is_object()) {
					continue;
				}
				const auto& extension = json_object(extensions);
				for (const auto key : { "type", "code" }) {
					if (
						json_member(extension, key).is_string()
						&& json_text(json_member(extension, key))
							== QStringLiteral("RATE_LIMITED")) {
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] auto
		graphql_error_body_rate_limited(const capability_probe_response& aResponse,
										const capability_probe_limits&   aLimits) noexcept
			-> bool
		{
			if (aResponse.Body.empty()
				|| aResponse.Body.size() > aLimits.MaximumResponseBytes
				|| !json_media_type(aResponse.ContentType)) {
				return false;
			}
			try {
				if (json_structural_preflight{ aResponse.Body, aLimits }.run()
					!= json_preflight_result::valid) {
					return false;
				}
				const auto document = parse_json_object(aResponse.Body);
				return document && graphql_error_rate_limited(json_member(*document, "errors"));
			} catch (...) {
				return false;
			}
		}

		[[nodiscard]] auto status_failure(const int aStatus) noexcept
			-> capability_probe_unavailability
		{
			switch (aStatus) {
			case 401:
				return capability_probe_unavailability::authentication;
			case 403:
				return capability_probe_unavailability::permission;
			case 404:
				return capability_probe_unavailability::api_version;
			case 429:
				return capability_probe_unavailability::rate_limit;
			default:
				return capability_probe_unavailability::invalid_status;
			}
		}
	} // namespace

	auto valid(const capability_probe_observation& aObservation) noexcept -> bool
	{
		if (
			!valid(aObservation.Unavailability)
			|| !std::ranges::all_of(
				aObservation.Facts,
				[](const auto aState) { return valid(aState); })) {
			return false;
		}

		if (!aObservation.Complete) {
			return aObservation.Unavailability
				!= capability_probe_unavailability::none
				&& aObservation.Capabilities == core::github_capabilities{ }
			&& std::ranges::all_of(
					   aObservation.Facts,
					   [](const auto aState) {
						   return aState
							   == capability_schema_fact_state::unknown;
					   });
		}
		if (
			aObservation.Unavailability
				!= capability_probe_unavailability::none
			|| std::ranges::any_of(
				aObservation.Facts,
				[](const auto aState) {
					return aState == capability_schema_fact_state::unknown;
				})) {
			return false;
		}

		const auto review              = aObservation.Facts[fact_index(
			capability_schema_fact::review_threads_read)];
		const auto viewed              = aObservation.Facts[fact_index(
			capability_schema_fact::viewed_file_state_read)];
		const auto mark                = aObservation.Facts[fact_index(
			capability_schema_fact::mark_file_as_viewed)];
		const auto unmark              = aObservation.Facts[fact_index(
			capability_schema_fact::unmark_file_as_viewed)];
		const auto resolvePermission   = aObservation.Facts[fact_index(
			capability_schema_fact::resolve_permission)];
		const auto resolve             = aObservation.Facts[fact_index(
			capability_schema_fact::resolve_review_thread)];
		const auto unresolvePermission = aObservation.Facts[fact_index(
			capability_schema_fact::unresolve_permission)];
		const auto unresolve           = aObservation.Facts[fact_index(
			capability_schema_fact::unresolve_review_thread)];
		return aObservation.Capabilities
			== core::github_capabilities{
				   .ReviewThreads       = capability_state({ review }),
				   .ViewedFileMutations = capability_state({ viewed, mark, unmark }),
				   .ResolveThread       = capability_state(
					   { review, resolvePermission, resolve }),
				   .UnresolveThread = capability_state(
					   { review, unresolvePermission, unresolve }),
			   };
	}

	auto capability_probe_graphql_document(const std::uint8_t aPart) noexcept -> std::string_view
	{
		return aPart < graphql_documents.size() ? graphql_documents[aPart] : std::string_view{ };
	}

	auto capability_probe_part_aliases(const std::uint8_t aPart) noexcept -> std::span<const std::string_view>
	{
		if (aPart >= part_aliases.size()) {
			return { };
		}
		const auto& aliases = part_aliases[aPart];
		return { aliases.begin(), std::find(aliases.begin(), aliases.end(), std::string_view{ }) };
	}

	auto capability_probe_document_is_query_only(
		const std::string_view aDocument) noexcept -> bool
	{
		return inspect_graphql_operation_policy(aDocument)
			== graphql_operation_policy_result::query_only;
	}

	auto compile_capability_probe_request(
		const validated_github_host_profile& aProfile,
		const capability_probe_limits& aLimits, const std::uint8_t aPart)
		-> capability_probe_request_result
	{
		const auto contract        = aProfile.server_contract();
		const auto graphqlDocument = capability_probe_graphql_document(aPart);
		if (graphqlDocument.empty()) {
			return std::unexpected{ capability_probe_unavailability::invalid_contract };
		}
		if (!application::valid(contract)) {
			return std::unexpected{
				capability_probe_unavailability::invalid_contract
			};
		}
		if (!valid_limits(aLimits)) {
			return std::unexpected{
				capability_probe_unavailability::invalid_limits
			};
		}

		try {
			auto bytes = glz::write<capability_json_options{ }>(capability_query_json{ graphqlDocument });
			if (!bytes || bytes->size() > aLimits.MaximumRequestBytes) {
				return std::unexpected{ capability_probe_unavailability::response_limit };
			}

			return capability_probe_request{
				.Part            = aPart,
				.Profile         = aProfile.profile(),
				.Host            = aProfile.host(),
				.ProfileRevision = aProfile.revision(),
				.Contract        = contract,
				.Method          = capability_probe_http_method::post,
				.Authority       = std::string{ aProfile.rest_authority() },
				.Root            = std::string{ aProfile.graphql_root() },
				.Path            = std::string{ aProfile.graphql_route() },
				.Headers         = {
					capability_probe_header{
						.Name  = "Accept",
						.Value = "application/json",
					},
					capability_probe_header{
						.Name  = "Content-Type",
						.Value = "application/json",
					},
					capability_probe_header{
						.Name  = "User-Agent",
						.Value = "BetterPullReview/0.1",
					},
				},
				.Body = std::move(*bytes),
			};
		} catch (const std::bad_alloc&) {
			return std::unexpected{
				capability_probe_unavailability::allocation_failure
			};
		}
	}

	namespace {
		// Only symbols occurring in the compiled public introspection documents
		// have provenance here. Identifier shape alone proves nothing.
		auto public_probe_symbol(const QString& aSymbol) -> QString
		{
			const auto symbol     = aSymbol.toStdString();
			const auto identifier = [](const char aByte) {
				return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z')
					|| (aByte >= '0' && aByte <= '9') || aByte == '_';
			};
			for (const auto document : graphql_documents) {
				auto position = document.find(symbol);
				while (!symbol.empty() && position != std::string_view::npos) {
					const auto end = position + symbol.size();
					if ((position == 0U || !identifier(document[position - 1U]))
						&& (end == document.size() || !identifier(document[end]))) {
						return QString::fromUtf8(document.substr(position, symbol.size()));
					}
					position = document.find(symbol, end);
				}
			}
			return QStringLiteral("[omitted:unclassified-symbol]");
		}

		auto classified_capability_detail(const QString& aText, const bool aMessage) -> QString
		{
			if (!aMessage) {
				if (graphql_error_code_detail(aText) != application::github_capability_diagnostic::graphql_errors
					|| aText == QStringLiteral("RATE_LIMITED")) {
					return aText;
				}
				return QStringLiteral("[omitted:unclassified-code]");
			}
			// Reconstruct a closed explanation, never concatenate free text. An
			// unfamiliar code can still explain a schema rejection; unmatched
			// messages and trailing context have an explicit omission reason.
			const auto field = QRegularExpression{ QStringLiteral(R"(^Unknown schema field ['"]?([_A-Za-z][_0-9A-Za-z]*)['"]?(?: is unavailable\.)?(.*)$)") }.match(aText);
			if (field.hasMatch()) {
				return QStringLiteral("Unknown schema field ") + public_probe_symbol(field.captured(1))
					+ (field.captured(2).isEmpty() ? QString{ } : QStringLiteral(" [omitted:unclassified-content]"));
			}
			const auto absent = QRegularExpression{ QStringLiteral(R"(^Field '([_A-Za-z][_0-9A-Za-z]*)' doesn't exist on type '([_A-Za-z][_0-9A-Za-z]*)'(.*)$)") }.match(aText);
			if (absent.hasMatch()) {
				return QStringLiteral("Field ") + public_probe_symbol(absent.captured(1))
					+ QStringLiteral(" does not exist on type ") + public_probe_symbol(absent.captured(2))
					+ (absent.captured(3).isEmpty() ? QString{ } : QStringLiteral(" [omitted:unclassified-content]"));
			}
			const auto repeated = QRegularExpression{ QStringLiteral(R"(^Field '([_A-Za-z][_0-9A-Za-z]*)' has a max repetition count of [0-9]+.*$)") }.match(aText);
			if (repeated.hasMatch()) {
				return QStringLiteral("Field ") + public_probe_symbol(repeated.captured(1))
					+ QStringLiteral(" exceeds a service repetition limit [omitted:unclassified-content]");
			}
			return QStringLiteral("[omitted:unclassified-content]");
		}
	} // namespace

	auto sanitize_capability_error(const std::string_view aBody, const std::span<const std::byte> aCredential) noexcept -> application::diagnostic_summary
	{
		using state = application::diagnostic_summary_state;
		application::diagnostic_summary result;
		try {
			const capability_probe_limits limits;
			if (aBody.size() > limits.MaximumResponseBytes || aCredential.empty() || aCredential.size() > 8192U) {
				result.State = state::omitted_limit;
				return result;
			}
			if (json_structural_preflight{ aBody, limits }.run() != json_preflight_result::valid) {
				result.State = state::omitted_invalid;
				return result;
			}
			const auto document = parse_json_object(aBody);
			if (!document) {
				result.State = state::omitted_invalid;
				return result;
			}
			const auto& envelope = *document;
			if (!has_member(envelope, "errors")) {
				return result;
			}
			const auto& errors = json_member(envelope, "errors");
			if (!errors.is_array() || json_array(errors).empty() || json_array(errors).size() > 8) {
				result.State = state::omitted_invalid;
				return result;
			}
			const QByteArray credential{ reinterpret_cast<const char*>(aCredential.data()), static_cast<qsizetype>(aCredential.size()) };
			auto             percent = QByteArray{ };
			for (const auto character : credential) {
				percent += '%';
				percent += QByteArray(1, character).toHex();
			}
			const auto encodedJson = glz::write<capability_json_options{ }>(QString::fromUtf8(credential).toStdString());
			if (!encodedJson || encodedJson->size() < 2U) {
				result.State = state::omitted_failure;
				return result;
			}
			const std::array representations{
				QString::fromUtf8(credential), QString::fromLatin1(credential.toBase64()),
				QString::fromLatin1(credential.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)),
				QString::fromLatin1(credential.toHex()), QString::fromLatin1(percent), QString::fromLatin1(credential.toPercentEncoding()),
				QString::fromUtf8(std::string_view{ *encodedJson }.substr(1U, encodedJson->size() - 2U))
			};
			QString    summary;
			const auto append = [&](const QString& aName, const capability_json& aValue, const bool aPresent) -> bool {
				if (!aPresent) {
					return true;
				}
				if (!aValue.is_string() || json_text(aValue).size() > 8192) {
					return false;
				}
				auto text = json_text(aValue);
				// Remove complete reflections before any truncation. JSON decoding above
				// handles wire escapes; common nested/encoded representations are removed too.
				for (const auto& representation : representations) {
					if (!representation.isEmpty()) {
						text.replace(representation, QStringLiteral("[redacted]"), Qt::CaseInsensitive);
					}
				}
				text.replace(QRegularExpression{ QStringLiteral(R"((?i)(?:authorization|proxy-authorization|cookie|set-cookie|password|access_token|refresh_token|client_secret)\s*[:=][^\r\n]*)") }, QStringLiteral("[redacted header]"));
				text.replace(QRegularExpression{ QStringLiteral(R"((?i)(?:https?://|www\.)[^\s<>"']+)") }, QStringLiteral("[redacted URL]"));
				text.replace(QRegularExpression{ QStringLiteral(R"((?i)\b(?:bearer|basic|token)\s+[^\s,;"']+)") }, QStringLiteral("[redacted credential]"));
				text.replace(QRegularExpression{ QStringLiteral(R"((?i)\b(?:gh[pousr]_[a-z0-9_]+|github_pat_[a-z0-9_]+|[a-z0-9_+/-]{32,}={0,2}|eyJ[a-z0-9_.-]+))") }, QStringLiteral("[redacted token]"));
				text.replace(QRegularExpression{ QStringLiteral("[\\x00-\\x1f\\x7f-\\x9f\\x{2028}\\x{2029}\\x{202a}-\\x{202e}\\x{2066}-\\x{2069}]") }, QStringLiteral(" "));
				summary += aName + QStringLiteral("=\"")
					+ classified_capability_detail(text, aName == QStringLiteral("message")) + QStringLiteral("\" ");
				return true;
			};
			for (const auto& value : json_array(errors)) {
				if (!value.is_object()) {
					result.State = state::omitted_invalid;
					return result;
				}
				const auto& entry      = json_object(value);
				const auto& extensions = json_member(entry, "extensions");
				if ((has_member(entry, "extensions") && !extensions.is_object())
					|| !append(QStringLiteral("message"), json_member(entry, "message"), has_member(entry, "message"))
					|| !append(QStringLiteral("type"), json_member(entry, "type"), has_member(entry, "type"))
					|| !append(QStringLiteral("type"), json_member(json_object(extensions), "type"), has_member(json_object(extensions), "type"))
					|| !append(QStringLiteral("code"), json_member(json_object(extensions), "code"), has_member(json_object(extensions), "code"))) {
					result.State = state::omitted_invalid;
					return result;
				}
			}
			auto bytes = summary.toUtf8();
			if (bytes.size() > static_cast<qsizetype>(result.Text.size())) {
				result.State = state::omitted_limit;
				return result;
			}
			std::copy(bytes.begin(), bytes.end(), result.Text.begin());
			result.Size  = static_cast<std::uint16_t>(bytes.size());
			result.State = state::sanitized;
			return result;
		} catch (...) {
			result       = { };
			result.State = state::omitted_failure;
			return result;
		}
	}

	namespace {
		auto data_unavailable(capability_probe_unavailability aReason, application::github_capability_diagnostic aDetail) -> std::expected<capability_object, capability_probe_observation> { return std::unexpected{ unavailable(aReason, aDetail) }; }
		auto checked_capability_data(const capability_probe_transport_result& aResponse, const capability_probe_limits& aLimits) -> std::expected<capability_object, capability_probe_observation>
		{
			if (!valid_limits(aLimits)) {
				return data_unavailable(
					capability_probe_unavailability::invalid_limits, application::github_capability_diagnostic::normalizer_invalid_limits);
			}
			if (!aResponse.has_value()) {
				const auto reason = aResponse.error();
				return data_unavailable(
					!valid(reason)
							|| reason == capability_probe_unavailability::none
						? capability_probe_unavailability::malformed_response
						: reason,
					application::github_capability_diagnostic::normalizer_transport_unavailable);
			}

			const auto& value = *aResponse;
			if (!value.Authenticated) {
				return data_unavailable(
					capability_probe_unavailability::authentication, application::github_capability_diagnostic::normalizer_transport_unavailable);
			}
			const auto rateLimit = parse_rate_limit_response_facts(value);
			if (!rateLimit.has_value()) {
				return data_unavailable(
					capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::normalizer_rate_metadata);
			}
			if (value.Body.size() > aLimits.MaximumResponseBytes) {
				return data_unavailable(capability_probe_unavailability::response_limit, application::github_capability_diagnostic::normalizer_body_limit);
			}
			if (value.Status != 200) {
				if (value.Status == 403
					&& (rateLimit->signals_throttling()
						|| graphql_error_body_rate_limited(value, aLimits))) {
					return data_unavailable(capability_probe_unavailability::rate_limit, application::github_capability_diagnostic::normalizer_transport_unavailable);
				}
				return data_unavailable(status_failure(value.Status), application::github_capability_diagnostic::normalizer_transport_unavailable);
			}
			if (!json_media_type(value.ContentType)) {
				return data_unavailable(
					capability_probe_unavailability::invalid_content_type, application::github_capability_diagnostic::transport_invalid_content_type);
			}
			if (value.Body.empty()) {
				return data_unavailable(capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::json_shape);
			}

			try {
				const auto preflight = json_structural_preflight{ value.Body, aLimits }.run();
				if (preflight != json_preflight_result::valid) {
					return data_unavailable(
						preflight == json_preflight_result::structural_limit
							? capability_probe_unavailability::response_limit
							: capability_probe_unavailability::
								  malformed_response,
						preflight == json_preflight_result::structural_limit ? application::github_capability_diagnostic::json_limits : application::github_capability_diagnostic::json_shape);
				}

				const auto document = parse_json_object(value.Body);
				if (!document) {
					return data_unavailable(capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::json_shape);
				}
				const auto& envelope = *document;
				if (has_member(envelope, "errors")) {
					if (
						rateLimit->signals_throttling()
						|| graphql_error_rate_limited(
							json_member(envelope, "errors"))) {
						return data_unavailable(
							capability_probe_unavailability::rate_limit, application::github_capability_diagnostic::graphql_errors);
					}
					return data_unavailable(
						capability_probe_unavailability::partial_response, graphql_error_detail(json_member(envelope, "errors")));
				}
				if (
					!has_member(envelope, "data")
					|| !json_member(envelope, "data").is_object()) {
					return data_unavailable(
						capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_data);
				}
				for (auto iterator = envelope.begin();
					 iterator != envelope.end();
					 ++iterator) {
					if (
						iterator->first != "data"
						&& iterator->first != "extensions") {
						return data_unavailable(
							capability_probe_unavailability::
								malformed_response,
							application::github_capability_diagnostic::json_shape);
					}
				}

				return json_object(json_member(envelope, "data"));
			} catch (const std::bad_alloc&) {
				return data_unavailable(
					capability_probe_unavailability::allocation_failure, application::github_capability_diagnostic::normalization_allocation);
			} catch (...) {
				return data_unavailable(
					capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::normalization_unexpected);
			}
		}
	}
	auto normalize_capability_probe(const capability_probe_transport_result& aResponse, const capability_probe_limits& aLimits) noexcept -> capability_probe_observation
	{
		try {
			const auto checked = checked_capability_data(aResponse, aLimits);
			if (!checked) {
				return checked.error();
			}
			const auto& data = *checked;

			if (!response_has_exact_aliases(data)) {
				return unavailable(
					capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_aliases);
			}
			if (schema_specific_limits_exceeded(data, aLimits)) {
				return unavailable(
					capability_probe_unavailability::response_limit, application::github_capability_diagnostic::schema_limits);
			}

			capability_probe_observation observation;
			if (!evaluate_schema(data, aLimits, observation)) {
				return unavailable(
					capability_probe_unavailability::
						malformed_response,
					application::github_capability_diagnostic::schema_shape);
			}
			return observation;
		} catch (const std::bad_alloc&) {
			return unavailable(
				capability_probe_unavailability::allocation_failure, application::github_capability_diagnostic::normalization_allocation);
		} catch (...) {
			return unavailable(
				capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::normalization_unexpected);
		}
	}
	// [[BPR-1525]]. Keep the already admitted request-local tree instead of
	// reparsing the entire serialized prefix after every physical part. Glaze
	// still serializes each prefix under the same byte bound, and finish runs
	// the same complete normalizer. No state is shared between attempts.
	struct capability_probe_batch::assembly
	{
		capability_object Envelope{ { "data", capability_json(capability_object{ }) } };
	};

	capability_probe_batch::capability_probe_batch()                                                     = default;
	capability_probe_batch::~capability_probe_batch()                                                    = default;
	capability_probe_batch::capability_probe_batch(capability_probe_batch&&) noexcept                    = default;
	auto capability_probe_batch::operator=(capability_probe_batch&&) noexcept -> capability_probe_batch& = default;

	auto capability_probe_batch::append(const capability_probe_response& aResponse, const std::uint8_t aPart) noexcept -> std::expected<void, capability_probe_observation>
	{
		const auto refuse = [](capability_probe_unavailability aReason, application::github_capability_diagnostic aDetail) -> std::expected<void, capability_probe_observation> { return std::unexpected{ unavailable(aReason, aDetail) }; };
		if (Failed || aPart != Parts || Parts >= capability_probe_part_count) {
			Failed = true;
			return refuse(capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::provider_correlation);
		}
		Failed = true;
		try {
			const capability_probe_limits limits;
			if (aResponse.Body.size() > limits.MaximumResponseBytes - Bytes) {
				return refuse(capability_probe_unavailability::response_limit, application::github_capability_diagnostic::normalizer_body_limit);
			}
			auto data = checked_capability_data(aResponse, limits);
			if (!data) {
				return std::unexpected{ data.error() };
			}
			const auto aliases = capability_probe_part_aliases(aPart);
			if (static_cast<std::size_t>(data->size()) != aliases.size()) {
				return refuse(capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_aliases);
			}
			for (const auto alias : aliases) {
				if (!has_member(*data, QString::fromUtf8(alias.data(), static_cast<qsizetype>(alias.size())))) {
					return refuse(capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_aliases);
				}
			}
			auto preflight = json_structural_preflight{ aResponse.Body, limits };
			if (preflight.run() != json_preflight_result::valid || preflight.value_count() > limits.MaximumTotalValues - Values) {
				return refuse(capability_probe_unavailability::response_limit, application::github_capability_diagnostic::json_limits);
			}
			if (!Assembly) {
				Assembly = std::make_unique<assembly>();
			}
			auto& merged = Assembly->Envelope.at("data").get_object();
			for (auto& [key, value] : *data) {
				if (!merged.emplace(key, std::move(value)).second) {
					return refuse(capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_aliases);
				}
			}
			auto bytes = glz::write<capability_json_options{ }>(Assembly->Envelope);
			if (!bytes || bytes->size() > limits.MaximumResponseBytes) {
				return refuse(capability_probe_unavailability::response_limit, application::github_capability_diagnostic::normalizer_body_limit);
			}
			Response      = aResponse;
			Response.Body = std::move(*bytes);
			Bytes += aResponse.Body.size();
			Values += preflight.value_count();
			++Parts;
			Failed = false;
			return { };
		} catch (...) {
			return refuse(capability_probe_unavailability::allocation_failure, application::github_capability_diagnostic::normalization_allocation);
		}
	}
	auto capability_probe_batch::finish() const noexcept -> capability_probe_observation
	{
		if (Failed || Parts != capability_probe_part_count) {
			return unavailable(capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_aliases);
		}
		try {
			return normalize_capability_probe(Response);
		} catch (...) {
			return unavailable(capability_probe_unavailability::allocation_failure, application::github_capability_diagnostic::normalization_allocation);
		}
	}
}
