#include "bpr/core/theme_exchange.hpp"

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/hex.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto role_at(const std::size_t aIndex) -> appearance_role
	{
		return static_cast<appearance_role>(static_cast<std::uint8_t>(aIndex));
	}

	/// \brief Builds a palette whose every role carries its own colour.
	[[nodiscard]] auto sample_palette() -> appearance_palette
	{
		auto palette = appearance_palette{ };
		for (std::size_t index = 0U; index < appearance_role_count; ++index) {
			palette[index] = appearance_color{
				.Red   = static_cast<std::uint8_t>((index * 5U) + 1U),
				.Green = static_cast<std::uint8_t>(255U - (index * 3U)),
				.Blue  = static_cast<std::uint8_t>((index * 7U) % 251U),
				.Alpha = static_cast<std::uint8_t>(index % 2U == 0U ? 255U : 128U),
			};
		}
		return palette;
	}

	[[nodiscard]] auto valid_document() -> std::string
	{
		return export_theme(sample_palette());
	}

	/// \brief Imports and enforces the result invariant on every case.
	[[nodiscard]] auto imported(const std::string_view aDocument) -> theme_import_result
	{
		auto result = import_theme(aDocument);
		expect(result.Palette.has_value() == result.Rejections.empty(),
			   "an import yields a palette exactly when it reports no rejection");
		return result;
	}

	[[nodiscard]] auto only_rejection(const theme_import_result& aResult) -> theme_import_rejection
	{
		expect(aResult.Rejections.size() == 1U, "exactly one part of the document is refused");
		return aResult.Rejections.front();
	}

	[[nodiscard]] auto has_reason(
		const theme_import_result& aResult,
		const theme_import_reason  aReason) -> bool
	{
		return std::ranges::any_of(
			aResult.Rejections,
			[aReason](const theme_import_rejection& aRejection) {
				return aRejection.Reason == aReason;
			});
	}

	[[nodiscard]] auto line_number_of(
		const std::string_view aDocument,
		const std::string_view aText) -> std::size_t
	{
		const auto position = aDocument.find(aText);
		expect(position != std::string_view::npos, "the located text is present");
		return static_cast<std::size_t>(
				   std::ranges::count(aDocument.substr(0U, position), '\n'))
			+ 1U;
	}

	[[nodiscard]] auto appended(std::string aDocument, const std::string_view aLine) -> std::string
	{
		aDocument.append(aLine);
		aDocument.push_back('\n');
		return aDocument;
	}

	[[nodiscard]] auto with_role_line(
		std::string            aDocument,
		const appearance_role  aRole,
		const std::string_view aReplacement) -> std::string
	{
		const auto position = aDocument.find(stable_name(aRole));
		expect(position != std::string::npos, "the role line is present");
		const auto end = aDocument.find('\n', position);
		expect(end != std::string::npos, "the role line ends");
		aDocument.replace(position, end - position, aReplacement);
		return aDocument;
	}

	[[nodiscard]] auto without_role(std::string aDocument, const appearance_role aRole) -> std::string
	{
		const auto position = aDocument.find(stable_name(aRole));
		expect(position != std::string::npos, "the role line is present");
		const auto end = aDocument.find('\n', position);
		expect(end != std::string::npos, "the role line ends");
		aDocument.erase(position, (end + 1U) - position);
		return aDocument;
	}

	[[nodiscard]] auto role_line_of(
		const std::string&    aDocument,
		const appearance_role aRole) -> std::string
	{
		const auto position = aDocument.find(stable_name(aRole));
		expect(position != std::string::npos, "the role line is present");
		const auto end = aDocument.find('\n', position);
		expect(end != std::string::npos, "the role line ends");
		return aDocument.substr(position, end - position);
	}

	/// \brief Rebuilds a valid document behind a different signature line.
	[[nodiscard]] auto with_signature_line(const std::string_view aLine) -> std::string
	{
		const auto document = valid_document();
		return std::string{ aLine } + '\n' + document.substr(document.find('\n') + 1U);
	}

	auto test_a_valid_document_round_trips() -> void
	{
		const auto palette  = sample_palette();
		const auto document = export_theme(palette);

		expect(document.starts_with("bpr-theme 1\n"),
			   "a document announces its format and version before anything else");
		expect(std::ranges::count(document, '\n') == static_cast<std::ptrdiff_t>(appearance_role_count) + 1,
			   "a document carries the signature line and one line per role");

		auto previous = std::size_t{ 0U };
		for (std::size_t index = 0U; index < appearance_role_count; ++index) {
			const auto position = document.find(stable_name(role_at(index)));
			expect(position != std::string::npos, "every role appears in the document");
			expect(position > previous, "roles are written in vocabulary order");
			previous = position;
		}

		auto opaque                                        = sample_palette();
		opaque[index_of(appearance_role::code_background)] = appearance_color{
			.Red = 0U, .Green = 0U, .Blue = 0U, .Alpha = 255U
		};
		opaque[index_of(appearance_role::syntax_error)] = appearance_color{
			.Red = 255U, .Green = 16U, .Blue = 1U, .Alpha = 0U
		};
		const auto written = export_theme(opaque);
		expect(written.find("code_background #000000ff\n") != std::string::npos
				   && written.find("syntax_error #ff100100\n") != std::string::npos,
			   "a colour is written as eight lower-case hexadecimal digits, alpha included");

		const auto result = imported(document);
		expect(result.Rejections.empty(), "a document this export produced is accepted");
		expect(result.Palette.has_value() && *result.Palette == palette,
			   "an exported palette imports back to an equal palette");
		expect(result.Palette.has_value()
				   && (*result.Palette)[index_of(appearance_role::syntax_import)]
					   == palette[index_of(appearance_role::syntax_import)],
			   "a role whose identifier resembles a directive is still an ordinary role");
		expect(export_theme(*result.Palette) == document,
			   "the exchange is stable across a second round trip");
	}

	auto test_a_document_may_be_written_by_hand() -> void
	{
		const auto shortened = with_role_line(
			valid_document(), appearance_role::code_background, "code_background #0A0B0C");
		const auto result = imported(shortened);
		expect(result.Palette.has_value(), "a six-digit value is accepted");
		expect(result.Palette.has_value()
				   && (*result.Palette)[index_of(appearance_role::code_background)]
					   == appearance_color{
						   .Red = 10U, .Green = 11U, .Blue = 12U, .Alpha = 255U },
			   "a six-digit value is opaque and its digits are case-insensitive");

		auto spaced = std::string{ "\n   \n" };
		for (const auto character : valid_document()) {
			if (character == '\n') {
				spaced.append("\r\n");
				continue;
			}
			spaced.push_back(character);
		}
		spaced.append("\t\n");
		const auto tolerant = imported(spaced);
		expect(tolerant.Palette.has_value() && *tolerant.Palette == sample_palette(),
			   "blank lines, carriage returns, and surrounding blanks do not change the palette");

		const auto tabbed = with_role_line(
			valid_document(), appearance_role::state_stale, "state_stale\t#01020304");
		expect(imported(tabbed).Palette.has_value(),
			   "an identifier and its value may be separated by any run of blanks");
	}

	auto test_an_unknown_field_is_refused_by_name() -> void
	{
		const auto extra  = appended(valid_document(), "custom_glow #112233");
		const auto result = imported(extra);
		expect(result.Rejections
				   == std::vector<theme_import_rejection>{
					   { .Reason  = theme_import_reason::unknown_field,
						 .Subject = "custom_glow",
						 .Line    = line_number_of(extra, "custom_glow") },
				   },
			   "a key outside the closed vocabulary is refused by name, not silently ignored");
		expect(!result.Palette.has_value(),
			   "a document carrying an unknown field produces no palette");

		const auto misspelled = with_role_line(
			valid_document(), appearance_role::syntax_keyword, "Syntax_Keyword #010203");
		expect(only_rejection(imported(misspelled))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::unknown_field,
					   .Subject = "Syntax_Keyword",
					   .Line    = line_number_of(misspelled, "Syntax_Keyword") },
			   "a near-miss identifier is an unknown field, not a case-insensitive match");

		const auto oversizedKey = appended(
			valid_document(), std::string(500U, 'z') + " #112233");
		const auto quoted = only_rejection(imported(oversizedKey));
		expect(quoted.Reason == theme_import_reason::unknown_field
				   && quoted.Subject.size() == theme_import_subject_limit,
			   "a quoted subject is bounded by the published limit");
	}

	auto test_a_partial_document_is_refused_rather_than_defaulted() -> void
	{
		const auto partial = without_role(
			without_role(valid_document(), appearance_role::code_background),
			appearance_role::syntax_error);
		const auto result = imported(partial);
		expect(result.Rejections
				   == std::vector<theme_import_rejection>{
					   { .Reason  = theme_import_reason::missing_field,
						 .Subject = "code_background",
						 .Line    = 0U },
					   { .Reason  = theme_import_reason::missing_field,
						 .Subject = "syntax_error",
						 .Line    = 0U },
				   },
			   "each unassigned role is named once, in vocabulary order");
		expect(!result.Palette.has_value(),
			   "a partial document yields no palette instead of a defaulted one");

		const auto headerOnly = imported("bpr-theme 1\n");
		expect(headerOnly.Rejections.size() == appearance_role_count - feedback_label_role_count - 3U,
			   "a document that assigns nothing is missing every role but the additive optional colours");
	}

	/// \brief [[BPR-1083]] [[BPR-1411]]. A document written before additive
	/// colours existed still imports, those colours unassigned and every other role as written.
	auto test_a_document_without_label_tints_leaves_them_unassigned() -> void
	{
		auto document = std::string{ "bpr-theme 1\n" };
		auto expected = appearance_palette{ };
		for (std::size_t index = 0U; index < appearance_role_count; ++index) {
			const auto role = static_cast<appearance_role>(static_cast<std::uint8_t>(index));
			if (defaults_on_import(role)) {
				expected[index] = unassigned_appearance_color;
				continue;
			}
			const auto value      = static_cast<std::uint8_t>(index + 1U);
			expected[index]       = appearance_color{ value, value, value, 255U };
			auto           line   = std::string{ stable_name(role) } + " #";
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			for (auto channel = 0; channel < 3; ++channel) {
				line.push_back(digits[value >> 4U]);
				line.push_back(digits[value & 0x0FU]);
			}
			document += line + "\n";
		}
		const auto result = imported(document);
		expect(result.Rejections.empty() && result.Palette.has_value(),
			   "a document with the original required roles imports");
		expect(result.Palette == std::optional{ expected },
			   "optional additive colours are unassigned and every required role is the document's");
		const auto partial = imported(document + "feedback_label_issue #d1242fff\n");
		expect(partial.Palette.has_value()
				   && (*partial.Palette)[index_of(appearance_role::feedback_label_issue)] == appearance_color{ 0xD1U, 0x24U, 0x2FU, 255U }
				   && (*partial.Palette)[index_of(appearance_role::feedback_label_praise)] == unassigned_appearance_color,
			   "a document that assigns some label tints keeps them and leaves the rest unassigned");
		expect(!defaults_on_import(appearance_role::syntax_other) && !defaults_on_import(appearance_role::state_stale)
				   && defaults_on_import(appearance_role::feedback_label_praise)
				   && defaults_on_import(appearance_role::feedback_label_quibble)
				   && defaults_on_import(appearance_role::state_attention)
				   && defaults_on_import(appearance_role::syntax_namespace)
				   && defaults_on_import(appearance_role::syntax_enum_member),
			   "the twelve label tints, attention amber and new syntax categories may be absent");
		// [[BPR-1085]]. A document written with the label tints but before the
		// attention amber still imports, the amber unassigned.
		const auto withoutAttention = imported(document + "feedback_label_issue #d1242fff\n");
		expect(withoutAttention.Palette.has_value()
				   && (*withoutAttention.Palette)[index_of(appearance_role::state_attention)] == unassigned_appearance_color,
			   "a document without the attention amber leaves it unassigned");
		const auto withAttention = imported(document + "state_attention #ad7a00ff\n");
		expect(withAttention.Palette.has_value()
				   && (*withAttention.Palette)[index_of(appearance_role::state_attention)] == appearance_color{ 0xADU, 0x7AU, 0x00U, 255U },
			   "a document that assigns the attention amber keeps it");
		expect(stable_name(appearance_role::state_attention) == "state_attention"
				   && find_appearance_role("state_attention") == appearance_role::state_attention,
			   "the attention amber's stable name is state_attention");
		expect(stable_name(appearance_role::feedback_label_issue) == "feedback_label_issue"
				   && find_appearance_role("feedback_label_quibble") == appearance_role::feedback_label_quibble,
			   "the label tints' stable names are feedback_label_<id>");
	}

	auto test_external_references_are_refused_wherever_they_appear() -> void
	{
		const auto inValue = with_role_line(
			valid_document(),
			appearance_role::code_background,
			"code_background https://theme.example/palette");
		const auto valueRejection = only_rejection(imported(inValue));
		expect(valueRejection.Reason == theme_import_reason::external_reference,
			   "a reference used as a value is refused as an external reference");
		expect(valueRejection.Subject == "https://theme.example/palette",
			   "an external reference is quoted in the refusal");

		const auto inKey = with_role_line(
			valid_document(), appearance_role::state_unread, "https://theme.example/x #112233");
		const auto keyResult = imported(inKey);
		expect(only_rejection(keyResult).Reason == theme_import_reason::external_reference,
			   "a reference hidden in a key is refused as a reference, not as an unknown field");
		expect(!has_reason(keyResult, theme_import_reason::unknown_field),
			   "the scan runs before any key is interpreted");

		const auto trailing = appended(valid_document(), "note file:/etc/passwd");
		expect(only_rejection(imported(trailing)).Subject == "file:/etc/passwd",
			   "a scheme without an authority is still an external reference");

		const auto unknownScheme = appended(valid_document(), "note weird://host/theme");
		expect(only_rejection(imported(unknownScheme))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::external_reference,
					   .Subject = "weird://host/theme",
					   .Line    = line_number_of(unknownScheme, "weird") },
			   "an unrecognised scheme is refused by its separator alone");

		expect(!has_reason(imported(valid_document()), theme_import_reason::external_reference),
			   "an ordinary document carries no external reference");
	}

	auto test_include_directives_are_refused() -> void
	{
		const auto imports = appended(valid_document(), "@import other_theme");
		expect(only_rejection(imported(imports))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::include_directive,
					   .Subject = "import",
					   .Line    = line_number_of(imports, "@import") },
			   "a directive that pulls in another document is refused and named");

		const auto includes = appended(valid_document(), "#include <other_theme>");
		const auto result   = imported(includes);
		expect(result.Rejections.size() == 2U
				   && result.Rejections.front().Reason == theme_import_reason::include_directive
				   && result.Rejections.back().Reason == theme_import_reason::executable_payload,
			   "distinct classes of foreign content are reported distinctly, in document order");

		expect(!has_reason(imported(valid_document()), theme_import_reason::include_directive),
			   "the role named syntax_import is not mistaken for a directive");
	}

	auto test_style_and_executable_payloads_are_refused() -> void
	{
		const auto styleBlock  = appended(valid_document(), "QWidget { color: green; }");
		const auto blockResult = imported(styleBlock);
		expect(only_rejection(blockResult).Reason == theme_import_reason::style_payload,
			   "block syntax the format does not have is refused as a style payload");
		expect(only_rejection(blockResult).Subject.starts_with('{'),
			   "the refused style content is quoted from where it begins");

		const auto styleWord = appended(valid_document(), "qss theme");
		expect(only_rejection(imported(styleWord))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::style_payload,
					   .Subject = "qss",
					   .Line    = line_number_of(styleWord, "qss theme") },
			   "a style-sheet keyword is refused as a style payload and named");

		const auto setter = appended(valid_document(), "setStyleSheet payload");
		expect(only_rejection(imported(setter)).Subject == "setStyleSheet",
			   "a style-sheet application call is quoted as written");

		const auto markup       = appended(valid_document(), "<script>eval(1)</script>");
		const auto markupResult = imported(markup);
		expect(only_rejection(markupResult).Reason == theme_import_reason::executable_payload,
			   "markup and script content is refused as an executable payload");
		expect(only_rejection(markupResult).Subject.starts_with("<script>"),
			   "the refused executable content is quoted from where it begins");

		const auto interpreter = appended(valid_document(), "#!/bin/sh");
		expect(only_rejection(imported(interpreter))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::executable_payload,
					   .Subject = "#!/bin/sh",
					   .Line    = line_number_of(interpreter, "#!") },
			   "an interpreter line is refused as an executable payload");

		const auto call = appended(valid_document(), "function payload");
		expect(only_rejection(imported(call)).Subject == "function",
			   "an executable keyword is refused and named");

		expect(!has_reason(imported(valid_document()), theme_import_reason::style_payload)
				   && !has_reason(imported(valid_document()),
								  theme_import_reason::executable_payload),
			   "an ordinary document carries neither payload");
	}

	auto test_a_duplicate_key_is_refused_rather_than_resolved() -> void
	{
		const auto repeated = appended(valid_document(), "code_background #00ff00ff");
		const auto result   = imported(repeated);
		expect(only_rejection(result)
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::duplicate_field,
					   .Subject = "code_background",
					   .Line    = appearance_role_count + 2U },
			   "a repeated key is refused and named rather than resolved by precedence");
		expect(!result.Palette.has_value(),
			   "neither of a duplicate key's values can win, because no palette is produced");

		const auto document = valid_document();
		const auto same     = appended(
			document, role_line_of(document, appearance_role::state_stale));
		expect(only_rejection(imported(same)).Reason == theme_import_reason::duplicate_field,
			   "a key repeated with the same value is still a duplicate");
	}

	auto test_an_oversized_document_is_refused_before_parsing() -> void
	{
		auto atLimit = valid_document();
		expect(atLimit.size() < theme_document_byte_limit,
			   "a total document fits well inside the published bound");
		atLimit.append(theme_document_byte_limit - atLimit.size(), '\n');
		expect(atLimit.size() == theme_document_byte_limit, "the padded document is exactly at the bound");
		expect(imported(atLimit).Palette.has_value(),
			   "a document of exactly the published size is accepted");

		const auto overLimit = atLimit + '\n';
		expect(only_rejection(imported(overLimit))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::document_too_large,
					   .Subject = "",
					   .Line    = 0U },
			   "one byte over the published bound is refused as an oversized document");

		auto hostile = std::string{ "https://theme.example/x\n{ }\ncustom_glow #112233\n" };
		while (hostile.size() <= theme_document_byte_limit) {
			hostile.append("custom_glow #112233\n");
		}
		expect(only_rejection(imported(hostile)).Reason == theme_import_reason::document_too_large,
			   "an oversized document is measured before it is parsed, so it reports size alone");
	}

	auto test_the_document_structure_is_closed() -> void
	{
		expect(only_rejection(imported(""))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::malformed_document,
					   .Subject = "",
					   .Line    = 0U },
			   "an empty document carries no signature and is refused");
		expect(only_rejection(imported(" \n\t\n")).Reason
				   == theme_import_reason::malformed_document,
			   "a document of blanks carries no signature and is refused");

		const auto complete = valid_document();
		const auto headless = complete.substr(complete.find('\n') + 1U);
		expect(only_rejection(imported(headless)).Reason == theme_import_reason::malformed_document,
			   "a document whose first line is not the signature is refused");

		expect(only_rejection(imported(with_signature_line("other-theme 1")))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::malformed_document,
					   .Subject = "other-theme 1",
					   .Line    = 1U },
			   "another format's signature is refused with the offending line quoted");

		expect(only_rejection(imported(with_signature_line("bpr-theme 2")))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::unsupported_version,
					   .Subject = "2",
					   .Line    = 1U },
			   "a version this build does not interpret is refused under its own reason");

		expect(only_rejection(imported(with_signature_line("bpr-theme latest"))).Reason
				   == theme_import_reason::malformed_document,
			   "a version that is not a number is a malformed signature line");

		const auto extraToken = with_role_line(
			valid_document(), appearance_role::syntax_number, "syntax_number #010203 #040506");
		expect(only_rejection(imported(extraToken))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::malformed_document,
					   .Subject = "syntax_number #010203 #040506",
					   .Line    = line_number_of(extraToken, "syntax_number") },
			   "a line that is not one identifier and one value is refused");

		const auto shortColor = with_role_line(
			valid_document(), appearance_role::syntax_number, "syntax_number #12345");
		expect(only_rejection(imported(shortColor))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::malformed_color,
					   .Subject = "#12345",
					   .Line    = line_number_of(shortColor, "syntax_number") },
			   "a value of the wrong length is refused and quoted");

		const auto badDigits = with_role_line(
			valid_document(), appearance_role::syntax_number, "syntax_number #gg0000");
		expect(only_rejection(imported(badDigits)).Reason == theme_import_reason::malformed_color,
			   "a value outside hexadecimal is refused");

		auto embedded = valid_document();
		embedded.insert(embedded.find("syntax_number"), std::string(1U, '\0'));
		expect(only_rejection(imported(embedded))
				   == theme_import_rejection{
					   .Reason  = theme_import_reason::malformed_document,
					   .Subject = "",
					   .Line    = line_number_of(embedded, "syntax_number") },
			   "a byte outside the format's alphabet is refused without quoting it");

		const auto highBytes = appended(valid_document(), "note \xe2\x80\xa8");
		expect(only_rejection(imported(highBytes)).Reason
				   == theme_import_reason::malformed_document,
			   "a non-ASCII byte is outside the format's alphabet");
	}

	/// [[BPR-1439]]. The one plain lowercase hex the core and the Qt-free
	/// targets share; a colour is written with it.
	void test_lower_hex_is_two_lowercase_digits_per_byte()
	{
		const auto bytes = std::vector<std::uint8_t>{ 0x00U, 0x0FU, 0xA5U, 0xFFU };
		expect(bpr::core::lower_hex(bytes) == "000fa5ff", "two lowercase digits per byte, high nibble first");
		expect(bpr::core::lower_hex(std::string_view{ "\x01\xFE", 2U }) == "01fe", "characters are read as octets");
		expect(bpr::core::lower_hex(std::span<const std::uint8_t>{ }).empty(), "no bytes, no digits");
		auto text = std::string{ "#" };
		bpr::core::append_lower_hex(text, bytes);
		expect(text == "#000fa5ff", "appending keeps what was there");
	}
} // namespace

int main()
{
	test_lower_hex_is_two_lowercase_digits_per_byte();
	test_a_valid_document_round_trips();
	test_a_document_may_be_written_by_hand();
	test_an_unknown_field_is_refused_by_name();
	test_a_partial_document_is_refused_rather_than_defaulted();
	test_a_document_without_label_tints_leaves_them_unassigned();
	test_external_references_are_refused_wherever_they_appear();
	test_include_directives_are_refused();
	test_style_and_executable_payloads_are_refused();
	test_a_duplicate_key_is_refused_rather_than_resolved();
	test_an_oversized_document_is_refused_before_parsing();
	test_the_document_structure_is_closed();
	return EXIT_SUCCESS;
}
