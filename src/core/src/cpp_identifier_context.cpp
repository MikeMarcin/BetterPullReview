#include "bpr/core/cpp_identifier_context.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::core {
	namespace {
		using role             = core::appearance_role;
		constexpr auto NoIndex = std::numeric_limits<std::uint32_t>::max();
		/// Bounds on anonymous block scopes and on tracked `#if` nesting (H5, H8).
		constexpr auto MaximumBlockDepth       = std::size_t{ 256U };
		constexpr auto MaximumScopes           = std::size_t{ 1U } << 20U;
		constexpr auto MaximumConditionalDepth = std::size_t{ 32U };

		/// Every byte of a UTF-8 sequence continues an identifier, so a non-ASCII
		/// name is one token (H7). Strings and comments never reach the tokenizer.
		[[nodiscard]] auto identifier_start(const char aByte) noexcept -> bool
		{
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || aByte == '_'
				|| static_cast<unsigned char>(aByte) >= 0x80U;
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

		/// \brief The namespaces ISO C++ nests in `std`, and the literal ones.
		///
		/// \details [[BPR-1710]]. `std` is the namespace the language itself
		/// names ([namespace.std]) and a program may not add to it, so this set is
		/// closed by the standard rather than guessed. They are predeclared as if
		/// the blob declared them, so its own declarations, `::std` and a `struct
		/// std` resolve by the ordinary lookup; an unlisted scope under `std`
		/// (`std::string::npos`) is a library class and keeps the grammar's role.
		/// The inline literal namespaces sit under `std` and `std::literals`.
		constexpr auto StandardNamespaces = std::array<std::string_view, 15U>{ "chrono", "execution", "experimental", "filesystem", "linalg", "literals", "meta", "numbers", "placeholders", "pmr", "ranges", "regex_constants", "rel_ops", "this_thread", "views" };
		constexpr auto LiteralNamespaces  = std::array<std::string_view, 4U>{ "chrono_literals", "complex_literals", "string_literals", "string_view_literals" };

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

		enum class directive : std::uint8_t {
			open,
			branch,
			close,
		};

		/// A conditional directive line, at its `#`.
		struct conditional_line
		{
			std::uint32_t Offset = 0U;
			directive     Kind   = directive::open;
		};

		/// The braces of an open `#if` and its first branch's, once it has ended.
		struct branch_state
		{
			std::size_t                Depth     = 0U;
			std::uint32_t              Scope     = 0U;
			bool                       Valid     = true;
			bool                       Stashed   = false;
			std::uint32_t              TailScope = 0U;
			std::vector<std::uint32_t> Tail;
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
				predeclare_standard_namespaces();
			}

			[[nodiscard]] auto refine(token_span_list& aSpans, const std::size_t aMaximumSpans)
				-> syntax_classification_status
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

			void prepare_roles(const token_span_list& aSpans)
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
					const auto opened  = at < end && Text[at] == '#' && (Roles[at] == role::syntax_preprocessor || code_role(Roles[at]));
					const auto protect = continued || opened;
					if (protect) {
						std::fill(Protected.begin() + begin, Protected.begin() + static_cast<std::ptrdiff_t>(end), true);
					}
					if (opened && !continued) {
						record_conditional(at, end);
					}
					auto last = end;
					if (last > begin && Text[last - 1U] == '\r') {
						--last;
					}
					continued = protect && last > begin && Text[last - 1U] == '\\';
				}
			}

			void record_conditional(const std::size_t aHash, const std::size_t aEnd)
			{
				auto at = aHash + 1U;
				while (at < aEnd && (Text[at] == ' ' || Text[at] == '\t')) {
					++at;
				}
				auto end = at;
				while (end < aEnd && identifier_unit(Text[end])) {
					++end;
				}
				const auto name   = Text.substr(at, end - at);
				const auto offset = static_cast<std::uint32_t>(aHash);
				if (name.starts_with("if")) {
					Conditionals.push_back({ .Offset = offset, .Kind = directive::open });
				} else if (name == "else" || name.starts_with("elif")) {
					Conditionals.push_back({ .Offset = offset, .Kind = directive::branch });
				} else if (name == "endif") {
					Conditionals.push_back({ .Offset = offset, .Kind = directive::close });
				}
			}

			void tokenize()
			{
				for (auto at = std::size_t{ 0U }; at < Text.size();) {
					if (Stop.stop_requested()) {
						return;
					}
					const auto byte = Text[at];
					if (Protected[at] || !code_role(Roles[at]) || byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n') {
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

			[[nodiscard]] auto assignment(const std::uint32_t aAt) const noexcept -> bool
			{
				const auto previous = aAt > 0U ? word(aAt - 1U) : std::string_view{ };
				return word(aAt) == "=" && word(aAt + 1U) != "=" && previous != "=" && previous != "!" && previous != "<" && previous != ">";
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
					} else if (text == ";" || text == "{" || text == "}" || assignment(at)) {
						// A template argument list holds no bare `=`, so `A = X < 3, B = Y > 2`
						// is two comparisons rather than one argument list (H3).
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

			[[nodiscard]] auto declare_scope(const std::uint32_t aParent, const std::string_view aName,
											 const symbol_kind aKind, const bool aDefined) -> std::uint32_t
			{
				auto found = Scopes[aParent].Symbols.find(aName);
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
				Scopes[aParent].Symbols.emplace(aName, symbol{ .Kind = aKind, .Child = child, .Defined = aDefined });
				return child;
			}

			void predeclare_standard_namespaces()
			{
				const auto standard = declare_scope(0U, "std", symbol_kind::name_space, false);
				auto       literals = standard;
				for (const auto name : StandardNamespaces) {
					const auto child = declare_scope(standard, name, symbol_kind::name_space, false);
					if (name == "ranges") {
						static_cast<void>(declare_scope(child, "views", symbol_kind::name_space, false));
					}
					literals = name == "literals" ? child : literals;
				}
				for (const auto literal : LiteralNamespaces) {
					static_cast<void>(declare_scope(standard, literal, symbol_kind::name_space, false));
					static_cast<void>(declare_scope(literals, literal, symbol_kind::name_space, false));
				}
			}

			void paint(const std::size_t aAt, const role aRole)
			{
				const auto& item = Tokens[aAt];
				const auto  old  = Roles[item.Offset];
				// The grammar paints a whole `std::` chain built-in; only a namespace,
				// never an inferred role, replaces that (no language type precedes `::`).
				if (old == role::syntax_normal || old == role::syntax_macro || old == role::syntax_type || old == role::syntax_function
					|| (old == role::syntax_builtin && aRole == role::syntax_namespace)) {
					std::fill_n(Roles.begin() + item.Offset, item.Length, aRole);
				}
			}

			void paint_namespace(const std::size_t aAt)
			{
				paint(aAt, role::syntax_namespace);
				separate(aAt);
			}

			/// A `::` the grammar's built-in chain covered is an operator, as elsewhere.
			void separate(const std::size_t aAt)
			{
				if (word(aAt + 1U) == "::" && Roles[Tokens[aAt + 1U].Offset] == role::syntax_builtin) {
					std::fill_n(Roles.begin() + Tokens[aAt + 1U].Offset, Tokens[aAt + 1U].Length, role::syntax_operator);
				}
			}

			/// \brief Records every segment of a using-directive's or a namespace
			/// alias target's name as a namespace ([[BPR-1710]]).
			/// \details Only a namespace may be named there and a class cannot
			/// enclose one. A known segment keeps its identity: a namespace is
			/// entered, anything else ends the record rather than being overridden.
			void nominate_namespaces(std::size_t aAt, const std::uint32_t aScope)
			{
				const auto global = word(aAt) == "::";
				if (global) {
					++aAt;
				}
				auto scope   = global ? 0U : aScope;
				auto parents = !global;
				auto known   = true;
				while (identifier(aAt)) {
					if (Stop.stop_requested()) {
						return;
					}
					paint_namespace(aAt);
					Tokens[aAt].Declaration = true;
					if (known) {
						const auto found = find_symbol(scope, word(aAt), parents);
						if (!found) {
							scope = declare_scope(scope, word(aAt), symbol_kind::name_space, false);
						} else if (found->Kind == symbol_kind::name_space && found->Child != NoIndex) {
							scope = found->Child;
						} else {
							known = false;
						}
						parents = false;
					}
					if (word(aAt + 1U) != "::") {
						return;
					}
					aAt += 2U;
				}
			}

			void namespace_declaration(const std::size_t aAt, const std::uint32_t aScope)
			{
				auto at = aAt + 1U;
				// `namespace [[deprecated]] Legacy {` (H2).
				while (word(at) == "[" && Tokens[at].Match != NoIndex) {
					at = Tokens[at].Match + 1U;
				}
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
					// `namespace A::inline B {` names B; `inline` is not a namespace (H1).
					if (word(at) == "inline" && identifier(at + 1U)) {
						++at;
					}
					names.push_back(at);
				}
				if (word(at + 1U) == "=") {
					paint_namespace(names.front());
					Tokens[names.front()].Declaration = true;
					Scopes[aScope].Symbols.insert_or_assign(word(names.front()), symbol{ .Kind = symbol_kind::namespace_alias });
					nominate_namespaces(at + 2U, aScope);
					return;
				}
				if (word(at + 1U) != "{") {
					return;
				}
				auto parent = aScope;
				for (const auto name : names) {
					paint_namespace(name);
					Tokens[name].Declaration = true;
					parent                   = declare_scope(parent, word(name), symbol_kind::name_space, true);
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
				auto name = identifier(at) ? std::optional{ at } : std::nullopt;
				// `struct std::hash<Key> {` names a member of a scope declared elsewhere:
				// its qualifier is a use and nothing is declared here [[BPR-1711]].
				const auto qualified = name && word(at + 1U) == "::";
				while (name && word(*name + 1U) == "::" && identifier(*name + 2U) && !Stop.stop_requested()) {
					name = *name + 2U;
				}
				at = name.value_or(at);
				if (name) {
					paint(*name, role::syntax_type);
					Tokens[*name].Declaration = true;
					++at;
				}
				// A malformed header cannot force a scan of the rest of the blob.
				const auto limit = std::min(Tokens.size(), at + 256U);
				// An enum head holds no parenthesis, so `f(enum E e) {` names no enum body.
				while (at < limit && word(at) != "{" && word(at) != ";" && word(at) != "=" && word(at) != "}"
					   && !(enumeration && (word(at) == "(" || word(at) == ")"))) {
					++at;
				}
				if (at == limit || (word(at) != "{" && word(at) != ";")) {
					return;
				}
				const auto kind  = enumeration ? symbol_kind::enumeration : symbol_kind::type;
				const auto child = name && !qualified ? declare_scope(aScope, word(*name), kind, word(at) == "{") : new_scope(aScope, enumeration);
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
				// A brace initializer declares like `=` does: `int Reason{ 0 };` (H6), but
				// `class EXPORT_MACRO Name {` is a class head, not a declarator.
				return (previous == "auto" || previous == "using" || previous == "*" || previous == "&"
						|| old == role::syntax_builtin || old == role::syntax_type)
					&& (next == "=" || next == ";" || next == "," || next == ")" || next == "["
						|| (next == "{" && !Tokens[aAt - 1U].Declaration));
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

			/// A macro-styled name before `(` in an enum body is an X-macro invocation,
			/// never an enumerator (H4).
			[[nodiscard]] auto x_macro(const std::size_t aAt) const noexcept -> bool
			{
				return word(aAt + 1U) == "(" && Roles[Tokens[aAt].Offset] == role::syntax_macro;
			}

			/// Whether the source before a `{` is `extern "C"` or `extern "C++"`. The
			/// grammar styles the whole specification, so it supplies no token.
			[[nodiscard]] auto linkage_block(const std::size_t aOffset) const noexcept -> bool
			{
				auto       before = Text.substr(0U, aOffset);
				const auto trim   = [&before] {
					while (!before.empty() && (before.back() == ' ' || before.back() == '\t' || before.back() == '\r' || before.back() == '\n')) {
						before.remove_suffix(1U);
					}
				};
				trim();
				if (!before.ends_with("\"C\"") && !before.ends_with("\"C++\"")) {
					return false;
				}
				before.remove_suffix(before.ends_with("\"C\"") ? 3U : 5U);
				trim();
				return before.ends_with("extern") && (before.size() == 6U || !identifier_unit(before[before.size() - 7U]));
			}

			/// Whether an anonymous `{` opens its own scope. `extern "C" {` is transparent,
			/// and deep or very many blocks share their parent's scope, so 8 MiB of `{`
			/// cannot allocate a scope per byte (H8). Shadows then reach the parent,
			/// which only leaves more names neutral; an enum body keeps its own scope.
			[[nodiscard]] auto block_scope(const std::size_t aAt, const std::uint32_t aCurrent, const std::size_t aDepth) const noexcept -> bool
			{
				if (linkage_block(Tokens[aAt].Offset)) {
					return false;
				}
				return Scopes[aCurrent].EnumBody || (aDepth <= MaximumBlockDepth && Scopes.size() < MaximumScopes);
			}

			/// Each `#else`/`#elif` branch starts from the braces open at its `#if`, and
			/// `#endif` resumes the first branch's, so `enum E {` in both branches opens
			/// one enum (H5). A branch that closes braces opened before its `#if` ends
			/// the tracking of that conditional.
			void conditional(const directive aKind, std::vector<std::uint32_t>& aBraces, std::uint32_t& aCurrent)
			{
				if (aKind == directive::open) {
					Branches.push_back({ .Depth = aBraces.size(), .Scope = aCurrent, .Valid = Branches.size() < MaximumConditionalDepth });
					return;
				}
				if (Branches.empty()) {
					return;
				}
				auto& branch = Branches.back();
				branch.Valid = branch.Valid && aBraces.size() >= branch.Depth;
				if (branch.Valid && aKind == directive::branch && !branch.Stashed) {
					branch.Tail.assign(aBraces.begin() + static_cast<std::ptrdiff_t>(branch.Depth), aBraces.end());
					branch.TailScope = aCurrent;
					branch.Stashed   = true;
				}
				if (branch.Valid && branch.Stashed) {
					aBraces.resize(branch.Depth);
					aCurrent = branch.Scope;
					if (aKind == directive::close) {
						aBraces.insert(aBraces.end(), branch.Tail.begin(), branch.Tail.end());
						aCurrent = branch.TailScope;
					}
				}
				if (aKind == directive::close) {
					Branches.pop_back();
				}
			}

			void collect_declarations()
			{
				auto current     = std::uint32_t{ 0U };
				auto braces      = std::vector<std::uint32_t>{ };
				auto parentheses = 0U;
				auto next        = std::size_t{ 0U };
				for (auto at = std::size_t{ 0U }; at < Tokens.size(); ++at) {
					if (cancelled(at)) {
						return;
					}
					for (; next < Conditionals.size() && Conditionals[next].Offset < Tokens[at].Offset; ++next) {
						conditional(Conditionals[next].Kind, braces, current);
					}
					Tokens[at].Scope = current;
					const auto text  = word(at);
					if (text == "(") {
						++parentheses;
					} else if (text == ")" && parentheses > 0U) {
						--parentheses;
					}
					if (text == "using" && word(at + 1U) == "namespace") {
						nominate_namespaces(at + 2U, current);
					} else if (text == "namespace" && !(at > 0U && word(at - 1U) == "using")) {
						namespace_declaration(at, current);
					} else if ((text == "enum" || text == "class" || text == "struct" || text == "union")
							   && !(at > 0U && word(at - 1U) == "enum")) {
						enum_or_type_declaration(at, current);
					}
					if (text == "{") {
						braces.push_back(current);
						const auto open = Opens.find(at);
						if (open != Opens.end()) {
							current = open->second;
						} else if (block_scope(at, current, braces.size())) {
							current = new_scope(current);
						}
						if (braces.back() != current) {
							Scopes[current].ParenthesisBase = parentheses;
						}
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
							} else if (enumeration.ExpectMember && identifier(at) && !x_macro(at)) {
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
				// After `.` or `->` only a namespace resolves. A class declares none, so a
				// namespace there scopes a base (`p->std::exception::what()`) or starts a
				// trailing return type, which shares the token (`-> std::vector<int>`) [[BPR-1711]].
				const auto member = aFirst > 0U && (word(aFirst - 1U) == "." || word(aFirst - 1U) == "->");
				const auto kept   = [member](const std::optional<symbol>& aFound) {
					return member && aFound && aFound->Kind != symbol_kind::name_space && aFound->Kind != symbol_kind::namespace_alias ? std::nullopt : aFound;
				};
				auto found = kept(find_symbol(global ? 0U : Tokens[aFirst].Scope, word(aFirst), !global));
				// Once a namespace qualifies the chain, every later separator the
				// grammar's built-in chain covered is an operator too.
				auto qualified = false;
				for (auto at = aFirst; at <= aLast; at += 2U) {
					if (Stop.stop_requested()) {
						return;
					}
					if (qualified && at < aLast) {
						separate(at);
					}
					if (found && (found->Kind == symbol_kind::name_space || found->Kind == symbol_kind::namespace_alias)) {
						paint_namespace(at);
						qualified = true;
					} else if (found && found->Kind == symbol_kind::member && at == aLast) {
						paint(at, role::syntax_enum_member);
					} else if (at < aLast || (found && (found->Kind == symbol_kind::enumeration || found->Kind == symbol_kind::type))) {
						paint(at, role::syntax_type);
					} else if (word(at + 1U) != "(" && Roles[Tokens[at].Offset] == role::syntax_macro) {
						paint(at, role::syntax_normal);
					}
					if (at < aLast) {
						found = found && found->Child != NoIndex && found->Kind != symbol_kind::unknown
							? kept(find_symbol(found->Child, word(at + 2U), false))
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

			[[nodiscard]] auto publish(token_span_list& aSpans, const std::size_t aMaximumSpans)
				-> syntax_classification_status
			{
				auto result = token_span_list{ };
				auto line   = std::uint32_t{ 0U };
				for (auto at = std::size_t{ 0U }; at < Text.size();) {
					if (Stop.stop_requested()) {
						aSpans.clear();
						return syntax_classification_status::cancelled;
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
							return syntax_classification_status::cancelled;
						}
						++end;
					}
					if (selected != role::syntax_normal) {
						if (result.size() >= aMaximumSpans) {
							aSpans.clear();
							return syntax_classification_status::limit_exceeded;
						}
						result.push_back({ .Line = line, .Column = static_cast<std::uint32_t>(at) - Lines[line], .Length = static_cast<std::uint32_t>(end - at), .Role = selected });
					}
					at = end;
				}
				aSpans = std::move(result);
				return syntax_classification_status::available;
			}

			std::string_view                               Text;
			std::stop_token                                Stop;
			std::vector<role>                              Roles;
			std::vector<bool>                              Protected;
			std::vector<std::uint32_t>                     Lines;
			std::vector<token>                             Tokens;
			std::vector<scope>                             Scopes;
			std::unordered_map<std::size_t, std::uint32_t> Opens;
			std::vector<conditional_line>                  Conditionals;
			std::vector<branch_state>                      Branches;
		};
	}

	auto refine_cpp_identifiers(const std::string_view aText, token_span_list& aSpans,
								const std::size_t aMaximumSpans, const std::stop_token aStop) -> syntax_classification_status
	{
		return cpp_context{ aText, aStop }.refine(aSpans, aMaximumSpans);
	}
}
