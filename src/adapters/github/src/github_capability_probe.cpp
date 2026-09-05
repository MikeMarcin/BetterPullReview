#include "github_capability_probe.hpp"
#include "github_graphql_operation_policy.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QString>

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
		constexpr std::string_view graphql_document = "query BetterPullReviewCapabilityProbeV1{"
													  "schemaRoot:__schema{queryType{...FieldType}"
													  "mutationType{...FieldType}}"
													  "repository:__type(name:\"Repository\"){...FieldType}"
													  "pullRequest:__type(name:\"PullRequest\"){...FieldType}"
													  "threadConnection:__type(name:\"PullRequestReviewThreadConnection\")"
													  "{...FieldType}"
													  "reviewThread:__type(name:\"PullRequestReviewThread\"){...FieldType}"
													  "commentConnection:__type(name:\"PullRequestReviewCommentConnection\")"
													  "{...FieldType}"
													  "reviewComment:__type(name:\"PullRequestReviewComment\"){...FieldType}"
													  "changedFileConnection:__type(name:\"PullRequestChangedFileConnection\")"
													  "{...FieldType}"
													  "changedFile:__type(name:\"PullRequestChangedFile\"){...FieldType}"
													  "pageInfo:__type(name:\"PageInfo\"){...FieldType}"
													  "actor:__type(name:\"Actor\"){...FieldType}"
													  "node:__type(name:\"Node\"){...FieldType}"
													  "reviewThreadInterfaces:__type(name:\"PullRequestReviewThread\")"
													  "{...InterfaceType}"
													  "actorPossibleTypes:__type(name:\"Actor\"){...PossibleType}"
													  "nodePossibleTypes:__type(name:\"Node\"){...PossibleType}"
													  "threadSubject:__type(name:\"PullRequestReviewThreadSubjectType\")"
													  "{...EnumType}"
													  "diffSide:__type(name:\"DiffSide\"){...EnumType}"
													  "fileViewedState:__type(name:\"FileViewedState\"){...EnumType}"
													  "markInput:__type(name:\"MarkFileAsViewedInput\"){...InputType}"
													  "markPayload:__type(name:\"MarkFileAsViewedPayload\"){...FieldType}"
													  "unmarkInput:__type(name:\"UnmarkFileAsViewedInput\"){...InputType}"
													  "unmarkPayload:__type(name:\"UnmarkFileAsViewedPayload\")"
													  "{...FieldType}"
													  "resolveInput:__type(name:\"ResolveReviewThreadInput\")"
													  "{...InputType}"
													  "resolvePayload:__type(name:\"ResolveReviewThreadPayload\")"
													  "{...FieldType}"
													  "unresolveInput:__type(name:\"UnresolveReviewThreadInput\")"
													  "{...InputType}"
													  "unresolvePayload:__type(name:\"UnresolveReviewThreadPayload\")"
													  "{...FieldType}"
													  "}"
													  "fragment TypeRef on __Type{kind name ofType{kind name}}"
													  "fragment FieldType on __Type{kind name "
													  "fields(includeDeprecated:true){name args{name defaultValue "
													  "type{...TypeRef}} type{...TypeRef}}}"
													  "fragment InputType on __Type{kind name inputFields{name defaultValue "
													  "type{...TypeRef}}}"
													  "fragment EnumType on __Type{kind name "
													  "enumValues(includeDeprecated:true){name}}"
													  "fragment InterfaceType on __Type{kind name interfaces{kind name}}"
													  "fragment PossibleType on __Type{kind name possibleTypes{kind name}}";

		constexpr capability_probe_limits hard_limits{ };

		static_assert(
			inspect_graphql_operation_policy(graphql_document)
			== graphql_operation_policy_result::query_only);

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

		enum class json_preflight_result : std::uint8_t {
			valid,
			malformed,
			duplicate_key,
			structural_limit,
		};

		class json_structural_preflight
		{
		public:
			json_structural_preflight(
				const std::string_view         aInput,
				const capability_probe_limits& aLimits) noexcept
				: Input{ aInput }
				, Limits{ aLimits }
			{
			}

			[[nodiscard]] auto run() noexcept -> json_preflight_result
			{
				whitespace();
				const auto result = value(0U);
				if (result != json_preflight_result::valid) {
					return result;
				}
				whitespace();
				return Position == Input.size()
					? json_preflight_result::valid
					: json_preflight_result::malformed;
			}

		private:
			[[nodiscard]] auto value(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (Values == Limits.MaximumTotalValues) {
					return json_preflight_result::structural_limit;
				}
				++Values;
				whitespace();
				if (Position >= Input.size()) {
					return json_preflight_result::malformed;
				}
				switch (Input[Position]) {
				case '{':
					return object(aDepth);
				case '[':
					return array(aDepth);
				case '"':
					if (string(nullptr, false)) {
						return json_preflight_result::valid;
					}
					return StringLimitExceeded
						? json_preflight_result::structural_limit
						: json_preflight_result::malformed;
				case 't':
					return literal("true");
				case 'f':
					return literal("false");
				case 'n':
					return literal("null");
				default:
					return number();
				}
			}

			[[nodiscard]] auto object(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (aDepth >= Limits.MaximumDepth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume('}')) {
					return json_preflight_result::valid;
				}

				std::array<std::string_view, hard_limits.MaximumObjectMembers> keys;
				std::size_t                                                    count = 0U;
				while (true) {
					if (count == Limits.MaximumObjectMembers) {
						return json_preflight_result::structural_limit;
					}
					std::string_view key;
					if (!string(&key, true)) {
						return StringLimitExceeded
							? json_preflight_result::structural_limit
							: json_preflight_result::malformed;
					}
					if (
						std::ranges::find(
							keys.begin(),
							keys.begin() + static_cast<std::ptrdiff_t>(count),
							key)
						!= keys.begin() + static_cast<std::ptrdiff_t>(count)) {
						return json_preflight_result::duplicate_key;
					}
					keys[count++] = key;
					whitespace();
					if (!consume(':')) {
						return json_preflight_result::malformed;
					}
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume('}')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto array(const std::size_t aDepth) noexcept
				-> json_preflight_result
			{
				if (aDepth >= Limits.MaximumDepth) {
					return json_preflight_result::structural_limit;
				}
				++Position;
				whitespace();
				if (consume(']')) {
					return json_preflight_result::valid;
				}

				std::size_t count = 0U;
				while (true) {
					if (count == Limits.MaximumArrayElements) {
						return json_preflight_result::structural_limit;
					}
					++count;
					const auto child = value(aDepth + 1U);
					if (child != json_preflight_result::valid) {
						return child;
					}
					whitespace();
					if (consume(']')) {
						return json_preflight_result::valid;
					}
					if (!consume(',')) {
						return json_preflight_result::malformed;
					}
					whitespace();
				}
			}

			[[nodiscard]] auto string(
				std::string_view* aOutput,
				const bool        aObjectKey) noexcept -> bool
			{
				if (!consume('"')) {
					return false;
				}
				const auto start = Position;
				while (Position < Input.size()) {
					if (Position - start > Limits.MaximumStringBytes) {
						StringLimitExceeded = true;
						return false;
					}
					const auto character = static_cast<unsigned char>(Input[Position++]);
					if (character == '"') {
						if (aOutput != nullptr) {
							*aOutput = Input.substr(start, Position - start - 1U);
						}
						return true;
					}
					if (character < 0x20U) {
						return false;
					}
					if (character != '\\') {
						continue;
					}
					if (aObjectKey || Position >= Input.size()) {
						return false;
					}
					const auto escaped = Input[Position++];
					if (
						escaped == '"' || escaped == '\\' || escaped == '/'
						|| escaped == 'b' || escaped == 'f' || escaped == 'n'
						|| escaped == 'r' || escaped == 't') {
						continue;
					}
					if (escaped != 'u' || Input.size() - Position < 4U) {
						return false;
					}
					for (std::size_t index = 0U; index < 4U; ++index) {
						const auto digit = static_cast<unsigned char>(Input[Position++]);
						if (std::isxdigit(digit) == 0) {
							return false;
						}
					}
				}
				return false;
			}

			[[nodiscard]] auto number() noexcept -> json_preflight_result
			{
				const auto start = Position;
				(void)consume('-');
				if (consume('0')) {
					if (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						return json_preflight_result::malformed;
					}
				} else {
					if (
						Position >= Input.size() || Input[Position] < '1'
						|| Input[Position] > '9') {
						return json_preflight_result::malformed;
					}
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
				}
				if (consume('.')) {
					const auto digits = Position;
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
					if (Position == digits) {
						return json_preflight_result::malformed;
					}
				}
				if (
					Position < Input.size()
					&& (Input[Position] == 'e' || Input[Position] == 'E')) {
					++Position;
					if (
						Position < Input.size()
						&& (Input[Position] == '+' || Input[Position] == '-')) {
						++Position;
					}
					const auto digits = Position;
					while (
						Position < Input.size() && Input[Position] >= '0'
						&& Input[Position] <= '9') {
						++Position;
					}
					if (Position == digits) {
						return json_preflight_result::malformed;
					}
				}
				return Position != start ? json_preflight_result::valid
										 : json_preflight_result::malformed;
			}

			[[nodiscard]] auto literal(const std::string_view aExpected) noexcept
				-> json_preflight_result
			{
				if (Input.substr(Position, aExpected.size()) != aExpected) {
					return json_preflight_result::malformed;
				}
				Position += aExpected.size();
				return json_preflight_result::valid;
			}

			auto whitespace() noexcept -> void
			{
				while (
					Position < Input.size()
					&& (Input[Position] == ' ' || Input[Position] == '\t'
						|| Input[Position] == '\r' || Input[Position] == '\n')) {
					++Position;
				}
			}

			[[nodiscard]] auto consume(const char aExpected) noexcept -> bool
			{
				if (Position >= Input.size() || Input[Position] != aExpected) {
					return false;
				}
				++Position;
				return true;
			}

			std::string_view               Input;
			const capability_probe_limits& Limits;
			std::size_t                    Position            = 0U;
			std::size_t                    Values              = 0U;
			bool                           StringLimitExceeded = false;
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
			bool       Missing = false;
			QString    Kind;
			QString    Name;
			bool       MembersAvailable = false;
			QJsonArray Members;
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
		[[nodiscard]] auto graphql_error_detail(const QJsonValue& aErrors) noexcept
			-> application::github_capability_diagnostic
		{
			try {
				using diagnostic = application::github_capability_diagnostic;
				if (!aErrors.isArray()) {
					return diagnostic::graphql_errors_not_array;
				}
				const auto errors = aErrors.toArray();
				if (errors.isEmpty()) {
					return diagnostic::graphql_errors_empty;
				}
				std::optional<diagnostic> selected;
				bool                      unknown       = false;
				bool                      mixed         = false;
				std::size_t               markerCount   = 0U;
				const auto                observeMarker = [&](const QJsonValue& aMarker) {
					++markerCount;
					if (!aMarker.isString()) {
						return false;
					}
					const auto detail = graphql_error_code_detail(aMarker.toString());
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
					if (!value.isObject()) {
						return diagnostic::graphql_errors_malformed;
					}
					const auto entry  = value.toObject();
					const auto before = markerCount;
					if (entry.contains("type") && !observeMarker(entry.value("type"))) {
						return diagnostic::graphql_errors_malformed;
					}
					if (entry.contains("extensions")) {
						if (!entry.value("extensions").isObject()) {
							return diagnostic::graphql_errors_malformed;
						}
						const auto extensions = entry.value("extensions").toObject();
						for (const auto* key : { "type", "code" }) {
							if (extensions.contains(key) && !observeMarker(extensions.value(key))) {
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
			const QJsonObject&                   aObject,
			const std::initializer_list<QString> aKeys) -> bool
		{
			if (aObject.size() != static_cast<qsizetype>(aKeys.size())) {
				return false;
			}
			return std::ranges::all_of(
				aKeys,
				[&aObject](const QString& aKey) { return aObject.contains(aKey); });
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

		[[nodiscard]] auto valid_type_reference(const QJsonValue& aValue) -> bool
		{
			if (!aValue.isObject()) {
				return false;
			}
			const auto object = aValue.toObject();
			if (
				!exact_keys(
					object,
					{
						QStringLiteral("kind"),
						QStringLiteral("name"),
						QStringLiteral("ofType"),
					})
				|| !object.value("kind").isString()) {
				return false;
			}

			const auto kind = object.value("kind").toString();
			if (!valid_kind(kind)) {
				return false;
			}
			const bool wrapper = kind == QStringLiteral("LIST")
				|| kind == QStringLiteral("NON_NULL");
			if (wrapper) {
				if (!object.value("name").isNull()) {
					return false;
				}
				const auto inner = object.value("ofType");
				if (!inner.isObject()) {
					return false;
				}
				const auto innerObject = inner.toObject();
				if (
					!exact_keys(
						innerObject,
						{ QStringLiteral("kind"), QStringLiteral("name") })
					|| !innerObject.value("kind").isString()
					|| !valid_kind(innerObject.value("kind").toString())) {
					return false;
				}
				const auto innerKind    = innerObject.value("kind").toString();
				const bool innerWrapper = innerKind == QStringLiteral("LIST")
					|| innerKind == QStringLiteral("NON_NULL");
				return innerWrapper
					? innerObject.value("name").isNull()
					: innerObject.value("name").isString()
						&& !innerObject.value("name").toString().isEmpty();
			}
			return object.value("name").isString()
				&& !object.value("name").toString().isEmpty()
				&& object.value("ofType").isNull();
		}

		[[nodiscard]] auto exact_type(
			const QJsonValue&       aValue,
			const type_requirement& aRequirement) -> bool
		{
			if (!valid_type_reference(aValue)) {
				return false;
			}
			const auto object       = aValue.toObject();
			const auto expectedKind = QString::fromUtf8(
				aRequirement.Kind.data(),
				static_cast<qsizetype>(aRequirement.Kind.size()));
			const auto expectedName = QString::fromUtf8(
				aRequirement.Name.data(),
				static_cast<qsizetype>(aRequirement.Name.size()));

			switch (aRequirement.Wrapper) {
			case type_wrapper::named:
				return object.value("kind").toString() == expectedKind
					&& object.value("name").toString() == expectedName
					&& object.value("ofType").isNull();
			case type_wrapper::non_null:
			case type_wrapper::list:     {
				const auto wrapper = aRequirement.Wrapper
						== type_wrapper::non_null
					? QStringLiteral("NON_NULL")
					: QStringLiteral("LIST");
				if (object.value("kind").toString() != wrapper) {
					return false;
				}
				const auto inner = object.value("ofType").toObject();
				return inner.value("kind").toString() == expectedKind
					&& inner.value("name").toString() == expectedName;
			}
			}
			return false;
		}

		[[nodiscard]] auto required_argument(const QJsonObject& aArgument) -> bool
		{
			return aArgument.value("type")
					   .toObject()
					   .value("kind")
					   .toString()
				== QStringLiteral("NON_NULL")
				&& aArgument.value("defaultValue").isNull();
		}

		[[nodiscard]] auto validate_arguments(
			const QJsonArray&              aArguments,
			const capability_probe_limits& aLimits) -> bool
		{
			if (
				aArguments.size()
				> static_cast<qsizetype>(aLimits.MaximumArguments)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aArguments) {
				if (!value.isObject()) {
					return false;
				}
				auto object = value.toObject();
				if (
					!exact_keys(
						object,
						{
							QStringLiteral("name"),
							QStringLiteral("defaultValue"),
							QStringLiteral("type"),
						})
					|| !object.value("name").isString()
					|| object.value("name").toString().isEmpty()
					|| (!object.value("defaultValue").isNull()
						&& !object.value("defaultValue").isString())
					|| !valid_type_reference(object.value("type"))) {
					return false;
				}
				const auto name = object.value("name").toString();
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto validate_field_members(
			const QJsonArray&              aFields,
			const std::size_t              aMaximum,
			const capability_probe_limits& aLimits) -> bool
		{
			if (aFields.size() > static_cast<qsizetype>(aMaximum)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aFields) {
				if (!value.isObject()) {
					return false;
				}
				const auto object = value.toObject();
				if (
					!exact_keys(
						object,
						{
							QStringLiteral("name"),
							QStringLiteral("args"),
							QStringLiteral("type"),
						})
					|| !object.value("name").isString()
					|| object.value("name").toString().isEmpty()
					|| !object.value("args").isArray()
					|| !validate_arguments(
						object.value("args").toArray(),
						aLimits)
					|| !valid_type_reference(object.value("type"))) {
					return false;
				}
				const auto name = object.value("name").toString();
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto validate_input_members(
			const QJsonArray&              aFields,
			const capability_probe_limits& aLimits) -> bool
		{
			if (
				aFields.size()
				> static_cast<qsizetype>(aLimits.MaximumInputFields)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aFields) {
				if (!value.isObject()) {
					return false;
				}
				const auto object = value.toObject();
				if (
					!exact_keys(
						object,
						{
							QStringLiteral("name"),
							QStringLiteral("defaultValue"),
							QStringLiteral("type"),
						})
					|| !object.value("name").isString()
					|| object.value("name").toString().isEmpty()
					|| (!object.value("defaultValue").isNull()
						&& !object.value("defaultValue").isString())
					|| !valid_type_reference(object.value("type"))) {
					return false;
				}
				const auto name = object.value("name").toString();
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto validate_enum_members(
			const QJsonArray&              aValues,
			const capability_probe_limits& aLimits) -> bool
		{
			if (
				aValues.size()
				> static_cast<qsizetype>(aLimits.MaximumEnumValues)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aValues) {
				if (!value.isObject()) {
					return false;
				}
				const auto object = value.toObject();
				if (
					!exact_keys(object, { QStringLiteral("name") })
					|| !object.value("name").isString()
					|| object.value("name").toString().isEmpty()) {
					return false;
				}
				const auto name = object.value("name").toString();
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto
		validate_named_type_members(const QJsonArray&              aValues,
									const capability_probe_limits& aLimits) -> bool
		{
			if (aValues.size() > static_cast<qsizetype>(aLimits.MaximumNamedTypes)) {
				return false;
			}
			QSet<QString> names;
			for (const auto& value : aValues) {
				if (!value.isObject()) {
					return false;
				}
				const auto object = value.toObject();
				if (!exact_keys(object,
								{
									QStringLiteral("kind"),
									QStringLiteral("name"),
								})
					|| !object.value("kind").isString()
					|| !valid_kind(object.value("kind").toString())
					|| !object.value("name").isString()
					|| object.value("name").toString().isEmpty()) {
					return false;
				}
				const auto name = object.value("name").toString();
				if (names.contains(name)) {
					return false;
				}
				names.insert(name);
			}
			return true;
		}

		[[nodiscard]] auto parse_selected_type(const QJsonValue&              aValue,
											   const std::string_view         aExpectedName,
											   const selected_type_shape      aShape,
											   const std::size_t              aMaximumFields,
											   const capability_probe_limits& aLimits,
											   selected_schema_type&          aOutput) -> bool
		{
			if (aValue.isNull()) {
				aOutput = {
					.Missing          = true,
					.Kind             = { },
					.Name             = { },
					.MembersAvailable = false,
					.Members          = { },
				};
				return true;
			}
			if (!aValue.isObject()) {
				return false;
			}

			const auto object     = aValue.toObject();
			const auto memberName = aShape == selected_type_shape::fields
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
				|| !object.value("kind").isString()
				|| !valid_kind(object.value("kind").toString())
				|| !object.value("name").isString()
				|| object.value("name").toString().isEmpty()) {
				return false;
			}
			if (!aExpectedName.empty()) {
				const auto expected = QString::fromUtf8(
					aExpectedName.data(),
					static_cast<qsizetype>(aExpectedName.size()));
				if (object.value("name").toString() != expected) {
					return false;
				}
			}

			aOutput = {
				.Missing          = false,
				.Kind             = object.value("kind").toString(),
				.Name             = object.value("name").toString(),
				.MembersAvailable = false,
				.Members          = { },
			};
			const auto members = object.value(memberName);
			if (members.isNull()) {
				return !kind_has_selected_members(aOutput.Kind, aShape);
			}
			if (!members.isArray()) {
				return false;
			}
			aOutput.MembersAvailable = true;
			aOutput.Members          = members.toArray();
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
			const QJsonArray&      aMembers,
			const std::string_view aName) -> std::optional<QJsonObject>
		{
			const auto expected = QString::fromUtf8(
				aName.data(),
				static_cast<qsizetype>(aName.size()));
			for (const auto& value : aMembers) {
				auto object = value.toObject();
				if (object.value("name").toString() == expected) {
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
			if (!exact_type(field->value("type"), aReturnType)) {
				return capability_schema_fact_state::incompatible;
			}

			const auto    arguments = field->value("args").toArray();
			QSet<QString> expectedNames;
			for (const auto& expected : aExpectedArguments) {
				expectedNames.insert(
					QString::fromUtf8(
						expected.Name.data(),
						static_cast<qsizetype>(expected.Name.size())));
				const auto argument = find_named_member(arguments, expected.Name);
				if (
					!argument.has_value()
					|| !exact_type(argument->value("type"), expected.Type)) {
					return capability_schema_fact_state::incompatible;
				}
			}
			for (const auto& value : arguments) {
				const auto argument = value.toObject();
				if (
					!expectedNames.contains(
						argument.value("name").toString())
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
					|| !exact_type(field->value("type"), expected.Type)) {
					return capability_schema_fact_state::incompatible;
				}
			}
			for (const auto& value : aType.Members) {
				const auto field = value.toObject();
				if (
					!expectedNames.contains(field.value("name").toString())
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
					&& member->value("kind").toString() == expectedMemberKind
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
			if (aLeft.Members.isEmpty()) {
				return capability_schema_fact_state::incompatible;
			}
			for (const auto& value : aLeft.Members) {
				const auto member = value.toObject();
				if (member.value("kind").toString() != QStringLiteral("OBJECT")) {
					return capability_schema_fact_state::incompatible;
				}
				const auto name     = member.value("name").toString().toStdString();
				const auto matching = find_named_member(aRight.Members, name);
				if (!matching.has_value()
					|| matching->value("kind").toString() != QStringLiteral("OBJECT")) {
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
			const QJsonObject& aData) -> bool
		{
			return aData.size()
				== static_cast<qsizetype>(response_aliases.size())
				&& std::ranges::all_of(
					   response_aliases,
					   [&aData](const auto aAlias) {
						   return aData.contains(QString::fromUtf8(
							   aAlias.data(),
							   static_cast<qsizetype>(aAlias.size())));
					   });
		}

		[[nodiscard]] auto field_array_exceeds_limits(
			const QJsonValue&              aValue,
			const std::size_t              aMaximumFields,
			const capability_probe_limits& aLimits) -> bool
		{
			if (!aValue.isObject()) {
				return false;
			}
			const auto fields = aValue.toObject().value("fields");
			if (!fields.isArray()) {
				return false;
			}
			const auto array = fields.toArray();
			if (array.size() > static_cast<qsizetype>(aMaximumFields)) {
				return true;
			}
			return std::ranges::any_of(array, [&aLimits](const auto& aField) {
				return aField.isObject()
					&& aField.toObject().value("args").isArray()
					&& aField.toObject().value("args").toArray().size()
					> static_cast<qsizetype>(aLimits.MaximumArguments);
			});
		}

		[[nodiscard]] auto schema_specific_limits_exceeded(
			const QJsonObject&             aData,
			const capability_probe_limits& aLimits) -> bool
		{
			const auto schema = aData.value("schemaRoot");
			if (
				schema.isObject()
				&& (field_array_exceeds_limits(schema.toObject().value("queryType"),
											   aLimits.MaximumQueryFields, aLimits)
					|| field_array_exceeds_limits(
						schema.toObject().value("mutationType"),
						aLimits.MaximumMutationFields, aLimits))) {
				return true;
			}
			if (field_array_exceeds_limits(aData.value("repository"),
										   aLimits.MaximumRepositoryFields,
										   aLimits)) {
				return true;
			}
			if (
				field_array_exceeds_limits(
					aData.value("pullRequest"),
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
						aData.value(QString::fromUtf8(
							alias.data(),
							static_cast<qsizetype>(alias.size()))),
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
				const auto value = aData.value(QString::fromUtf8(
					alias.data(),
					static_cast<qsizetype>(alias.size())));
				if (
					value.isObject()
					&& value.toObject().value("inputFields").isArray()
					&& value.toObject()
							.value("inputFields")
							.toArray()
							.size()
						> static_cast<qsizetype>(aLimits.MaximumInputFields)) {
					return true;
				}
			}

			for (const auto alias : { "threadSubject", "fileViewedState" }) {
				const auto value = aData.value(alias);
				if (
					value.isObject()
					&& value.toObject().value("enumValues").isArray()
					&& value.toObject()
							.value("enumValues")
							.toArray()
							.size()
						> static_cast<qsizetype>(aLimits.MaximumEnumValues)) {
					return true;
				}
			}
			for (const auto alias : {
					 "reviewThreadInterfaces",
					 "actorPossibleTypes",
					 "nodePossibleTypes",
				 }) {
				const auto value = aData.value(alias);
				if (!value.isObject()) {
					continue;
				}
				const auto object  = value.toObject();
				const auto members = object.contains("interfaces")
					? object.value("interfaces")
					: object.value("possibleTypes");
				if (members.isArray()
					&& members.toArray().size()
						> static_cast<qsizetype>(aLimits.MaximumNamedTypes)) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto evaluate_schema(const QJsonObject&             aData,
										   const capability_probe_limits& aLimits,
										   capability_probe_observation&  aOutput) -> bool
		{
			const auto parseFields = [&aData, &aLimits](const std::string_view aAlias,
														const std::string_view aTypeName,
														const std::size_t      aMaximum,
														selected_schema_type&  aResult) {
				return parse_selected_type(
					aData.value(QString::fromUtf8(aAlias.data(),
												  static_cast<qsizetype>(aAlias.size()))),
					aTypeName, selected_type_shape::fields, aMaximum, aLimits, aResult);
			};
			const auto parseInput = [&aData, &aLimits](const std::string_view aAlias,
													   const std::string_view aTypeName,
													   selected_schema_type&  aResult) {
				return parse_selected_type(
					aData.value(QString::fromUtf8(aAlias.data(),
												  static_cast<qsizetype>(aAlias.size()))),
					aTypeName, selected_type_shape::input_fields,
					aLimits.MaximumInputFields, aLimits, aResult);
			};
			const auto parseEnum = [&aData, &aLimits](const std::string_view aAlias,
													  const std::string_view aTypeName,
													  selected_schema_type&  aResult) {
				return parse_selected_type(
					aData.value(QString::fromUtf8(aAlias.data(),
												  static_cast<qsizetype>(aAlias.size()))),
					aTypeName, selected_type_shape::enum_values,
					aLimits.MaximumEnumValues, aLimits, aResult);
			};
			const auto parseNamed = [&aData, &aLimits](const std::string_view    aAlias,
													   const std::string_view    aTypeName,
													   const selected_type_shape aShape,
													   selected_schema_type&     aResult) {
				return parse_selected_type(
					aData.value(QString::fromUtf8(aAlias.data(),
												  static_cast<qsizetype>(aAlias.size()))),
					aTypeName, aShape, aLimits.MaximumNamedTypes, aLimits, aResult);
			};

			selected_schema_type query;
			selected_schema_type mutation;
			const auto           schemaRoot = aData.value("schemaRoot");
			if (!schemaRoot.isObject()) {
				return false;
			}
			const auto schemaObject = schemaRoot.toObject();
			if (!exact_keys(schemaObject,
							{
								QStringLiteral("queryType"),
								QStringLiteral("mutationType"),
							})
				|| !parse_selected_type(schemaObject.value("queryType"), { },
										selected_type_shape::fields,
										aLimits.MaximumQueryFields, aLimits, query)
				|| !parse_selected_type(schemaObject.value("mutationType"), { },
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
			const QJsonValue& aErrors) -> bool
		{
			if (!aErrors.isArray()) {
				return false;
			}
			for (const auto& value : aErrors.toArray()) {
				if (!value.isObject()) {
					continue;
				}
				const auto error = value.toObject();
				if (
					error.value("type").isString()
					&& error.value("type").toString()
						== QStringLiteral("RATE_LIMITED")) {
					return true;
				}
				const auto extensions = error.value("extensions");
				if (!extensions.isObject()) {
					continue;
				}
				const auto extension = extensions.toObject();
				for (const auto key : { "type", "code" }) {
					if (
						extension.value(key).isString()
						&& extension.value(key).toString()
							== QStringLiteral("RATE_LIMITED")) {
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] auto json_content_type(
			const std::string_view aContentType) noexcept -> bool
		{
			if (aContentType.size() > 256U) {
				return false;
			}
			const auto separator = aContentType.find(';');
			auto       mediaType = aContentType.substr(0U, separator);
			while (!mediaType.empty() && std::isspace(static_cast<unsigned char>(mediaType.front())) != 0) {
				mediaType.remove_prefix(1U);
			}
			while (!mediaType.empty() && std::isspace(static_cast<unsigned char>(mediaType.back())) != 0) {
				mediaType.remove_suffix(1U);
			}
			constexpr std::string_view expected = "application/json";
			if (mediaType.size() != expected.size()) {
				return false;
			}
			for (std::size_t index = 0U; index < mediaType.size(); ++index) {
				if (ascii_fold(mediaType[index]) != expected[index]) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto
		graphql_error_body_rate_limited(const capability_probe_response& aResponse,
										const capability_probe_limits&   aLimits) noexcept
			-> bool
		{
			if (aResponse.Body.empty()
				|| aResponse.Body.size() > aLimits.MaximumResponseBytes
				|| !json_content_type(aResponse.ContentType)) {
				return false;
			}
			try {
				if (json_structural_preflight{ aResponse.Body, aLimits }.run()
					!= json_preflight_result::valid) {
					return false;
				}
				QJsonParseError error;
				const auto      document = QJsonDocument::fromJson(
					QByteArray{
						aResponse.Body.data(),
						static_cast<qsizetype>(aResponse.Body.size()),
					},
					&error);
				return error.error == QJsonParseError::NoError && document.isObject()
					&& graphql_error_rate_limited(
						   document.object().value(QStringLiteral("errors")));
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

	auto capability_probe_graphql_document() noexcept -> std::string_view
	{
		return graphql_document;
	}

	auto capability_probe_document_is_query_only(
		const std::string_view aDocument) noexcept -> bool
	{
		return inspect_graphql_operation_policy(aDocument)
			== graphql_operation_policy_result::query_only;
	}

	auto compile_capability_probe_request(
		const validated_github_host_profile& aProfile,
		const capability_probe_limits&       aLimits)
		-> capability_probe_request_result
	{
		const auto contract = aProfile.server_contract();
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
			const QJsonObject payload{
				{
					QStringLiteral("query"),
					QString::fromUtf8(
						graphql_document.data(),
						static_cast<qsizetype>(graphql_document.size())),
				},
			};
			const auto bytes = QJsonDocument{ payload }.toJson(QJsonDocument::Compact);
			if (
				bytes.size() < 0
				|| static_cast<std::size_t>(bytes.size())
					> aLimits.MaximumRequestBytes) {
				return std::unexpected{
					capability_probe_unavailability::response_limit
				};
			}

			return capability_probe_request{
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
				.Body = {
					bytes.constData(),
					static_cast<std::size_t>(bytes.size()),
				},
			};
		} catch (const std::bad_alloc&) {
			return std::unexpected{
				capability_probe_unavailability::allocation_failure
			};
		}
	}

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
			QJsonParseError parseError;
			const auto      document = QJsonDocument::fromJson(QByteArray{ aBody.data(), static_cast<qsizetype>(aBody.size()) }, &parseError);
			if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
				result.State = state::omitted_invalid;
				return result;
			}
			const auto envelope = document.object();
			if (!envelope.contains("errors")) {
				return result;
			}
			const auto errors = envelope.value("errors");
			if (!errors.isArray() || errors.toArray().isEmpty() || errors.toArray().size() > 8) {
				result.State = state::omitted_invalid;
				return result;
			}
			const QByteArray credential{ reinterpret_cast<const char*>(aCredential.data()), static_cast<qsizetype>(aCredential.size()) };
			auto             percent = QByteArray{ };
			for (const auto character : credential) {
				percent += '%';
				percent += QByteArray(1, character).toHex();
			}
			const auto       encodedJson = QJsonDocument{ QJsonArray{ QString::fromUtf8(credential) } }.toJson(QJsonDocument::Compact);
			const std::array representations{
				QString::fromUtf8(credential), QString::fromLatin1(credential.toBase64()),
				QString::fromLatin1(credential.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)),
				QString::fromLatin1(credential.toHex()), QString::fromLatin1(percent), QString::fromLatin1(credential.toPercentEncoding()),
				QString::fromUtf8(encodedJson.mid(2, encodedJson.size() - 4))
			};
			QString    summary;
			const auto append = [&](const QString& aName, const QJsonValue& aValue) -> bool {
				if (aValue.isUndefined()) {
					return true;
				}
				if (!aValue.isString() || aValue.toString().size() > 8192) {
					return false;
				}
				auto text = aValue.toString();
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
				summary += aName + QStringLiteral("=\"") + text + QStringLiteral("\" ");
				return true;
			};
			for (const auto& value : errors.toArray()) {
				if (!value.isObject()) {
					result.State = state::omitted_invalid;
					return result;
				}
				const auto entry      = value.toObject();
				const auto extensions = entry.value("extensions");
				if ((!extensions.isUndefined() && !extensions.isObject())
					|| !append(QStringLiteral("message"), entry.value("message"))
					|| !append(QStringLiteral("type"), entry.value("type"))
					|| !append(QStringLiteral("type"), extensions.toObject().value("type"))
					|| !append(QStringLiteral("code"), extensions.toObject().value("code"))) {
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

	auto normalize_capability_probe(
		const capability_probe_transport_result& aResponse,
		const capability_probe_limits&           aLimits) noexcept
		-> capability_probe_observation
	{
		if (!valid_limits(aLimits)) {
			return unavailable(
				capability_probe_unavailability::invalid_limits, application::github_capability_diagnostic::normalizer_invalid_limits);
		}
		if (!aResponse.has_value()) {
			const auto reason = aResponse.error();
			return unavailable(
				!valid(reason)
						|| reason == capability_probe_unavailability::none
					? capability_probe_unavailability::malformed_response
					: reason,
				application::github_capability_diagnostic::normalizer_transport_unavailable);
		}

		const auto& value = *aResponse;
		if (!value.Authenticated) {
			return unavailable(
				capability_probe_unavailability::authentication, application::github_capability_diagnostic::normalizer_transport_unavailable);
		}
		const auto rateLimit = parse_rate_limit_response_facts(value);
		if (!rateLimit.has_value()) {
			return unavailable(
				capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::normalizer_rate_metadata);
		}
		if (value.Body.size() > aLimits.MaximumResponseBytes) {
			return unavailable(capability_probe_unavailability::response_limit, application::github_capability_diagnostic::normalizer_body_limit);
		}
		if (value.Status != 200) {
			if (value.Status == 403
				&& (rateLimit->signals_throttling()
					|| graphql_error_body_rate_limited(value, aLimits))) {
				return unavailable(capability_probe_unavailability::rate_limit, application::github_capability_diagnostic::normalizer_transport_unavailable);
			}
			return unavailable(status_failure(value.Status), application::github_capability_diagnostic::normalizer_transport_unavailable);
		}
		if (!json_content_type(value.ContentType)) {
			return unavailable(
				capability_probe_unavailability::invalid_content_type, application::github_capability_diagnostic::transport_invalid_content_type);
		}
		if (value.Body.empty()) {
			return unavailable(capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::json_shape);
		}

		try {
			const auto preflight = json_structural_preflight{ value.Body, aLimits }.run();
			if (preflight != json_preflight_result::valid) {
				return unavailable(
					preflight == json_preflight_result::structural_limit
						? capability_probe_unavailability::response_limit
						: capability_probe_unavailability::
							  malformed_response,
					preflight == json_preflight_result::structural_limit ? application::github_capability_diagnostic::json_limits : application::github_capability_diagnostic::json_shape);
			}

			QJsonParseError error;
			const auto      document = QJsonDocument::fromJson(
				QByteArray{
					value.Body.data(),
					static_cast<qsizetype>(value.Body.size()),
				},
				&error);
			if (
				error.error != QJsonParseError::NoError
				|| !document.isObject()) {
				return unavailable(
					capability_probe_unavailability::malformed_response, application::github_capability_diagnostic::json_shape);
			}
			const auto envelope = document.object();
			if (envelope.contains("errors")) {
				if (
					rateLimit->signals_throttling()
					|| graphql_error_rate_limited(
						envelope.value("errors"))) {
					return unavailable(
						capability_probe_unavailability::rate_limit, application::github_capability_diagnostic::graphql_errors);
				}
				return unavailable(
					capability_probe_unavailability::partial_response, graphql_error_detail(envelope.value("errors")));
			}
			if (
				!envelope.contains("data")
				|| !envelope.value("data").isObject()) {
				return unavailable(
					capability_probe_unavailability::partial_response, application::github_capability_diagnostic::graphql_data);
			}
			for (auto iterator = envelope.begin();
				 iterator != envelope.end();
				 ++iterator) {
				if (
					iterator.key() != QStringLiteral("data")
					&& iterator.key() != QStringLiteral("extensions")) {
					return unavailable(
						capability_probe_unavailability::
							malformed_response,
						application::github_capability_diagnostic::json_shape);
				}
			}

			const auto data = envelope.value("data").toObject();
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
} // namespace bpr::adapters::github::detail
