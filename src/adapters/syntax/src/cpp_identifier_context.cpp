#include "cpp_identifier_context.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::adapters::syntax {
	namespace {
		using role             = core::appearance_role;
		constexpr auto NoIndex = std::numeric_limits<std::uint32_t>::max();

		[[nodiscard]] auto identifier_start(const char aByte) noexcept -> bool
		{
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || aByte == '_';
		}

		[[nodiscard]] auto identifier_unit(const char aByte) noexcept -> bool
		{
			return identifier_start(aByte) || (aByte >= '0' && aByte <= '9');
		}

		[[nodiscard]] auto code_role(const role aRole) noexcept -> bool
		{
			return aRole == role::syntax_normal || aRole == role::syntax_keyword
				|| aRole == role::syntax_control_flow || aRole == role::syntax_builtin
				|| aRole == role::syntax_operator || aRole == role::syntax_type
				|| aRole == role::syntax_function || aRole == role::syntax_variable
				|| aRole == role::syntax_macro || aRole == role::syntax_constant
				|| aRole == role::syntax_number;
		}

		struct token
		{
			std::uint32_t Offset      = 0U;
			std::uint32_t Length      = 0U;
			std::uint32_t Scope       = 0U;
			std::uint32_t Match       = NoIndex;
			bool          Declaration = false;
		};

		enum class symbol_kind : std::uint8_t {
			unknown,
			name_space,
			namespace_alias,
			enumeration,
			type,
			member,
		};

		struct symbol
		{
			symbol_kind   Kind    = symbol_kind::unknown;
			std::uint32_t Child   = NoIndex;
			bool          Defined = false;
		};

		struct scope
		{
			std::uint32_t                                Parent = NoIndex;
			std::unordered_map<std::string_view, symbol> Symbols;
			bool                                         EnumBody        = false;
			bool                                         ExpectMember    = true;
			std::size_t                                  ProtectedUntil  = 0U;
			unsigned                                     ParenthesisBase = 0U;
		};

		/// Grammar spans protect strings/comments and directive lines before tokenization.
		/// Facts and offsets refer to the admitted immutable blob; no copied source names
		/// or fully-qualified path strings multiply storage with nesting depth.
		class cpp_context final
		{
		public:
			cpp_context(const std::string_view aText, const std::stop_token aStop)
				: Text{ aText }
				, Stop{ aStop }
				, Roles(aText.size(), role::syntax_normal)
			{
				Scopes.emplace_back();
			}

			[[nodiscard]] auto refine(application::token_span_list& aSpans, const std::size_t aMaximumSpans)
				-> application::syntax_classification_status
			{
				prepare_roles(aSpans);
				tokenize();
				match_delimiters();
				collect_declarations();
				classify_uses();
				return publish(aSpans, aMaximumSpans);
			}

		private:
			[[nodiscard]] auto cancelled(const std::size_t aAt) const noexcept -> bool
			{
				return aAt % 4096U == 0U && Stop.stop_requested();
			}

			[[nodiscard]] auto word(const std::size_t aAt) const noexcept -> std::string_view
			{
				return aAt < Tokens.size() ? Text.substr(Tokens[aAt].Offset, Tokens[aAt].Length) : std::string_view{ };
			}

			[[nodiscard]] auto identifier(const std::size_t aAt) const noexcept -> bool
			{
				const auto text = word(aAt);
				return !text.empty() && identifier_start(text.front());
			}

			void prepare_roles(const application::token_span_list& aSpans)
			{
				Lines.push_back(0U);
				for (auto at = std::size_t{ 0U }; at < Text.size(); ++at) {
					if (cancelled(at)) {
						return;
					}
					if (Text[at] == '\n') {
						Lines.push_back(static_cast<std::uint32_t>(at + 1U));
					}
				}
				for (const auto& span : aSpans) {
					if (Stop.stop_requested()) {
						return;
					}
					const auto first = Lines[span.Line] + span.Column;
					std::fill_n(Roles.begin() + first, span.Length, span.Role);
				}
				// A grammar may style a directive's operands as ordinary identifiers or
				// operators. Protect the complete directive, including continued lines.
				Protected.assign(Text.size(), false);
				auto continued = false;
				for (auto line = std::size_t{ 0U }; line < Lines.size(); ++line) {
					if (Stop.stop_requested()) {
						return;
					}
					const auto begin = Lines[line];
					const auto end   = line + 1U < Lines.size() ? Lines[line + 1U] - 1U : Text.size();
					auto       at    = static_cast<std::size_t>(begin);
					while (at < end && (Text[at] == ' ' || Text[at] == '\t')) {
						++at;
					}
					const auto directive = continued || (at < end && Text[at] == '#' && (Roles[at] == role::syntax_preprocessor || code_role(Roles[at])));
					if (directive) {
						std::fill(Protected.begin() + begin, Protected.begin() + static_cast<std::ptrdiff_t>(end), true);
					}
					auto last = end;
					if (last > begin && Text[last - 1U] == '\r') {
						--last;
					}
					continued = directive && last > begin && Text[last - 1U] == '\\';
				}
			}

			void tokenize()
			{
				for (auto at = std::size_t{ 0U }; at < Text.size();) {
					if (Stop.stop_requested()) {
						return;
					}
					const auto byte = Text[at];
					if (Protected[at] || !code_role(Roles[at]) || byte == ' ' || byte == '\t'
						|| byte == '\r' || byte == '\n' || static_cast<unsigned char>(byte) >= 128U) {
						++at;
						continue;
					}
					auto end = at + 1U;
					if (identifier_start(byte)) {
						while (end < Text.size() && identifier_unit(Text[end]) && !Protected[end] && code_role(Roles[end])) {
							if (cancelled(end)) {
								return;
							}
							++end;
						}
					} else if (end < Text.size() && ((byte == ':' && Text[end] == ':') || (byte == '-' && Text[end] == '>'))) {
						++end;
					}
					Tokens.push_back({ .Offset = static_cast<std::uint32_t>(at), .Length = static_cast<std::uint32_t>(end - at) });
					at = end;
				}
			}

			void match_delimiters()
			{
				auto stack  = std::vector<std::uint32_t>{ };
				auto angles = std::vector<std::uint32_t>{ };
				for (auto at = std::uint32_t{ 0U }; at < Tokens.size(); ++at) {
					if (cancelled(at)) {
						return;
					}
					const auto text = word(at);
					if (text == "<" && at > 0U && (identifier(at - 1U) || word(at - 1U) == ">") && word(at + 1U) != "=") {
						angles.push_back(at);
					} else if (text == ">" && !angles.empty() && word(at + 1U) != "=") {
						Tokens[angles.back()].Match = at;
						angles.pop_back();
					} else if (text == ";" || text == "{" || text == "}") {
						angles.clear();
					}
					if (text == "(" || text == "[" || text == "{") {
						stack.push_back(at);
					} else if (!stack.empty() && ((text == ")" && word(stack.back()) == "(") || (text == "]" && word(stack.back()) == "[") || (text == "}" && word(stack.back()) == "{"))) {
						Tokens[stack.back()].Match = at;
						Tokens[at].Match           = stack.back();
						stack.pop_back();
					}
				}
			}

			[[nodiscard]] auto new_scope(const std::uint32_t aParent, const bool aEnum = false) -> std::uint32_t
			{
				const auto index = static_cast<std::uint32_t>(Scopes.size());
				auto       child = scope{ };
				child.Parent     = aParent;
				child.EnumBody   = aEnum;
				Scopes.push_back(std::move(child));
				return index;
			}

			[[nodiscard]] auto declare_scope(const std::uint32_t aParent, const std::size_t aName,
											 const symbol_kind aKind, const bool aDefined) -> std::uint32_t
			{
				auto found = Scopes[aParent].Symbols.find(word(aName));
				if (found != Scopes[aParent].Symbols.end()) {
					auto& existing = found->second;
					if (existing.Kind == aKind && (aKind == symbol_kind::name_space || !existing.Defined)) {
						existing.Defined = aDefined || existing.Defined;
						return existing.Child;
					}
					existing.Kind = symbol_kind::unknown;
					return new_scope(aParent, aKind == symbol_kind::enumeration);
				}
				const auto child = new_scope(aParent, aKind == symbol_kind::enumeration);
				Scopes[aParent].Symbols.emplace(word(aName), symbol{ .Kind = aKind, .Child = child, .Defined = aDefined });
				return child;
			}

			void paint(const std::size_t aAt, const role aRole)
			{
				const auto& item = Tokens[aAt];
				const auto  old  = Roles[item.Offset];
				if (old == role::syntax_normal || old == role::syntax_macro || old == role::syntax_type || old == role::syntax_function) {
					std::fill_n(Roles.begin() + item.Offset, item.Length, aRole);
				}
			}

			void namespace_declaration(const std::size_t aAt, const std::uint32_t aScope)
			{
				auto at = aAt + 1U;
				if (!identifier(at)) {
					if (word(at) == "{") {
						Opens[at] = new_scope(aScope);
					}
					return;
				}
				auto names = std::vector<std::size_t>{ at };
				while (word(at + 1U) == "::" && identifier(at + 2U)) {
					if (Stop.stop_requested()) {
						return;
					}
					at += 2U;
					names.push_back(at);
				}
				if (word(at + 1U) == "=") {
					paint(names.front(), role::syntax_namespace);
					Tokens[names.front()].Declaration = true;
					Scopes[aScope].Symbols.insert_or_assign(word(names.front()), symbol{ .Kind = symbol_kind::namespace_alias });
					return;
				}
				if (word(at + 1U) != "{") {
					return;
				}
				auto parent = aScope;
				for (const auto name : names) {
					paint(name, role::syntax_namespace);
					Tokens[name].Declaration = true;
					parent                   = declare_scope(parent, name, symbol_kind::name_space, true);
				}
				Opens[at + 1U] = parent;
			}

			void enum_or_type_declaration(const std::size_t aAt, const std::uint32_t aScope)
			{
				if (aAt > 0U && word(aAt - 1U) == "using") {
					return;
				}
				const auto enumeration = word(aAt) == "enum";
				auto       at          = aAt + 1U;
				if (enumeration && (word(at) == "class" || word(at) == "struct")) {
					++at;
				}
				// Skip standard attributes before the declaration name.
				while (word(at) == "[" && Tokens[at].Match != NoIndex) {
					at = Tokens[at].Match + 1U;
				}
				const auto name = identifier(at) ? std::optional{ at } : std::nullopt;
				if (name) {
					paint(*name, role::syntax_type);
					Tokens[*name].Declaration = true;
					++at;
				}
				// A malformed header cannot force a scan of the rest of the blob.
				const auto limit = std::min(Tokens.size(), at + 256U);
				while (at < limit && word(at) != "{" && word(at) != ";" && word(at) != "=" && word(at) != "}") {
					++at;
				}
				if (at == limit || (word(at) != "{" && word(at) != ";")) {
					return;
				}
				const auto kind  = enumeration ? symbol_kind::enumeration : symbol_kind::type;
				const auto child = name ? declare_scope(aScope, *name, kind, word(at) == "{") : new_scope(aScope, enumeration);
				if (word(at) == "{") {
					Opens[at] = child;
				}
			}

			[[nodiscard]] auto looks_like_declarator(const std::size_t aAt) const noexcept -> bool
			{
				if (aAt == 0U || !identifier(aAt) || Tokens[aAt].Declaration) {
					return false;
				}
				const auto previous = word(aAt - 1U);
				const auto next     = word(aAt + 1U);
				const auto old      = Roles[Tokens[aAt - 1U].Offset];
				return (previous == "auto" || previous == "using" || previous == "*" || previous == "&"
						|| old == role::syntax_builtin || old == role::syntax_type)
					&& (next == "=" || next == ";" || next == "," || next == ")" || next == "[");
			}

			void parameter_shadows(const std::size_t aOpen, const std::uint32_t aScope)
			{
				if (aOpen == 0U || word(aOpen - 1U) != ")" || Tokens[aOpen - 1U].Match == NoIndex) {
					return;
				}
				for (auto at = Tokens[aOpen - 1U].Match + 1U; at < aOpen - 1U; ++at) {
					if (looks_like_declarator(at)) {
						Scopes[aScope].Symbols.insert_or_assign(word(at), symbol{ });
					}
				}
			}

			void using_namespace(const std::size_t aAt)
			{
				auto at = aAt + 2U;
				if (word(at) == "::") {
					++at;
				}
				while (identifier(at)) {
					paint(at, role::syntax_namespace);
					Tokens[at].Declaration = true;
					if (word(at + 1U) != "::") {
						break;
					}
					at += 2U;
				}
			}

			void collect_declarations()
			{
				auto current     = std::uint32_t{ 0U };
				auto braces      = std::vector<std::uint32_t>{ };
				auto parentheses = 0U;
				for (auto at = std::size_t{ 0U }; at < Tokens.size(); ++at) {
					if (cancelled(at)) {
						return;
					}
					Tokens[at].Scope = current;
					const auto text  = word(at);
					if (text == "(") {
						++parentheses;
					} else if (text == ")" && parentheses > 0U) {
						--parentheses;
					}
					if (text == "using" && word(at + 1U) == "namespace") {
						using_namespace(at);
					} else if (text == "namespace" && !(at > 0U && word(at - 1U) == "using")) {
						namespace_declaration(at, current);
					} else if ((text == "enum" || text == "class" || text == "struct" || text == "union")
							   && !(at > 0U && word(at - 1U) == "enum")) {
						enum_or_type_declaration(at, current);
					}
					if (text == "{") {
						braces.push_back(current);
						const auto open                 = Opens.find(at);
						current                         = open == Opens.end() ? new_scope(current) : open->second;
						Scopes[current].ParenthesisBase = parentheses;
						parameter_shadows(at, current);
					} else if (text == "}" && !braces.empty()) {
						current = braces.back();
						braces.pop_back();
					} else if (Scopes[current].EnumBody) {
						auto& enumeration = Scopes[current];
						if ((text == "(" || text == "[" || text == "<") && Tokens[at].Match != NoIndex) {
							enumeration.ProtectedUntil = std::max(enumeration.ProtectedUntil, static_cast<std::size_t>(Tokens[at].Match));
						}
						if (at > enumeration.ProtectedUntil) {
							if (text == ",") {
								enumeration.ExpectMember = true;
							} else if (enumeration.ExpectMember && identifier(at)) {
								paint(at, role::syntax_enum_member);
								Tokens[at].Declaration        = true;
								const auto [member, inserted] = enumeration.Symbols.emplace(text, symbol{ .Kind = symbol_kind::member });
								if (!inserted) {
									member->second.Kind = symbol_kind::unknown;
								}
								enumeration.ExpectMember = false;
							}
						}
					} else if (parentheses == Scopes[current].ParenthesisBase && looks_like_declarator(at)) {
						Scopes[current].Symbols.insert_or_assign(text, symbol{ });
					}
				}
			}

			[[nodiscard]] auto find_symbol(std::uint32_t aScope, const std::string_view aName, const bool aParents) const -> std::optional<symbol>
			{
				while (aScope != NoIndex && !Stop.stop_requested()) {
					const auto found = Scopes[aScope].Symbols.find(aName);
					if (found != Scopes[aScope].Symbols.end()) {
						return found->second;
					}
					if (!aParents) {
						break;
					}
					aScope = Scopes[aScope].Parent;
				}
				return std::nullopt;
			}

			void classify_chain(const std::size_t aFirst, const std::size_t aLast)
			{
				const auto global = aFirst > 0U && word(aFirst - 1U) == "::";
				auto       found  = aFirst > 0U && (word(aFirst - 1U) == "." || word(aFirst - 1U) == "->")
					? std::optional<symbol>{ }
					: find_symbol(global ? 0U : Tokens[aFirst].Scope, word(aFirst), !global);
				for (auto at = aFirst; at <= aLast; at += 2U) {
					if (Stop.stop_requested()) {
						return;
					}
					if (found && (found->Kind == symbol_kind::name_space || found->Kind == symbol_kind::namespace_alias)) {
						paint(at, role::syntax_namespace);
					} else if (found && found->Kind == symbol_kind::member && at == aLast) {
						paint(at, role::syntax_enum_member);
					} else if (at < aLast || (found && (found->Kind == symbol_kind::enumeration || found->Kind == symbol_kind::type))) {
						paint(at, role::syntax_type);
					} else if (word(at + 1U) != "(" && Roles[Tokens[at].Offset] == role::syntax_macro) {
						paint(at, role::syntax_normal);
					}
					if (at < aLast) {
						found = found && found->Child != NoIndex && found->Kind != symbol_kind::unknown
							? find_symbol(found->Child, word(at + 2U), false)
							: std::nullopt;
					}
				}
			}

			void classify_uses()
			{
				for (auto at = std::size_t{ 0U }; at < Tokens.size(); ++at) {
					if (cancelled(at)) {
						return;
					}
					if (identifier(at) && !Tokens[at].Declaration && at > 0U && word(at - 1U) == "::"
						&& word(at + 1U) != "::" && word(at + 1U) != "(" && Roles[Tokens[at].Offset] == role::syntax_macro) {
						paint(at, role::syntax_normal);
					}
					if (!identifier(at) || Tokens[at].Declaration || word(at + 1U) != "::") {
						continue;
					}
					auto last = at;
					while (word(last + 1U) == "::" && identifier(last + 2U)) {
						if (Stop.stop_requested()) {
							return;
						}
						last += 2U;
					}
					if (last > at) {
						classify_chain(at, last);
						at = last;
					}
				}
			}

			[[nodiscard]] auto publish(application::token_span_list& aSpans, const std::size_t aMaximumSpans)
				-> application::syntax_classification_status
			{
				auto result = application::token_span_list{ };
				auto line   = std::uint32_t{ 0U };
				for (auto at = std::size_t{ 0U }; at < Text.size();) {
					if (Stop.stop_requested()) {
						aSpans.clear();
						return application::syntax_classification_status::cancelled;
					}
					if (Text[at] == '\n') {
						++line;
						++at;
						continue;
					}
					const auto selected = Roles[at];
					auto       end      = at + 1U;
					while (end < Text.size() && Text[end] != '\n' && Roles[end] == selected) {
						if (cancelled(end)) {
							aSpans.clear();
							return application::syntax_classification_status::cancelled;
						}
						++end;
					}
					if (selected != role::syntax_normal) {
						if (result.size() >= aMaximumSpans) {
							aSpans.clear();
							return application::syntax_classification_status::limit_exceeded;
						}
						result.push_back({ .Line = line, .Column = static_cast<std::uint32_t>(at) - Lines[line], .Length = static_cast<std::uint32_t>(end - at), .Role = selected });
					}
					at = end;
				}
				aSpans = std::move(result);
				return application::syntax_classification_status::available;
			}

			std::string_view                               Text;
			std::stop_token                                Stop;
			std::vector<role>                              Roles;
			std::vector<bool>                              Protected;
			std::vector<std::uint32_t>                     Lines;
			std::vector<token>                             Tokens;
			std::vector<scope>                             Scopes;
			std::unordered_map<std::size_t, std::uint32_t> Opens;
		};
	}

	auto refine_cpp_identifiers(const std::string_view aText, application::token_span_list& aSpans,
								const std::size_t aMaximumSpans, const std::stop_token aStop) -> application::syntax_classification_status
	{
		return cpp_context{ aText, aStop }.refine(aSpans, aMaximumSpans);
	}
}
