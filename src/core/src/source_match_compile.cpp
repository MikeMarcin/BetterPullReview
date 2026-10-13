#include "source_matcher_internal.hpp"

#include <array>
#include <new>

namespace bpr::core::source_matching {
	namespace {
		enum class node_kind { empty,
							   character,
							   any,
							   character_class,
							   assertion,
							   sequence,
							   alternate,
							   repeat };
		struct node
		{
			node_kind     Kind    = node_kind::empty;
			std::uint32_t Child   = 0U;
			std::uint32_t Next    = 0U;
			char32_t      Value   = 0U;
			std::uint32_t Minimum = 0U;
			std::uint32_t Maximum = 0U;
			bool          Flag    = false;
		};
		struct flags
		{
			bool Fold      = false;
			bool Multiline = true;
			bool DotAll    = false;
			bool Ungreedy  = false;
		};
		class parser
		{
		public:
			parser(std::string_view aText, source_match_program::implementation& aProgram, budget& aBudget)
				: Text(aText)
				, Program(aProgram)
				, Budget(aBudget)
			{
				Nodes.reserve(Program.Limits.MaximumInstructions);
				Nodes.push_back({ });
				Flags.Fold = !Program.Options.MatchCase;
			}
			auto parse() -> std::uint32_t
			{
				const auto root = expression(0U);
				if (Position != Text.size()) {
					fail();
				}
				return emit(root, add_instruction({ .Op = opcode::accept }));
			}

		private:
			std::string_view                      Text;
			std::size_t                           Position = 0U;
			std::vector<node>                     Nodes;
			source_match_program::implementation& Program;
			budget&                               Budget;
			flags                                 Flags;
			[[noreturn]] void                     fail(source_search_status aStatus = source_search_status::invalid_pattern) const { throw stopped{ aStatus, Position }; }
			auto                                  take(char aValue) -> bool
			{
				if (Position < Text.size() && Text[Position] == aValue) {
					++Position;
					Budget.tick();
					return true;
				}
				return false;
			}
			auto add_node(node aNode) -> std::uint32_t
			{
				Budget.tick();
				if (Nodes.size() >= Program.Limits.MaximumInstructions) {
					fail(source_search_status::resource_limit);
				}
				Nodes.push_back(aNode);
				return static_cast<std::uint32_t>(Nodes.size() - 1U);
			}
			auto add_instruction(instruction aInstruction) -> std::uint32_t
			{
				Budget.tick();
				if (Program.Code.size() >= Program.Limits.MaximumInstructions) {
					fail(source_search_status::resource_limit);
				}
				Program.Code.push_back(aInstruction);
				return static_cast<std::uint32_t>(Program.Code.size() - 1U);
			}
			auto expression(std::uint32_t aDepth) -> std::uint32_t
			{
				if (aDepth > 64U) {
					fail(source_search_status::resource_limit);
				}
				const auto first = sequence(aDepth);
				auto       tail  = first;
				while (take('|')) {
					const auto branch = sequence(aDepth);
					Nodes[tail].Next  = branch;
					tail              = branch;
				}
				return tail == first ? first : add_node({ .Kind = node_kind::alternate, .Child = first });
			}
			auto sequence(std::uint32_t aDepth) -> std::uint32_t
			{
				auto first = std::uint32_t{ 0U };
				auto tail  = first;
				while (Position < Text.size() && Text[Position] != ')' && Text[Position] != '|') {
					const auto item = repetition(atom(aDepth));
					if (tail != 0U) {
						Nodes[tail].Next = item;
					} else {
						first = item;
					}
					tail = item;
				}
				return add_node({ .Kind = node_kind::sequence, .Child = first });
			}
			auto count() -> std::uint32_t
			{
				auto       value = 0U;
				const auto begin = Position;
				while (Position < Text.size() && Text[Position] >= '0' && Text[Position] <= '9') {
					Budget.tick();
					value = value * 10U + static_cast<unsigned int>(Text[Position++] - '0');
					if (value > 1000U) {
						fail();
					}
				}
				if (begin == Position) {
					fail();
				}
				return value;
			}
			auto repetition(std::uint32_t aAtom) -> std::uint32_t
			{
				auto minimum = 0U;
				auto maximum = 0U;
				if (take('*')) {
					maximum = 1001U;
				} else if (take('+')) {
					minimum = 1U;
					maximum = 1001U;
				} else if (take('?')) {
					maximum = 1U;
				} else if (take('{')) {
					minimum = count();
					maximum = minimum;
					if (take(',')) {
						maximum = Position < Text.size() && Text[Position] == '}' ? 1001U : count();
					}
					if (!take('}') || maximum < minimum) {
						fail();
					}
				} else {
					return aAtom;
				}
				auto lazy = Flags.Ungreedy;
				if (take('?')) {
					lazy = !lazy;
				}
				if (Position < Text.size() && (Text[Position] == '*' || Text[Position] == '+' || Text[Position] == '?' || Text[Position] == '{')) {
					fail();
				}
				return add_node({ .Kind = node_kind::repeat, .Child = aAtom, .Minimum = minimum, .Maximum = maximum, .Flag = lazy });
			}
			auto category_mask(std::string_view aName) -> std::uint32_t
			{
				constexpr auto names = std::array<std::string_view, 30>{ "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd", "Nl", "No", "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Sm", "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co", "Cn" };
				auto           mask  = std::uint32_t{ 0U };
				for (std::size_t index = 0U; index < names.size(); ++index) {
					if (names[index] == aName || (aName.size() == 1U && names[index].front() == aName.front())) {
						mask |= 1U << index;
					}
				}
				if (aName == "Any") {
					mask = 0x3FFFFFFFU;
				}
				if (mask == 0U) {
					fail(source_search_status::unsupported_pattern);
				}
				return mask;
			}
			auto class_node(character_class aClass) -> std::uint32_t
			{
				if (Program.Classes.size() >= Program.Limits.MaximumInstructions) {
					fail(source_search_status::resource_limit);
				}
				aClass.Fold = Flags.Fold;
				Program.Classes.push_back(std::move(aClass));
				return add_node({ .Kind = node_kind::character_class, .Value = static_cast<char32_t>(Program.Classes.size() - 1U) });
			}
			auto escape(bool aInClass) -> node
			{
				if (Position == Text.size()) {
					fail();
				}
				const auto value = Text[Position++];
				Budget.tick();
				if (value >= '1' && value <= '9') {
					fail(source_search_status::unsupported_pattern);
				}
				if (value == 'd' || value == 'D' || value == 's' || value == 'S' || value == 'w' || value == 'W' || value == 'p' || value == 'P') {
					auto set    = character_class{ };
					set.Negated = value == 'D' || value == 'S' || value == 'W' || value == 'P';
					if (value == 'd' || value == 'D') {
						set.Ranges = { { U'0', U'9' } };
					} else if (value == 's' || value == 'S') {
						set.Ranges = { { U'\t', U'\n' }, { U'\f', U'\r' }, { U' ', U' ' } };
					} else if (value == 'w' || value == 'W') {
						set.Ranges = { { U'0', U'9' }, { U'A', U'Z' }, { U'_', U'_' }, { U'a', U'z' } };
					} else {
						const auto braced = take('{');
						const auto begin  = Position;
						if (braced) {
							while (Position < Text.size() && Text[Position] != '}') {
								Budget.tick();
								++Position;
							}
						} else if (Position < Text.size()) {
							++Position;
						}
						if (Position == begin) {
							fail();
						}
						set.Categories = category_mask(Text.substr(begin, Position - begin));
						if (braced && !take('}')) {
							fail();
						}
					}
					const auto index = class_node(std::move(set));
					return Nodes[index];
				}
				if (!aInClass && (value == 'A' || value == 'z' || value == 'b' || value == 'B')) {
					const auto op = value == 'A' ? opcode::text_begin : value == 'z' ? opcode::text_end
						: value == 'b'                                               ? opcode::boundary
																					 : opcode::not_boundary;
					return { .Kind = node_kind::assertion, .Value = static_cast<char32_t>(op) };
				}
				if (value == 'n') {
					return { .Kind = node_kind::character, .Value = U'\n', .Flag = Flags.Fold };
				}
				if (value == 'r') {
					return { .Kind = node_kind::character, .Value = U'\r', .Flag = Flags.Fold };
				}
				if (value == 't') {
					return { .Kind = node_kind::character, .Value = U'\t', .Flag = Flags.Fold };
				}
				if (value == 'f') {
					return { .Kind = node_kind::character, .Value = U'\f', .Flag = Flags.Fold };
				}
				if (value == 'a') {
					return { .Kind = node_kind::character, .Value = U'\a', .Flag = Flags.Fold };
				}
				if (value == 'x') {
					const auto braced = take('{');
					auto       cp     = char32_t{ 0U };
					auto       digits = 0U;
					while (Position < Text.size() && (braced ? Text[Position] != '}' : digits < 2U)) {
						const auto ch = Text[Position++];
						Budget.tick();
						const auto digit = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
							: ch >= 'A' && ch <= 'F'                                                  ? ch - 'A' + 10
																									  : -1;
						if (digit < 0 || ++digits > 6U) {
							fail();
						}
						cp = cp * 16U + static_cast<char32_t>(digit);
					}
					if ((braced && (!take('}') || digits == 0U)) || (!braced && digits != 2U) || cp > 0x10FFFFU || (cp >= 0xD800U && cp <= 0xDFFFU)) {
						fail();
					}
					return { .Kind = node_kind::character, .Value = cp, .Flag = Flags.Fold };
				}
				if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9')) {
					fail(source_search_status::unsupported_pattern);
				}
				return { .Kind = node_kind::character, .Value = static_cast<unsigned char>(value), .Flag = Flags.Fold };
			}
			auto character_set() -> std::uint32_t
			{
				auto set     = character_class{ };
				set.Negated  = take('^');
				auto hasItem = false;
				while (Position < Text.size() && Text[Position] != ']') {
					Budget.tick();
					if (Text.substr(Position).starts_with("[:")) {
						fail(source_search_status::unsupported_pattern);
					}
					auto item = take('\\') ? escape(true) : node{ .Kind = node_kind::character, .Value = scalar(Text, Position) };
					hasItem   = true;
					if (item.Kind == node_kind::character_class) {
						const auto& other = Program.Classes[static_cast<std::size_t>(item.Value)];
						if (other.Negated) {
							fail(source_search_status::unsupported_pattern);
						}
						set.Ranges.insert(set.Ranges.end(), other.Ranges.begin(), other.Ranges.end());
						set.Categories |= other.Categories;
					} else {
						auto last = item.Value;
						if (Position + 1U < Text.size() && Text[Position] == '-' && Text[Position + 1U] != ']') {
							++Position;
							const auto end = take('\\') ? escape(true) : node{ .Kind = node_kind::character, .Value = scalar(Text, Position) };
							if (end.Kind != node_kind::character || end.Value < item.Value) {
								fail();
							}
							last = end.Value;
						}
						set.Ranges.push_back({ item.Value, last });
					}
				}
				if (!hasItem || !take(']')) {
					fail();
				}
				return class_node(std::move(set));
			}
			auto group(std::uint32_t aDepth) -> std::uint32_t
			{
				const auto saved = Flags;
				if (take('?') && !take(':')) {
					auto enabled = true;
					auto found   = false;
					while (Position < Text.size() && Text[Position] != ')' && Text[Position] != ':') {
						const auto value = Text[Position++];
						Budget.tick();
						if (value == '-') {
							enabled = false;
							continue;
						}
						found = true;
						if (value == 'i') {
							Flags.Fold = enabled;
						} else if (value == 'm') {
							Flags.Multiline = enabled;
						} else if (value == 's') {
							Flags.DotAll = enabled;
						} else if (value == 'U') {
							Flags.Ungreedy = enabled;
						} else {
							fail(source_search_status::unsupported_pattern);
						}
					}
					if (!found) {
						fail();
					}
					if (take(')')) {
						return add_node({ });
					}
					if (!take(':')) {
						fail();
					}
				}
				const auto root = expression(aDepth + 1U);
				if (!take(')')) {
					fail();
				}
				Flags = saved;
				return root;
			}
			auto atom(std::uint32_t aDepth) -> std::uint32_t
			{
				Budget.tick();
				if (take('(')) {
					return group(aDepth);
				}
				if (take('[')) {
					return character_set();
				}
				if (take('\\')) {
					return add_node(escape(false));
				}
				if (take('.')) {
					return add_node({ .Kind = node_kind::any, .Flag = Flags.DotAll });
				}
				if (take('^')) {
					return add_node({ .Kind = node_kind::assertion, .Value = static_cast<char32_t>(opcode::begin), .Flag = Flags.Multiline });
				}
				if (take('$')) {
					return add_node({ .Kind = node_kind::assertion, .Value = static_cast<char32_t>(opcode::end), .Flag = Flags.Multiline });
				}
				if (Position == Text.size() || Text[Position] == '*' || Text[Position] == '+' || Text[Position] == '?' || Text[Position] == '{') {
					fail();
				}
				return add_node({ .Kind = node_kind::character, .Value = scalar(Text, Position), .Flag = Flags.Fold });
			}
			auto emit_children(std::uint32_t aFirst, std::uint32_t aNext, bool aAlternate) -> std::uint32_t
			{
				auto children = std::vector<std::uint32_t>{ };
				for (auto child = aFirst; child != 0U; child = Nodes[child].Next) {
					Budget.tick();
					children.push_back(child);
				}
				if (!aAlternate) {
					for (auto it = children.rbegin(); it != children.rend(); ++it) {
						aNext = emit(*it, aNext);
					}
					return aNext;
				}
				auto start = emit(children.back(), aNext);
				for (auto index = children.size() - 1U; index > 0U; --index) {
					start = add_instruction({ .Op = opcode::split, .Next = emit(children[index - 1U], aNext), .Other = start });
				}
				return start;
			}
			auto nullable(std::uint32_t aNode) -> bool
			{
				Budget.tick();
				const auto& item = Nodes[aNode];
				if (item.Kind == node_kind::empty || item.Kind == node_kind::assertion) {
					return true;
				}
				if (item.Kind == node_kind::repeat) {
					return item.Minimum == 0U || nullable(item.Child);
				}
				if (item.Kind == node_kind::sequence || item.Kind == node_kind::alternate) {
					for (auto child = item.Child; child != 0U; child = Nodes[child].Next) {
						const auto empty = nullable(child);
						if (item.Kind == node_kind::sequence && !empty) {
							return false;
						}
						if (item.Kind == node_kind::alternate && empty) {
							return true;
						}
					}
					return item.Kind == node_kind::sequence;
				}
				return false;
			}
			auto emit_repeat(const node& aNode, std::uint32_t aNext) -> std::uint32_t
			{
				auto start     = aNext;
				auto mandatory = aNode.Minimum;
				if (aNode.Maximum == 1001U) {
					const auto split          = add_instruction({ .Op = opcode::split });
					const auto body           = emit(aNode.Child, split);
					Program.Code[split].Next  = aNode.Flag ? aNext : body;
					Program.Code[split].Other = aNode.Flag ? body : aNext;
					if (mandatory > 0U) {
						start = body;
						--mandatory;
					} else if (nullable(aNode.Child)) {
						// A nullable star is an optional plus. Its first empty
						// operand reaches the loop exit before lower-priority
						// alternatives, matching RE2's leftmost-first policy.
						start = add_instruction({ .Op = opcode::split, .Next = aNode.Flag ? aNext : body, .Other = aNode.Flag ? body : aNext });
					} else {
						start = split;
					}
				} else {
					for (auto count = aNode.Maximum; count > aNode.Minimum; --count) {
						const auto body = emit(aNode.Child, start);
						start           = add_instruction({ .Op = opcode::split, .Next = aNode.Flag ? start : body, .Other = aNode.Flag ? body : start });
					}
				}
				for (auto count = 0U; count < mandatory; ++count) {
					start = emit(aNode.Child, start);
				}
				return start;
			}
			auto emit(std::uint32_t aNode, std::uint32_t aNext) -> std::uint32_t
			{
				Budget.tick();
				const auto& item = Nodes[aNode];
				switch (item.Kind) {
				case node_kind::empty:           return aNext;
				case node_kind::sequence:        return emit_children(item.Child, aNext, false);
				case node_kind::alternate:       return emit_children(item.Child, aNext, true);
				case node_kind::repeat:          return emit_repeat(item, aNext);
				case node_kind::character:       return add_instruction({ .Op = opcode::character, .Next = aNext, .Value = item.Flag ? fold(item.Value) : item.Value, .Flag = item.Flag });
				case node_kind::any:             return add_instruction({ .Op = opcode::any, .Next = aNext, .Flag = item.Flag });
				case node_kind::character_class: return add_instruction({ .Op = opcode::character_class, .Next = aNext, .Value = item.Value });
				case node_kind::assertion:       return add_instruction({ .Op = static_cast<opcode>(item.Value), .Next = aNext, .Flag = item.Flag });
				}
				fail();
			}
		};
		void compile_literal(source_match_program::implementation& aProgram, std::string_view aQuery, budget& aBudget)
		{
			aProgram.Literal.reserve(aQuery.size());
			aProgram.Prefix.reserve(aQuery.size());
			for (auto offset = std::size_t{ 0U }; offset < aQuery.size();) {
				aBudget.tick();
				auto value = scalar(aQuery, offset);
				if (value == U'\r' && offset < aQuery.size() && aQuery[offset] == '\n') {
					++offset;
					value = U'\n';
				}
				aProgram.Literal.push_back(aProgram.Options.MatchCase ? value : fold(value));
			}
			aProgram.Prefix.resize(aProgram.Literal.size(), 0U);
			for (auto index = std::size_t{ 1U }; index < aProgram.Literal.size(); ++index) {
				auto prefix = aProgram.Prefix[index - 1U];
				while (prefix > 0U && aProgram.Literal[index] != aProgram.Literal[prefix]) {
					aBudget.tick();
					prefix = aProgram.Prefix[prefix - 1U];
				}
				aBudget.tick();
				if (aProgram.Literal[index] == aProgram.Literal[prefix]) {
					++prefix;
				}
				aProgram.Prefix[index] = prefix;
			}
		}
	}
	auto compile_pattern(std::string_view aQuery, source_search_options aOptions, source_match_limits aLimits, diff_compute_stop* aStop, bool aGlob) -> source_match_compile
	{
		auto result = source_match_compile{ };
		try {
			auto work = budget{ aLimits.MaximumCompileSteps, aStop };
			try {
				if (!limits_valid(aLimits)) {
					throw stopped{ source_search_status::resource_limit };
				}
				if (aQuery.empty()) {
					result.Status = source_search_status::idle;
					return result;
				}
				if (aQuery.size() > (aGlob ? 32768U : maximum_source_search_query_bytes)) {
					throw stopped{ source_search_status::invalid_input };
				}
				for (auto offset = std::size_t{ 0U }; offset < aQuery.size();) {
					work.tick();
					(void)scalar(aQuery, offset);
				}
				// Compile scratch, persistent instructions/classes and VM space have a
				// conservative per-instruction reservation, charged before allocation.
				const auto reservation = aLimits.MaximumInstructions * 256U + aQuery.size() * 32U;
				if (reservation > aLimits.MaximumOwnedBytes) {
					throw stopped{ source_search_status::resource_limit };
				}
				auto normalized = std::string{ };
				normalized.reserve(aQuery.size());
				for (auto offset = std::size_t{ 0U }; offset < aQuery.size(); ++offset) {
					work.tick();
					// [[BPR-1432]]. CRLF reads as LF; a lone CR stays literal.
					if (aQuery[offset] == '\r' && offset + 1U < aQuery.size() && aQuery[offset + 1U] == '\n') {
						normalized += '\n';
						++offset;
					} else {
						normalized += aQuery[offset];
					}
				}
				aQuery           = normalized;
				auto program     = std::make_shared<source_match_program::implementation>();
				program->Limits  = aLimits;
				program->Options = aOptions;
				if (aOptions.RegularExpression) {
					program->Code.reserve(aLimits.MaximumInstructions);
					program->Classes.reserve(aLimits.MaximumInstructions);
					program->Start = parser{ aQuery, *program, work }.parse();
				} else {
					compile_literal(*program, aQuery, work);
				}
				program->OwnedBytes = reservation;
				result.Program      = source_match_access::make(std::move(program));
				result.Status       = source_search_status::complete;
			} catch (const stopped& stop) {
				result.Status      = stop.Status;
				result.ErrorOffset = stop.Offset;
			}
			result.Steps = work.Steps;
		} catch (const stopped& stop) {
			result.Status = stop.Status;
		} catch (const std::bad_alloc&) {
			result.Status = source_search_status::resource_limit;
		}
		return result;
	}
}
namespace bpr::core {
	source_match_program::source_match_program(std::shared_ptr<const implementation> aProgram)
		: Program(std::move(aProgram))
	{ }
	auto source_match_program::retained_bytes() const noexcept -> std::size_t { return Program->OwnedBytes; }
	auto compile_source_match(std::string_view aQuery, source_search_options aOptions, source_match_limits aLimits, diff_compute_stop* aStop) -> source_match_compile
	{
		return source_matching::compile_pattern(aQuery, aOptions, aLimits, aStop);
	}
}
