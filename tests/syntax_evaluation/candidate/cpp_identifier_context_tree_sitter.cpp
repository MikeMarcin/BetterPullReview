// [[BPR-1442]]. RFC-BPR-0081 candidate T: tree-sitter-cpp as the structural
// front end of the C++ identifier refinement. Evaluation only; never shipped.
//
// KSyntaxHighlighting keeps authority over every role it styles. tree-sitter
// replaces the shipped heuristic's tokenization, delimiter and template-angle
// pairing, header recognition, enumerator boundaries and declarator shadows.
// Resolution stays first-party and follows the shipped rules: the same scope
// and symbol table, the same conservative handling of shadows, aliases,
// conflicts and object or template paths, the same painting precedence, macro
// convention, publication, span limit and cancellation rules.
//
// This file defines `refine_cpp_identifiers` with the shipped signature, so a
// target that links it ahead of the syntax adapter classifies with it through
// the unchanged `packaged_syntax_classifier`. No tree-sitter type leaves it.
// The adapter never calls `ts_set_allocator`.

#include "bpr/core/cpp_identifier_context.hpp"

#include <tree_sitter/api.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" auto tree_sitter_cpp() -> const TSLanguage*;

namespace bpr::core {
	namespace {
		using role             = core::appearance_role;
		constexpr auto NoIndex = std::numeric_limits<std::uint32_t>::max();

#if defined(BPR_TREE_SITTER_FACTS_IN_ERROR) && !BPR_TREE_SITTER_FACTS_IN_ERROR
		/// Error-recovery policy, an evaluation variable. "Strict": declarations
		/// inside tree-sitter ERROR nodes supply no facts. Measured as a
		/// diagnostic; it loses more declarations and gains no accuracy.
		constexpr auto FactsInError = false;
#else
		/// Error-recovery policy, an evaluation variable. "Lenient", the
		/// evaluated T: declarations tree-sitter recovered inside ERROR nodes
		/// still supply facts; MISSING nodes never do.
		constexpr auto FactsInError = true;
#endif

		[[nodiscard]] auto code_role(const role aRole) noexcept -> bool
		{
			return aRole == role::syntax_normal || aRole == role::syntax_keyword
				|| aRole == role::syntax_control_flow || aRole == role::syntax_builtin
				|| aRole == role::syntax_operator || aRole == role::syntax_type
				|| aRole == role::syntax_function || aRole == role::syntax_variable
				|| aRole == role::syntax_macro || aRole == role::syntax_constant
				|| aRole == role::syntax_number;
		}

		[[nodiscard]] auto identifier_start(const char aByte) noexcept -> bool
		{
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || aByte == '_'
				|| static_cast<unsigned char>(aByte) >= 0x80U;
		}

		[[nodiscard]] auto identifier_unit(const char aByte) noexcept -> bool
		{
			return identifier_start(aByte) || (aByte >= '0' && aByte <= '9');
		}

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
			bool                                         EnumBody = false;
		};

		struct name_range
		{
			std::uint32_t Offset = 0U;
			std::uint32_t Length = 0U;
		};

		/// One written qualified chain `A::B::C`, its names in `ChainNames`.
		struct qualified_use
		{
			std::uint32_t First  = 0U;
			std::uint32_t Count  = 0U;
			std::uint32_t Scope  = 0U;
			bool          Global = false;
			bool          Object = false;
		};

		/// The node kinds the walk acts on, resolved once per language.
		struct symbols
		{
			TSSymbol  Error;
			TSSymbol  NamespaceDefinition;
			TSSymbol  NestedNamespaceSpecifier;
			TSSymbol  NamespaceIdentifier;
			TSSymbol  NamespaceAliasDefinition;
			TSSymbol  UsingDeclaration;
			TSSymbol  EnumSpecifier;
			TSSymbol  Enumerator;
			TSSymbol  ClassSpecifier;
			TSSymbol  StructSpecifier;
			TSSymbol  UnionSpecifier;
			TSSymbol  QualifiedIdentifier;
			TSSymbol  TemplateType;
			TSSymbol  FieldExpression;
			TSSymbol  CompoundStatement;
			TSSymbol  FieldDeclarationList;
			TSSymbol  DeclarationList;
			TSSymbol  EnumeratorList;
			TSSymbol  InitializerList;
			TSSymbol  LinkageSpecification;
			TSSymbol  Declaration;
			TSSymbol  FieldDeclaration;
			TSSymbol  FunctionDefinition;
			TSSymbol  LambdaExpression;
			TSSymbol  ParameterDeclaration;
			TSSymbol  OptionalParameterDeclaration;
			TSSymbol  InitDeclarator;
			TSSymbol  PointerDeclarator;
			TSSymbol  ReferenceDeclarator;
			TSSymbol  ArrayDeclarator;
			TSSymbol  AttributedDeclarator;
			TSSymbol  ParenthesizedDeclarator;
			TSSymbol  FunctionDeclarator;
			TSSymbol  AbstractFunctionDeclarator;
			TSSymbol  StructuredBindingDeclarator;
			TSSymbol  Identifier;
			TSSymbol  FieldIdentifier;
			TSSymbol  TypeIdentifier;
			TSSymbol  TemplateDeclaration;
			TSSymbol  TranslationUnit;
			TSFieldId Name;
			TSFieldId Body;
			TSFieldId Scope;
			TSFieldId DeclaratorField;
			TSFieldId Parameters;
			TSFieldId Field;

			explicit symbols(const TSLanguage* aLanguage)
				: Error{ ts_language_symbol_for_name(aLanguage, "ERROR", 5U, true) }
				, NamespaceDefinition{ named(aLanguage, "namespace_definition") }
				, NestedNamespaceSpecifier{ named(aLanguage, "nested_namespace_specifier") }
				, NamespaceIdentifier{ named(aLanguage, "namespace_identifier") }
				, NamespaceAliasDefinition{ named(aLanguage, "namespace_alias_definition") }
				, UsingDeclaration{ named(aLanguage, "using_declaration") }
				, EnumSpecifier{ named(aLanguage, "enum_specifier") }
				, Enumerator{ named(aLanguage, "enumerator") }
				, ClassSpecifier{ named(aLanguage, "class_specifier") }
				, StructSpecifier{ named(aLanguage, "struct_specifier") }
				, UnionSpecifier{ named(aLanguage, "union_specifier") }
				, QualifiedIdentifier{ named(aLanguage, "qualified_identifier") }
				, TemplateType{ named(aLanguage, "template_type") }
				, FieldExpression{ named(aLanguage, "field_expression") }
				, CompoundStatement{ named(aLanguage, "compound_statement") }
				, FieldDeclarationList{ named(aLanguage, "field_declaration_list") }
				, DeclarationList{ named(aLanguage, "declaration_list") }
				, EnumeratorList{ named(aLanguage, "enumerator_list") }
				, InitializerList{ named(aLanguage, "initializer_list") }
				, LinkageSpecification{ named(aLanguage, "linkage_specification") }
				, Declaration{ named(aLanguage, "declaration") }
				, FieldDeclaration{ named(aLanguage, "field_declaration") }
				, FunctionDefinition{ named(aLanguage, "function_definition") }
				, LambdaExpression{ named(aLanguage, "lambda_expression") }
				, ParameterDeclaration{ named(aLanguage, "parameter_declaration") }
				, OptionalParameterDeclaration{ named(aLanguage, "optional_parameter_declaration") }
				, InitDeclarator{ named(aLanguage, "init_declarator") }
				, PointerDeclarator{ named(aLanguage, "pointer_declarator") }
				, ReferenceDeclarator{ named(aLanguage, "reference_declarator") }
				, ArrayDeclarator{ named(aLanguage, "array_declarator") }
				, AttributedDeclarator{ named(aLanguage, "attributed_declarator") }
				, ParenthesizedDeclarator{ named(aLanguage, "parenthesized_declarator") }
				, FunctionDeclarator{ named(aLanguage, "function_declarator") }
				, AbstractFunctionDeclarator{ named(aLanguage, "abstract_function_declarator") }
				, StructuredBindingDeclarator{ named(aLanguage, "structured_binding_declarator") }
				, Identifier{ named(aLanguage, "identifier") }
				, FieldIdentifier{ named(aLanguage, "field_identifier") }
				, TypeIdentifier{ named(aLanguage, "type_identifier") }
				, TemplateDeclaration{ named(aLanguage, "template_declaration") }
				, TranslationUnit{ named(aLanguage, "translation_unit") }
				, Name{ field(aLanguage, "name") }
				, Body{ field(aLanguage, "body") }
				, Scope{ field(aLanguage, "scope") }
				, DeclaratorField{ field(aLanguage, "declarator") }
				, Parameters{ field(aLanguage, "parameters") }
				, Field{ field(aLanguage, "field") }
			{
			}

		private:
			[[nodiscard]] static auto named(const TSLanguage* aLanguage, const char* aName) -> TSSymbol
			{
				return ts_language_symbol_for_name(aLanguage, aName, static_cast<std::uint32_t>(std::strlen(aName)), true);
			}

			[[nodiscard]] static auto field(const TSLanguage* aLanguage, const char* aName) -> TSFieldId
			{
				return ts_language_field_id_for_name(aLanguage, aName, static_cast<std::uint32_t>(std::strlen(aName)));
			}
		};

		[[nodiscard]] auto language_symbols() -> const symbols&
		{
			static const auto instance = symbols{ tree_sitter_cpp() };
			return instance;
		}

		/// Owns the parser and tree of one classification.
		struct parse_owner
		{
			TSParser* Parser = nullptr;
			TSTree*   Tree   = nullptr;

			parse_owner()                                      = default;
			parse_owner(const parse_owner&)                    = delete;
			auto operator=(const parse_owner&) -> parse_owner& = delete;
			parse_owner(parse_owner&&)                         = delete;
			auto operator=(parse_owner&&) -> parse_owner&      = delete;

			~parse_owner()
			{
				if (Tree != nullptr) {
					ts_tree_delete(Tree);
				}
				if (Parser != nullptr) {
					ts_parser_delete(Parser);
				}
			}
		};

		class tree_sitter_context final
		{
		public:
			tree_sitter_context(const std::string_view aText, const std::stop_token aStop)
				: Text{ aText }
				, Stop{ aStop }
				, Roles(aText.size(), role::syntax_normal)
				, Kinds{ language_symbols() }
			{
				Scopes.emplace_back();
			}

			[[nodiscard]] auto refine(core::token_span_list& aSpans, const std::size_t aMaximumSpans)
				-> core::syntax_classification_status
			{
				prepare_roles(aSpans);
				if (Stop.stop_requested()) {
					aSpans.clear();
					return core::syntax_classification_status::cancelled;
				}
				auto owner = parse_owner{ };
				if (!parse(owner)) {
					aSpans.clear();
					return Stop.stop_requested() ? core::syntax_classification_status::cancelled
												 : core::syntax_classification_status::unavailable;
				}
				walk(ts_tree_root_node(owner.Tree));
				resolve_uses();
				unqualify_macros();
				return publish(aSpans, aMaximumSpans);
			}

		private:
			[[nodiscard]] auto cancelled(const std::size_t aAt) const noexcept -> bool
			{
				return aAt % 4096U == 0U && Stop.stop_requested();
			}

			// Identical to the shipped refinement: roles from the grammar, whole
			// directive lines protected including continued lines.
			void prepare_roles(const core::token_span_list& aSpans)
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

			[[nodiscard]] auto parse(parse_owner& aOwner) -> bool
			{
				aOwner.Parser = ts_parser_new();
				if (aOwner.Parser == nullptr || !ts_parser_set_language(aOwner.Parser, tree_sitter_cpp())) {
					return false;
				}
				auto input = TSInput{
					.payload = this,
					.read    = [](void* aPayload, const std::uint32_t aByte, TSPoint, std::uint32_t* aRead) -> const char* {
						const auto& self = *static_cast<const tree_sitter_context*>(aPayload);
						if (aByte >= self.Text.size()) {
							*aRead = 0U;
							return "";
						}
						*aRead = static_cast<std::uint32_t>(self.Text.size() - aByte);
						return self.Text.data() + aByte;
					},
					.encoding = TSInputEncodingUTF8,
					.decode   = nullptr,
				};
				auto options = TSParseOptions{
					.payload           = this,
					.progress_callback = [](TSParseState* aState) -> bool {
						return static_cast<const tree_sitter_context*>(aState->payload)->Stop.stop_requested();
					},
				};
				aOwner.Tree = ts_parser_parse_with_options(aOwner.Parser, nullptr, input, options);
				if (aOwner.Tree == nullptr) {
					// A cancelled parse would otherwise resume on the next call.
					ts_parser_reset(aOwner.Parser);
					return false;
				}
				return true;
			}

			/// A node supplies a fact only for bytes the grammar styles as code
			/// and outside every directive line.
			[[nodiscard]] auto usable(const name_range aName) const noexcept -> bool
			{
				if (aName.Length == 0U || aName.Offset + aName.Length > Text.size()) {
					return false;
				}
				for (auto at = aName.Offset; at < aName.Offset + aName.Length; ++at) {
					if (Protected[at] || !code_role(Roles[at])) {
						return false;
					}
				}
				return true;
			}

			[[nodiscard]] static auto range_of(const TSNode aNode) noexcept -> name_range
			{
				const auto begin = ts_node_start_byte(aNode);
				return { .Offset = begin, .Length = ts_node_end_byte(aNode) - begin };
			}

			[[nodiscard]] auto text_of(const name_range aName) const noexcept -> std::string_view
			{
				return Text.substr(aName.Offset, aName.Length);
			}

			void paint(const name_range aName, const role aRole)
			{
				const auto old = Roles[aName.Offset];
				if (old == role::syntax_normal || old == role::syntax_macro || old == role::syntax_type || old == role::syntax_function) {
					std::fill_n(Roles.begin() + aName.Offset, aName.Length, aRole);
				}
			}

			[[nodiscard]] auto new_scope(const std::uint32_t aParent) -> std::uint32_t
			{
				const auto index = static_cast<std::uint32_t>(Scopes.size());
				auto       child = scope{ };
				child.Parent     = aParent;
				Scopes.push_back(std::move(child));
				return index;
			}

			// The shipped rule: a namespace may be reopened; any other repeated
			// definition, or a change of kind, makes the name unknown.
			[[nodiscard]] auto declare_scope(const std::uint32_t aParent, const std::string_view aName, const symbol_kind aKind, const bool aDefined) -> std::uint32_t
			{
				auto found = Scopes[aParent].Symbols.find(aName);
				if (found != Scopes[aParent].Symbols.end()) {
					auto& existing = found->second;
					if (existing.Kind == aKind && (aKind == symbol_kind::name_space || !existing.Defined)) {
						existing.Defined = aDefined || existing.Defined;
						return existing.Child;
					}
					existing.Kind = symbol_kind::unknown;
					return new_scope(aParent);
				}
				const auto child = new_scope(aParent);
				Scopes[aParent].Symbols.emplace(aName, symbol{ .Kind = aKind, .Child = child, .Defined = aDefined });
				return child;
			}

			// ---------------------------------------------------------------
			// The walk
			// ---------------------------------------------------------------

			/// The walk's record of each open node: the parent facts a node needs
			/// without `ts_node_parent`, which descends from the root each call.
			struct frame
			{
				TSNode        Node{ };
				TSSymbol      Symbol = 0U;
				std::uint32_t Scope  = 0U;
				bool          Error  = false;
			};

			void walk(const TSNode aRoot)
			{
				auto cursor = ts_tree_cursor_new(aRoot);
				auto stack  = std::vector<frame>{ };
				stack.push_back({ .Node = { }, .Symbol = 0U, .Scope = 0U, .Error = false });
				auto visited = std::size_t{ 0U };
				enter(cursor, stack);
				while (true) {
					if (cancelled(++visited)) {
						break;
					}
					if (ts_tree_cursor_goto_first_child(&cursor)) {
						enter(cursor, stack);
						continue;
					}
					auto done = false;
					while (true) {
						stack.pop_back();
						if (ts_tree_cursor_goto_next_sibling(&cursor)) {
							enter(cursor, stack);
							break;
						}
						if (!ts_tree_cursor_goto_parent(&cursor)) {
							done = true;
							break;
						}
					}
					if (done) {
						break;
					}
				}
				ts_tree_cursor_delete(&cursor);
			}

			void enter(const TSTreeCursor& aCursor, std::vector<frame>& aStack)
			{
				const auto aNode  = ts_tree_cursor_current_node(&aCursor);
				const auto field  = ts_tree_cursor_current_field_id(&aCursor);
				const auto parent = aStack.back();
				auto       next   = parent;
				const auto kind   = ts_node_symbol(aNode);
				next.Node         = aNode;
				next.Symbol       = kind;
				if ((FactsInError ? false : kind == Kinds.Error) || ts_node_is_missing(aNode)) {
					next.Error = true;
				}
				// A body this node's parent named opens the scope recorded for it.
				const auto pending = Pending.find(aNode.id);
				if (pending != Pending.end()) {
					next.Scope = pending->second;
					Pending.erase(pending);
				} else if (kind == Kinds.CompoundStatement || kind == Kinds.FieldDeclarationList
						   || kind == Kinds.EnumeratorList || kind == Kinds.InitializerList
						   || (kind == Kinds.DeclarationList && parent.Symbol != Kinds.LinkageSpecification)) {
					next.Scope = new_scope(next.Scope);
				}
				const auto shadows = PendingShadows.find(aNode.id);
				if (shadows != PendingShadows.end()) {
					for (const auto name : shadows->second) {
						Scopes[next.Scope].Symbols.insert_or_assign(text_of(name), symbol{ });
					}
					PendingShadows.erase(shadows);
				}
				aStack.push_back(next);
				if (next.Error) {
					// Inside error recovery a node supplies no declaration, but the
					// qualified uses it holds are still classified conservatively.
					if (kind == Kinds.QualifiedIdentifier) {
						qualified(aNode, next.Scope, parent, field);
					}
					return;
				}
				if (kind == Kinds.NamespaceDefinition) {
					namespace_definition(aNode, next.Scope);
				} else if (kind == Kinds.NamespaceAliasDefinition) {
					namespace_alias(aNode, next.Scope);
				} else if (kind == Kinds.UsingDeclaration) {
					using_declaration(aNode, next.Scope);
				} else if (kind == Kinds.EnumSpecifier) {
					type_specifier(aNode, next.Scope, symbol_kind::enumeration);
				} else if (kind == Kinds.ClassSpecifier || kind == Kinds.StructSpecifier || kind == Kinds.UnionSpecifier) {
					type_specifier(aNode, next.Scope, symbol_kind::type);
				} else if (kind == Kinds.Enumerator) {
					enumerator(aNode, next.Scope);
				} else if (kind == Kinds.QualifiedIdentifier) {
					qualified(aNode, next.Scope, parent, field);
				} else if (kind == Kinds.Declaration || kind == Kinds.FieldDeclaration) {
					declaration_shadows(aNode, next.Scope, parent.Symbol);
				} else if (kind == Kinds.FunctionDefinition || kind == Kinds.LambdaExpression) {
					parameter_shadows(aNode);
				}
			}

			void namespace_definition(const TSNode aNode, const std::uint32_t aScope)
			{
				const auto body  = ts_node_child_by_field_id(aNode, Kinds.Body);
				const auto name  = ts_node_child_by_field_id(aNode, Kinds.Name);
				auto       names = std::vector<name_range>{ };
				if (!ts_node_is_null(name)) {
					namespace_names(name, names);
				}
				auto parent = aScope;
				if (names.empty()) {
					parent = new_scope(aScope);
				}
				for (const auto item : names) {
					if (!usable(item)) {
						parent = new_scope(parent);
						continue;
					}
					paint(item, role::syntax_namespace);
					parent = declare_scope(parent, text_of(item), symbol_kind::name_space, true);
				}
				if (!ts_node_is_null(body)) {
					Pending.insert_or_assign(body.id, parent);
				}
			}

			void namespace_names(const TSNode aName, std::vector<name_range>& aNames) const
			{
				if (ts_node_symbol(aName) == Kinds.NamespaceIdentifier) {
					aNames.push_back(range_of(aName));
					return;
				}
				const auto count = ts_node_named_child_count(aName);
				for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
					const auto child = ts_node_named_child(aName, index);
					if (ts_node_symbol(child) == Kinds.NamespaceIdentifier || ts_node_symbol(child) == Kinds.NestedNamespaceSpecifier) {
						namespace_names(child, aNames);
					}
				}
			}

			void namespace_alias(const TSNode aNode, const std::uint32_t aScope)
			{
				const auto name = ts_node_child_by_field_id(aNode, Kinds.Name);
				if (ts_node_is_null(name) || !usable(range_of(name))) {
					return;
				}
				paint(range_of(name), role::syntax_namespace);
				Scopes[aScope].Symbols.insert_or_assign(text_of(range_of(name)), symbol{ .Kind = symbol_kind::namespace_alias });
				// The target is an ordinary qualified chain, as in the shipped rule.
				const auto count = ts_node_named_child_count(aNode);
				for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
					const auto child = ts_node_named_child(aNode, index);
					if (ts_node_symbol(child) == Kinds.NestedNamespaceSpecifier) {
						auto names = std::vector<name_range>{ };
						namespace_names(child, names);
						record_chain(names, aScope, false, false);
					}
				}
			}

			void using_declaration(const TSNode aNode, const std::uint32_t)
			{
				auto       directive = false;
				const auto count     = ts_node_child_count(aNode);
				for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
					const auto child = ts_node_child(aNode, index);
					if (!ts_node_is_named(child) && std::string_view{ ts_node_type(child) } == "namespace") {
						directive = true;
					}
				}
				if (!directive) {
					return;
				}
				for (auto index = std::uint32_t{ 0U }; index < ts_node_named_child_count(aNode); ++index) {
					const auto child = ts_node_named_child(aNode, index);
					auto       names = std::vector<name_range>{ };
					flatten_names(child, names);
					for (const auto item : names) {
						if (usable(item)) {
							paint(item, role::syntax_namespace);
							Declared.push_back(item.Offset);
						}
					}
				}
			}

			/// Every identifier-like name of a `using namespace` operand, in order.
			void flatten_names(const TSNode aNode, std::vector<name_range>& aNames) const
			{
				const auto kind = ts_node_symbol(aNode);
				if (kind == Kinds.Identifier || kind == Kinds.NamespaceIdentifier || kind == Kinds.TypeIdentifier) {
					aNames.push_back(range_of(aNode));
					return;
				}
				for (auto index = std::uint32_t{ 0U }; index < ts_node_named_child_count(aNode); ++index) {
					flatten_names(ts_node_named_child(aNode, index), aNames);
				}
			}

			void type_specifier(const TSNode aNode, const std::uint32_t aScope, const symbol_kind aKind)
			{
				const auto name   = ts_node_child_by_field_id(aNode, Kinds.Name);
				const auto body   = ts_node_child_by_field_id(aNode, Kinds.Body);
				const auto simple = !ts_node_is_null(name) && ts_node_symbol(name) == Kinds.TypeIdentifier && usable(range_of(name));
				if (simple) {
					paint(range_of(name), role::syntax_type);
				}
				if (ts_node_is_null(body)) {
					return;
				}
				const auto child = simple ? declare_scope(aScope, text_of(range_of(name)), aKind, true) : new_scope(aScope);
				Pending.insert_or_assign(body.id, child);
				if (aKind == symbol_kind::enumeration) {
					Scopes[child].EnumBody = true;
				}
			}

			void enumerator(const TSNode aNode, const std::uint32_t aScope)
			{
				const auto name = ts_node_child_by_field_id(aNode, Kinds.Name);
				if (ts_node_is_null(name) || !usable(range_of(name)) || !Scopes[aScope].EnumBody) {
					return;
				}
				paint(range_of(name), role::syntax_enum_member);
				Declared.push_back(range_of(name).Offset);
				const auto [member, inserted] = Scopes[aScope].Symbols.emplace(text_of(range_of(name)), symbol{ .Kind = symbol_kind::member });
				if (!inserted) {
					member->second.Kind = symbol_kind::unknown;
				}
			}

			/// The identifier a declarator declares, through init, pointer,
			/// reference, array, attribute and parenthesis wrappers. A function
			/// declarator declares a function, which shadows nothing.
			[[nodiscard]] auto declared_name(TSNode aDeclarator) const -> std::optional<name_range>
			{
				for (auto depth = 0; depth < 64 && !ts_node_is_null(aDeclarator); ++depth) {
					const auto kind = ts_node_symbol(aDeclarator);
					if (kind == Kinds.Identifier || kind == Kinds.FieldIdentifier) {
						return range_of(aDeclarator);
					}
					if (kind == Kinds.FunctionDeclarator || kind == Kinds.AbstractFunctionDeclarator) {
						return std::nullopt;
					}
					if (kind == Kinds.InitDeclarator || kind == Kinds.PointerDeclarator || kind == Kinds.ArrayDeclarator) {
						aDeclarator = ts_node_child_by_field_id(aDeclarator, Kinds.DeclaratorField);
					} else if (kind == Kinds.ReferenceDeclarator || kind == Kinds.AttributedDeclarator || kind == Kinds.ParenthesizedDeclarator) {
						aDeclarator = ts_node_named_child(aDeclarator, 0U);
					} else {
						return std::nullopt;
					}
				}
				return std::nullopt;
			}

			void shadow(const TSNode aDeclarator, const std::uint32_t aScope)
			{
				if (ts_node_symbol(aDeclarator) == Kinds.StructuredBindingDeclarator) {
					for (auto index = std::uint32_t{ 0U }; index < ts_node_named_child_count(aDeclarator); ++index) {
						const auto item = ts_node_named_child(aDeclarator, index);
						if (ts_node_symbol(item) == Kinds.Identifier && usable(range_of(item))) {
							Scopes[aScope].Symbols.insert_or_assign(text_of(range_of(item)), symbol{ });
						}
					}
					return;
				}
				const auto name = declared_name(aDeclarator);
				if (name && usable(*name)) {
					Scopes[aScope].Symbols.insert_or_assign(text_of(*name), symbol{ });
				}
			}

			// A declaration directly in a braced scope shadows the names it
			// declares there; declarations inside parentheses (conditions and
			// for-initializers) do not, as in the shipped rule.
			void declaration_shadows(const TSNode aNode, const std::uint32_t aScope, const TSSymbol aContainer)
			{
				const auto container = aContainer;
				if (container != Kinds.CompoundStatement && container != Kinds.DeclarationList && container != Kinds.FieldDeclarationList
					&& container != Kinds.TranslationUnit && container != Kinds.TemplateDeclaration) {
					return;
				}
				const auto count = ts_node_child_count(aNode);
				for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
					const auto* fieldName = ts_node_field_name_for_child(aNode, index);
					if (fieldName != nullptr && std::string_view{ fieldName } == "declarator") {
						shadow(ts_node_child(aNode, index), aScope);
					}
				}
			}

			// Parameters shadow inside the body they belong to.
			void parameter_shadows(const TSNode aNode)
			{
				const auto body = ts_node_child_by_field_id(aNode, Kinds.Body);
				if (ts_node_is_null(body)) {
					return;
				}
				auto declarator = ts_node_child_by_field_id(aNode, Kinds.DeclaratorField);
				for (auto depth = 0; depth < 64 && !ts_node_is_null(declarator); ++depth) {
					const auto kind = ts_node_symbol(declarator);
					if (kind == Kinds.FunctionDeclarator || kind == Kinds.AbstractFunctionDeclarator) {
						break;
					}
					declarator = kind == Kinds.ReferenceDeclarator || kind == Kinds.AttributedDeclarator || kind == Kinds.ParenthesizedDeclarator
						? ts_node_named_child(declarator, 0U)
						: ts_node_child_by_field_id(declarator, Kinds.DeclaratorField);
				}
				if (ts_node_is_null(declarator)) {
					return;
				}
				const auto parameters = ts_node_child_by_field_id(declarator, Kinds.Parameters);
				if (ts_node_is_null(parameters)) {
					return;
				}
				auto names = std::vector<name_range>{ };
				for (auto index = std::uint32_t{ 0U }; index < ts_node_named_child_count(parameters); ++index) {
					const auto parameter = ts_node_named_child(parameters, index);
					const auto kind      = ts_node_symbol(parameter);
					if (kind != Kinds.ParameterDeclaration && kind != Kinds.OptionalParameterDeclaration) {
						continue;
					}
					const auto name = declared_name(ts_node_child_by_field_id(parameter, Kinds.DeclaratorField));
					if (name && usable(*name)) {
						names.push_back(*name);
					}
				}
				if (!names.empty()) {
					PendingShadows.insert_or_assign(body.id, std::move(names));
				}
			}

			// ---------------------------------------------------------------
			// Qualified uses
			// ---------------------------------------------------------------

			void qualified(const TSNode aNode, const std::uint32_t aScope, const frame& aParent, const TSFieldId aField)
			{
				// Only the outermost node of a chain is a use; the chain's own
				// nested qualified_identifier is its tail.
				if (aParent.Symbol == Kinds.QualifiedIdentifier && aField == Kinds.Name) {
					return;
				}
				const auto object = aParent.Symbol == Kinds.FieldExpression && aField == Kinds.Field;
				auto       names  = std::vector<name_range>{ };
				auto       global = ts_node_is_null(ts_node_child_by_field_id(aNode, Kinds.Scope));
				auto       node   = aNode;
				for (auto depth = 0; depth < 4096 && !ts_node_is_null(node); ++depth) {
					if (ts_node_symbol(node) != Kinds.QualifiedIdentifier) {
						const auto kind = ts_node_symbol(node);
						if (kind == Kinds.Identifier || kind == Kinds.TypeIdentifier || kind == Kinds.FieldIdentifier || kind == Kinds.NamespaceIdentifier) {
							names.push_back(range_of(node));
						}
						break;
					}
					const auto scope = ts_node_child_by_field_id(node, Kinds.Scope);
					if (!ts_node_is_null(scope)) {
						if (ts_node_symbol(scope) == Kinds.NamespaceIdentifier) {
							names.push_back(range_of(scope));
						} else {
							// A template, dependent or decltype qualifier ends what
							// the blob can resolve: the template's name closes the
							// chain written before it, and nothing after it is
							// resolved.
							if (ts_node_symbol(scope) == Kinds.TemplateType) {
								const auto templateName = ts_node_child_by_field_id(scope, Kinds.Name);
								if (!ts_node_is_null(templateName)) {
									names.push_back(range_of(templateName));
								}
							}
							record_chain(names, aScope, global, object);
							return;
						}
					}
					node = ts_node_child_by_field_id(node, Kinds.Name);
				}
				record_chain(names, aScope, global, object);
			}

			void record_chain(const std::vector<name_range>& aNames, const std::uint32_t aScope, const bool aGlobal, const bool aObject)
			{
				// A chain counts only as far as the text agrees with the tree: each
				// name usable, and only white space, comments or directives
				// between it and the next name's `::`. Error recovery can form a
				// qualified node across names the source never joined.
				auto usableNames = std::size_t{ 0U };
				for (const auto item : aNames) {
					if (!usable(item) || (usableNames > 0U && !joined(aNames[usableNames - 1U], item))) {
						break;
					}
					++usableNames;
				}
				if (usableNames < 2U) {
					return;
				}
				Uses.push_back({ .First = static_cast<std::uint32_t>(ChainNames.size()), .Count = static_cast<std::uint32_t>(usableNames), .Scope = aScope, .Global = aGlobal, .Object = aObject });
				ChainNames.insert(ChainNames.end(), aNames.begin(), aNames.begin() + static_cast<std::ptrdiff_t>(usableNames));
			}

			/// Whether the source joins two names with `::` and nothing else that
			/// the grammar styles as code.
			[[nodiscard]] auto joined(const name_range aLeft, const name_range aRight) const noexcept -> bool
			{
				const auto at = next_code_position(aLeft.Offset + aLeft.Length);
				return at + 1U < Text.size() && Text[at] == ':' && Text[at + 1U] == ':' && next_code_position(at + 2U) == aRight.Offset;
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

			/// The first code byte after `aOffset`, skipping white space and
			/// anything the grammar does not style as code.
			[[nodiscard]] auto next_code_position(std::size_t aOffset) const noexcept -> std::size_t
			{
				while (aOffset < Text.size()) {
					const auto byte = Text[aOffset];
					if (!Protected[aOffset] && code_role(Roles[aOffset]) && byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') {
						return aOffset;
					}
					++aOffset;
				}
				return Text.size();
			}

			[[nodiscard]] auto next_code_byte(const std::size_t aOffset) const noexcept -> char
			{
				const auto at = next_code_position(aOffset);
				return at < Text.size() ? Text[at] : '\0';
			}

			// The shipped `classify_chain`, over written names instead of tokens.
			void resolve_uses()
			{
				for (auto index = std::size_t{ 0U }; index < Uses.size(); ++index) {
					if (cancelled(index)) {
						return;
					}
					const auto& use   = Uses[index];
					const auto  first = use.First;
					const auto  last  = use.First + use.Count - 1U;
					auto        found = use.Object ? std::optional<symbol>{ } : find_symbol(use.Global ? 0U : use.Scope, text_of(ChainNames[first]), !use.Global);
					for (auto at = first; at <= last; ++at) {
						const auto name = ChainNames[at];
						if (found && (found->Kind == symbol_kind::name_space || found->Kind == symbol_kind::namespace_alias)) {
							paint(name, role::syntax_namespace);
						} else if (found && found->Kind == symbol_kind::member && at == last) {
							paint(name, role::syntax_enum_member);
						} else if (at < last || (found && (found->Kind == symbol_kind::enumeration || found->Kind == symbol_kind::type))) {
							paint(name, role::syntax_type);
						} else if (next_code_byte(name.Offset + name.Length) != '(' && Roles[name.Offset] == role::syntax_macro) {
							paint(name, role::syntax_normal);
						}
						if (at < last) {
							found = found && found->Child != NoIndex && found->Kind != symbol_kind::unknown
								? find_symbol(found->Child, text_of(ChainNames[at + 1U]), false)
								: std::nullopt;
						}
					}
				}
			}

			// The shipped macro convention, which is lexical: a name written
			// after `::`, neither qualifying nor invoked, is not a macro. This
			// holds also where error recovery formed no qualified node.
			void unqualify_macros()
			{
				std::sort(Declared.begin(), Declared.end());
				for (auto at = std::size_t{ 0U }; at + 1U < Text.size(); ++at) {
					if (cancelled(at)) {
						return;
					}
					if (Text[at] != ':' || Text[at + 1U] != ':' || Protected[at] || !code_role(Roles[at]) || !code_role(Roles[at + 1U])) {
						continue;
					}
					const auto begin = next_code_position(at + 2U);
					if (begin >= Text.size() || !identifier_start(Text[begin]) || Roles[begin] != role::syntax_macro) {
						at += 1U;
						continue;
					}
					auto end = begin + 1U;
					while (end < Text.size() && identifier_unit(Text[end]) && Roles[end] == role::syntax_macro && !Protected[end]) {
						++end;
					}
					const auto after   = next_code_position(end);
					const auto invoked = after < Text.size() && Text[after] == '(';
					const auto scoping = after + 1U < Text.size() && Text[after] == ':' && Text[after + 1U] == ':';
					if (!invoked && !scoping && !std::binary_search(Declared.begin(), Declared.end(), static_cast<std::uint32_t>(begin))) {
						paint({ .Offset = static_cast<std::uint32_t>(begin), .Length = static_cast<std::uint32_t>(end - begin) }, role::syntax_normal);
					}
					at = end - 1U;
				}
			}

			// Identical to the shipped publication.
			[[nodiscard]] auto publish(core::token_span_list& aSpans, const std::size_t aMaximumSpans)
				-> core::syntax_classification_status
			{
				auto result = core::token_span_list{ };
				auto line   = std::uint32_t{ 0U };
				for (auto at = std::size_t{ 0U }; at < Text.size();) {
					if (Stop.stop_requested()) {
						aSpans.clear();
						return core::syntax_classification_status::cancelled;
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
							return core::syntax_classification_status::cancelled;
						}
						++end;
					}
					if (selected != role::syntax_normal) {
						if (result.size() >= aMaximumSpans) {
							aSpans.clear();
							return core::syntax_classification_status::limit_exceeded;
						}
						result.push_back({ .Line = line, .Column = static_cast<std::uint32_t>(at) - Lines[line], .Length = static_cast<std::uint32_t>(end - at), .Role = selected });
					}
					at = end;
				}
				aSpans = std::move(result);
				return core::syntax_classification_status::available;
			}

			std::string_view                                         Text;
			std::stop_token                                          Stop;
			std::vector<role>                                        Roles;
			std::vector<bool>                                        Protected;
			std::vector<std::uint32_t>                               Lines;
			std::vector<scope>                                       Scopes;
			std::vector<qualified_use>                               Uses;
			std::vector<name_range>                                  ChainNames;
			std::vector<std::uint32_t>                               Declared;
			std::unordered_map<const void*, std::uint32_t>           Pending;
			std::unordered_map<const void*, std::vector<name_range>> PendingShadows;
			const symbols&                                           Kinds;
		};
	}

	auto refine_cpp_identifiers(const std::string_view aText, core::token_span_list& aSpans,
								const std::size_t aMaximumSpans, const std::stop_token aStop) -> core::syntax_classification_status
	{
		return tree_sitter_context{ aText, aStop }.refine(aSpans, aMaximumSpans);
	}
}
