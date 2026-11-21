// [[BPR-2351]]. Code anchored to a reviewed file borrows the roles that
// file's own classification gives its names. Spans here are written as the
// classifier and the C++ refinement give them, so the course needs no grammar
// and no Qt.

#include "bpr/core/code_context_roles.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;
	using role = appearance_role;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// \brief A span over the first occurrence of `aWord` on `aLine` of `aText`.
	auto span_of(const std::string_view aText, const std::uint32_t aLine, const std::string_view aWord, const role aRole, const std::size_t aFrom = 0U) -> token_span
	{
		auto begin = std::size_t{ 0U };
		for (auto line = 0U; line < aLine; ++line) {
			begin = aText.find('\n', begin) + 1U;
		}
		const auto at = aText.find(aWord, begin + aFrom);
		expect(at != std::string_view::npos, "the word is on its line: " + std::string{ aWord });
		return { .Line = aLine, .Column = static_cast<std::uint32_t>(at - begin), .Length = static_cast<std::uint32_t>(aWord.size()), .Role = aRole };
	}

	/// \brief The role each identifier-sized run of `aSpans` paints, as `word:role` in order.
	auto painted(const std::string_view aText, const token_span_list& aSpans) -> std::string
	{
		auto lines = std::vector<std::size_t>{ 0U };
		for (auto at = aText.find('\n'); at != std::string_view::npos; at = aText.find('\n', at + 1U)) {
			lines.push_back(at + 1U);
		}
		auto out = std::string{ };
		for (const auto& span : aSpans) {
			out += std::string{ aText.substr(lines[span.Line] + span.Column, span.Length) } + ":" + std::to_string(static_cast<int>(span.Role)) + " ";
		}
		return out;
	}

	auto role_name(const role aRole) -> std::string { return std::to_string(static_cast<int>(aRole)); }

	/// The reviewed file: a namespace, a snake_case type with a constructor, a
	/// function, an enum and a macro; `frame` is both a type and a variable.
	constexpr auto File = std::string_view{
		"#define RENDER_CHECK(x) x\n"
		"namespace engine {\n"
		"struct frame_size { frame_size(int w); int Width; };\n"
		"enum class present_mode { fifo, mailbox };\n"
		"auto clamp_size(frame_size aSize) -> frame_size;\n"
		"struct frame { };\n"
		"int frame = 0;\n"
		"}\n"
	};

	auto file_spans() -> token_span_list
	{
		return {
			span_of(File, 0U, "#define", role::syntax_preprocessor),
			span_of(File, 0U, "RENDER_CHECK", role::syntax_macro),
			span_of(File, 1U, "namespace", role::syntax_keyword),
			span_of(File, 1U, "engine", role::syntax_namespace),
			span_of(File, 2U, "struct", role::syntax_keyword),
			span_of(File, 2U, "frame_size", role::syntax_type),
			// The constructor's declaration: the grammar calls it a function.
			span_of(File, 2U, "frame_size", role::syntax_function, 18U),
			span_of(File, 2U, "int", role::syntax_builtin),
			span_of(File, 3U, "enum", role::syntax_keyword),
			span_of(File, 3U, "present_mode", role::syntax_type),
			span_of(File, 3U, "fifo", role::syntax_enum_member),
			span_of(File, 3U, "mailbox", role::syntax_enum_member),
			span_of(File, 4U, "clamp_size", role::syntax_function),
			span_of(File, 5U, "frame", role::syntax_type),
			span_of(File, 6U, "frame", role::syntax_variable),
		};
	}

	void reads_the_file()
	{
		const auto roles = code_context_roles_of(File, file_spans());
		expect(code_context_role(roles, "engine") == role::syntax_namespace, "a namespace the file declares is a namespace");
		expect(code_context_role(roles, "frame_size") == role::syntax_type, "a type whose constructor reads as a function stays a type");
		expect(code_context_role(roles, "present_mode") == role::syntax_type, "an enum is a type");
		expect(code_context_role(roles, "fifo") == role::syntax_enum_member, "its members are members");
		expect(code_context_role(roles, "clamp_size") == role::syntax_function, "a function is a function");
		expect(code_context_role(roles, "RENDER_CHECK") == role::syntax_macro, "a macro is a macro");
		expect(!code_context_role(roles, "frame"), "a name the file gives conflicting roles is not borrowed");
		expect(!code_context_role(roles, "int") && !code_context_role(roles, "struct"), "built-ins and keywords are the grammar's, not the file's");
		expect(!code_context_role(roles, "Width"), "a name the file leaves plain is not a context name");
		for (auto at = std::size_t{ 1U }; at < roles.Names.size(); ++at) {
			expect(roles.Names[at - 1U].Name < roles.Names[at].Name, "names are ordered and unique");
		}

		auto broken = file_spans();
		broken.push_back({ .Line = 40U, .Column = 0U, .Length = 3U, .Role = role::syntax_type });
		broken.push_back({ .Line = 2U, .Column = 7U, .Length = 5U, .Role = role::syntax_type }); // "frame" inside frame_size
		broken.push_back({ .Line = 1U, .Column = 0U, .Length = 400U, .Role = role::syntax_type }); // across lines
		expect(code_context_roles_of(File, broken) == roles, "spans outside the text, inside a name or across lines are not read");
		expect(code_context_roles_of(File, { }).Names.empty(), "no spans, no names");
	}

	void borrows_by_name()
	{
		const auto roles = code_context_roles_of(File, file_spans());
		// The finding's block: its grammar named only what it could see.
		constexpr auto block = std::string_view{
			"frame_size size = clamp_size(input);\n"
			"engine::present(present_mode::fifo, size.Width);\n"
			"auto f = frame{ };\n"
			"// frame_size in a comment\n"
			"#if RENDER_CHECK\n"
			"other.clamp_size(); swap_chain chain;\n"
		};
		const auto own = token_span_list{
			span_of(block, 0U, "clamp_size", role::syntax_function),
			span_of(block, 1U, "engine", role::syntax_type),
			span_of(block, 1U, "present", role::syntax_function),
			span_of(block, 1U, "present_mode", role::syntax_type),
			span_of(block, 2U, "auto", role::syntax_keyword),
			{ .Line = 3U, .Column = 0U, .Length = 26U, .Role = role::syntax_comment },
			{ .Line = 4U, .Column = 0U, .Length = 3U, .Role = role::syntax_preprocessor },
			span_of(block, 4U, "RENDER_CHECK", role::syntax_macro),
			span_of(block, 5U, "clamp_size", role::syntax_function),
		};
		const auto borrowed = borrow_code_context_roles(block, own, roles, 1024U);
		const auto text     = painted(block, borrowed);
		const auto has      = [&text](const std::string& aWord, const role aRole) { return text.find(aWord + ":" + role_name(aRole) + " ") != std::string::npos; };
		expect(text.starts_with("frame_size:" + role_name(role::syntax_type) + " "), "a type declared only in the file is a type in the block: " + text);
		expect(has("engine", role::syntax_namespace), "the file's namespace replaces the block's guessed scope");
		expect(has("present", role::syntax_function), "a name the file does not have keeps the block's own role");
		expect(has("fifo", role::syntax_enum_member), "an enum member is borrowed after `::`");
		expect(has("auto", role::syntax_keyword), "a keyword is the block's");
		expect(text.find("frame:") == std::string::npos, "a conflicting name falls back to the block's own classification");
		expect(has("// frame_size in a comment", role::syntax_comment), "a comment is never repainted");
		expect(has("RENDER_CHECK", role::syntax_macro), "a directive line keeps its own roles");
		expect(text.find("Width:") == std::string::npos && text.find(" size:") == std::string::npos, "a plain name the file does not name stays plain");
		expect(has("clamp_size", role::syntax_function), "after `.` a function is borrowed");
		expect(text.find("swap_chain:") == std::string::npos, "a new name in a proposed fix is not given a role");

		// After `.` or `->` only a function; before `::` only a scope.
		constexpr auto access = std::string_view{ "a.frame_size; p->engine; fifo::x;\n" };
		expect(borrow_code_context_roles(access, { }, roles, 64U).empty(), "a member access or a scope a role cannot hold borrows nothing");
	}

	void deterministic_and_bounded()
	{
		const auto roles = code_context_roles_of(File, file_spans());
		expect(roles == code_context_roles_of(File, file_spans()), "equal inputs, equal names");
		constexpr auto block = std::string_view{ "frame_size a; frame_size b;\n" };
		const auto     once  = borrow_code_context_roles(block, { }, roles, 64U);
		expect(once == borrow_code_context_roles(block, { }, roles, 64U), "equal inputs, equal spans");
		expect(once.size() == 2U, "each use is one span");
		expect(borrow_code_context_roles(block, { }, roles, 1U).empty(), "past the span ceiling the block keeps its own spans");
		expect(borrow_code_context_roles(block, { }, code_context_roles{ }, 64U).empty(), "no context, no change");
		const auto outside = token_span_list{ { .Line = 9U, .Column = 0U, .Length = 1U, .Role = role::syntax_type } };
		expect(borrow_code_context_roles(block, outside, roles, 64U) == outside, "spans the text cannot hold come back as they were");

		// The same block in two files: each file decides.
		constexpr auto other      = std::string_view{ "namespace frame_size { }\n" };
		const auto     otherRoles = code_context_roles_of(other, { span_of(other, 0U, "frame_size", role::syntax_namespace) });
		expect(painted(block, borrow_code_context_roles(block, { }, otherRoles, 64U)) != painted(block, once), "the same block anchored to another file can read differently");

		// Bounded: a file of more names than the ceiling reads the first ones.
		auto many      = std::string{ };
		auto manySpans = token_span_list{ };
		for (auto index = std::size_t{ 0U }; index < maximum_code_context_names + 8U; ++index) {
			const auto name = "t" + std::to_string(index);
			manySpans.push_back({ .Line = static_cast<std::uint32_t>(index), .Column = 0U, .Length = static_cast<std::uint32_t>(name.size()), .Role = role::syntax_type });
			many += name + "\n";
		}
		expect(code_context_roles_of(many, manySpans).Names.size() == maximum_code_context_names, "at most the ceiling of names is read");
		const auto longName = std::string(maximum_code_context_name_bytes + 1U, 'n');
		expect(code_context_roles_of(longName, { { .Line = 0U, .Column = 0U, .Length = static_cast<std::uint32_t>(longName.size()), .Role = role::syntax_type } }).Names.empty(),
			   "a name past the byte bound is not read");
	}
} // namespace

int main()
{
	reads_the_file();
	borrows_by_name();
	deterministic_and_bounded();
	std::cout << "code-context-roles-tests:pass\n";
	return EXIT_SUCCESS;
}
