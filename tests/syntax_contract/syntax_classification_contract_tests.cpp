#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QDir>
#include <QStandardPaths>
#include <QString>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
	namespace app    = bpr::application;
	namespace core   = bpr::core;
	namespace syntax = bpr::adapters::syntax;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto fixture_root() -> std::filesystem::path
	{
		return std::filesystem::path{ BPR_SYNTAX_FIXTURE_ROOT };
	}

	[[nodiscard]] auto read_fixture(const std::string_view aName) -> std::string
	{
		const auto path = fixture_root() / aName;
		auto       file = std::ifstream{ path, std::ios::binary };
		expect(file.is_open(), "fixture file opens");
		auto contents = std::ostringstream{ };
		contents << file.rdbuf();
		return contents.str();
	}

	[[nodiscard]] auto blob(const std::uint8_t aSeed) -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = aSeed;
		bytes[19]  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	/// \brief Serves fixture bytes for the blob identities a test invents.
	class fixture_blobs final : public syntax::blob_source
	{
	public:
		void install(const core::git_blob_oid& aBlob, std::string aBytes)
		{
			Bytes.insert_or_assign(canonical(aBlob), std::move(aBytes));
		}

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid& aBlob)
			-> std::optional<std::string> override
		{
			++Reads;
			const auto found = Bytes.find(canonical(aBlob));
			if (found == Bytes.end()) {
				return std::nullopt;
			}
			return found->second;
		}

		std::size_t Reads = 0U;

	private:
		[[nodiscard]] static auto canonical(const core::git_blob_oid& aBlob)
			-> std::string
		{
			return std::string{ core::canonical_hex(aBlob).view() };
		}

		std::map<std::string, std::string> Bytes;
	};

	[[nodiscard]] auto request(
		const core::git_blob_oid&             aBlob,
		const app::syntax_definition_version& aDefinition,
		const std::size_t                     aMaximumSpans = 1'000'000U)
		-> app::syntax_classification_request
	{
		return {
			.Context      = { },
			.Key          = { .Blob = aBlob, .Definition = aDefinition },
			.MaximumSpans = aMaximumSpans,
		};
	}

	/// \brief Returns the zero-based line that carries the supplied marker.
	[[nodiscard]] auto line_of(
		const std::string_view aText,
		const std::string_view aMarker) -> std::uint32_t
	{
		auto line  = std::uint32_t{ 0U };
		auto begin = std::size_t{ 0U };
		while (begin <= aText.size()) {
			const auto end = aText.find('\n', begin);
			const auto row = aText.substr(
				begin,
				end == std::string_view::npos ? std::string_view::npos : end - begin);
			if (row.find(aMarker) != std::string_view::npos) {
				return line;
			}
			if (end == std::string_view::npos) {
				break;
			}
			begin = end + 1U;
			++line;
		}
		expect(false, "fixture marker is present");
		return 0U;
	}

	/// \brief Returns the byte length of one zero-based line.
	[[nodiscard]] auto length_of_line(
		const std::string_view aText,
		const std::uint32_t    aLine) -> std::uint32_t
	{
		auto line  = std::uint32_t{ 0U };
		auto begin = std::size_t{ 0U };
		while (begin <= aText.size()) {
			const auto end = aText.find('\n', begin);
			const auto row = aText.substr(
				begin,
				end == std::string_view::npos ? std::string_view::npos : end - begin);
			if (line == aLine) {
				return static_cast<std::uint32_t>(row.size());
			}
			if (end == std::string_view::npos) {
				break;
			}
			begin = end + 1U;
			++line;
		}
		expect(false, "fixture line exists");
		return 0U;
	}

	/// \brief Reports whether one whole line carries exactly the supplied role.
	[[nodiscard]] auto line_is_wholly(
		const app::token_span_list& aSpans,
		const std::uint32_t         aLine,
		const std::uint32_t         aLength,
		const core::appearance_role aRole) -> bool
	{
		auto covered = std::uint32_t{ 0U };
		for (const auto& span : aSpans) {
			if (span.Line != aLine) {
				continue;
			}
			if (span.Role != aRole || span.Column != covered) {
				return false;
			}
			covered += span.Length;
		}
		return covered == aLength && aLength > 0U;
	}

	// ---------------------------------------------------------------------
	// Definition selection
	// ---------------------------------------------------------------------

	void definition_selection_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		struct expectation
		{
			std::string_view Fixture;
			std::string_view Definition;
		};

		// The packaged build generates a JSON-with-comments definition that
		// claims *.json at a higher priority than plain JSON, and JSONC is a
		// superset, so a .json file selecting JSONC is the correct packaged
		// answer. The .jsonl fixture pins the plain JSON definition, which only
		// it claims, so both members of the family are covered.
		const auto expectations = std::array<expectation, 7U>{ {
			{ "multiline_comment.cpp", "C++" },
			{ "CMakeLists.txt", "CMake" },
			{ "notes.md", "Markdown" },
			{ "payload.json", "JSONC" },
			{ "records.jsonl", "JSON" },
			{ "script.py", "Python" },
			{ "notes.txt", syntax::plain_text_definition_name },
		} };

		for (const auto& expected : expectations) {
			const auto path     = (fixture_root() / expected.Fixture).string();
			const auto selected = aCatalog.definition_for_path(path);
			if (selected.Name != expected.Definition) {
				std::cerr << "FAILED: " << expected.Fixture << " selected '"
						  << selected.Name << "' rather than '"
						  << expected.Definition << "'\n";
				std::exit(EXIT_FAILURE);
			}
			const auto plain = expected.Definition
				== syntax::plain_text_definition_name;
			expect(plain == (selected.Version == 0U),
				   "a packaged definition carries a nonzero revision and the "
				   "plain-text record carries none");
			expect(app::valid(app::token_span_cache_key{
					   .Blob = blob(1U), .Definition = selected }),
				   "every selected definition forms a valid cache key");
		}

		expect(!aCatalog.definition_named("BprNoSuchDefinition").has_value(),
			   "an unpackaged definition name resolves to no record");
		expect(aCatalog.definition_named("C++").has_value(),
			   "a packaged definition name resolves");
	}

	// ---------------------------------------------------------------------
	// Complete-blob classification
	// ---------------------------------------------------------------------

	void complete_blob_state_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		const auto text  = read_fixture("multiline_comment.cpp");
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(7U);
		blobs.install(oid, text);

		auto       classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		const auto definition = aCatalog.definition_for_path("multiline_comment.cpp");
		expect(definition.Name == "C++", "the fixture selects the C++ definition");

		const auto whole = classifier.classify(request(oid, definition), { });
		expect(whole.Status == app::syntax_classification_status::available,
			   "the complete blob classifies");
		expect(!whole.Spans.empty(), "the complete blob produces spans");
		expect(whole.Key == request(oid, definition).Key,
			   "the classification repeats the requested key exactly");

		const auto commentLine = line_of(text, "BPR-COMMENT-BODY");
		const auto stringLine  = line_of(text, "BPR-STRING-BODY");
		expect(commentLine > 0U && stringLine > commentLine,
			   "the interior lines sit inside constructs opened earlier");
		expect(read_fixture("multiline_comment.cpp").find("/*") < text.find("BPR-COMMENT-BODY"),
			   "the comment opens before its interior line");

		expect(line_is_wholly(
				   whole.Spans,
				   commentLine,
				   length_of_line(text, commentLine),
				   core::appearance_role::syntax_comment),
			   "a line inside a multiline comment is wholly comment");
		expect(line_is_wholly(
				   whole.Spans,
				   stringLine,
				   length_of_line(text, stringLine),
				   core::appearance_role::syntax_string),
			   "a line inside a multiline string is wholly string");

		// The hunk begins on the comment's interior line, which carries no
		// comment marker of its own.
		const auto hunk = syntax::blob_line_range{ .First = commentLine, .Count = 2U };
		const auto hunkClassification
			= classifier.classify_hunk(request(oid, definition), hunk, { });
		expect(hunkClassification.Status == app::syntax_classification_status::available,
			   "the hunk classifies");

		auto expectedSpans = app::token_span_list{ };
		for (const auto& span : whole.Spans) {
			if (span.Line >= hunk.First && span.Line < hunk.First + hunk.Count) {
				expectedSpans.push_back(span);
			}
		}
		expect(!expectedSpans.empty(), "the hunk range carries spans");
		expect(hunkClassification.Spans == expectedSpans,
			   "a hunk beginning inside a multiline comment classifies exactly as "
			   "the same region of the complete blob");
		expect(line_is_wholly(
				   hunkClassification.Spans,
				   commentLine,
				   length_of_line(text, commentLine),
				   core::appearance_role::syntax_comment),
			   "the hunk's first line stays wholly comment");
	}

	void plain_text_and_failure_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		auto       blobs = fixture_blobs{ };
		const auto plain = blob(9U);
		blobs.install(plain, read_fixture("notes.txt"));
		auto classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };

		const auto plainDefinition = aCatalog.definition_for_path("notes.txt");
		const auto plainAnswer     = classifier.classify(request(plain, plainDefinition), { });
		expect(plainAnswer.Status == app::syntax_classification_status::available,
			   "a plain-text blob classifies");
		expect(plainAnswer.Spans.empty(),
			   "plain text carries the default role and names no span");

		const auto absent = classifier.classify(request(blob(31U), plainDefinition), { });
		expect(absent.Status == app::syntax_classification_status::unavailable,
			   "an unavailable blob is unavailable, never empty");

		const auto unknown = classifier.classify(
			request(plain, { .Name = "BprNoSuchDefinition", .Version = 1U }),
			{ });
		expect(unknown.Status == app::syntax_classification_status::unavailable,
			   "a key naming an unpackaged definition is refused");

		auto stale = aCatalog.definition_for_path("multiline_comment.cpp");
		stale.Version += 1U;
		blobs.install(blob(11U), read_fixture("multiline_comment.cpp"));
		const auto staleAnswer = classifier.classify(request(blob(11U), stale), { });
		expect(staleAnswer.Status == app::syntax_classification_status::unavailable,
			   "a key naming a revision this build does not package is refused");

		auto stop = std::stop_source{ };
		stop.request_stop();
		const auto cancelled
			= classifier.classify(request(plain, plainDefinition), stop.get_token());
		expect(cancelled.Status == app::syntax_classification_status::cancelled,
			   "a cancelled classification reports cancellation");

		const auto bounded = classifier.classify(
			request(blob(13U), aCatalog.definition_for_path("multiline_comment.cpp"), 1U),
			{ });
		expect(bounded.Status == app::syntax_classification_status::unavailable,
			   "an absent blob is refused before any span bound applies");
		blobs.install(blob(13U), read_fixture("multiline_comment.cpp"));
		const auto exceeded = classifier.classify(
			request(blob(13U), aCatalog.definition_for_path("multiline_comment.cpp"), 1U),
			{ });
		expect(exceeded.Status == app::syntax_classification_status::limit_exceeded,
			   "a classification above the span bound is refused, never truncated");
	}

	void appearance_independence_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(17U);
		blobs.install(oid, read_fixture("multiline_comment.cpp"));
		auto classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };

		const auto definition = aCatalog.definition_for_path("multiline_comment.cpp");
		const auto first      = classifier.classify(request(oid, definition), { });
		const auto passes     = classifier.lexical_passes();
		expect(passes == 1U, "one classification is one lexical pass");

		// The cache key is the whole appearance-independence argument: it carries
		// a blob and a definition revision and nothing a theme could change.
		const auto key = app::token_span_cache_key{
			.Blob = oid, .Definition = definition
		};
		const auto repeated = aCatalog.definition_for_path("multiline_comment.cpp");
		expect(key.Definition == repeated,
			   "the definition record is stable across lookups");

		const auto second = classifier.classify(request(oid, definition), { });
		expect(second.Spans == first.Spans,
			   "the same blob and definition produce the same spans");
		expect(classifier.lexical_passes() == passes + 1U,
			   "a repeated request the host did not cache is a second pass");
	}

	// ---------------------------------------------------------------------
	// Resource policy
	// ---------------------------------------------------------------------

	/// \brief Owns one directory tree the test created and must remove exactly.
	class owned_tree final
	{
	public:
		explicit owned_tree(std::filesystem::path aRoot)
			: Root{ std::move(aRoot) }
		{
			std::error_code error;
			std::filesystem::remove_all(Root, error);
			std::filesystem::create_directories(Root);
		}

		~owned_tree()
		{
			std::error_code error;
			std::filesystem::remove_all(Root, error);
		}

		owned_tree(const owned_tree&)                    = delete;
		auto operator=(const owned_tree&) -> owned_tree& = delete;
		owned_tree(owned_tree&&)                         = delete;
		auto operator=(owned_tree&&) -> owned_tree&      = delete;

		[[nodiscard]] auto path() const -> const std::filesystem::path&
		{
			return Root;
		}

	private:
		std::filesystem::path Root;
	};

	[[nodiscard]] auto canary_definition() -> std::string
	{
		return R"(<?xml version="1.0" encoding="UTF-8"?>
<language name="BprStandardPathCanary" section="Other" version="1" kateversion="5.62" extensions="*.bprcanary" priority="99">
  <highlighting>
    <contexts>
      <context name="Normal" attribute="Normal Text" lineEndContext="#stay"/>
    </contexts>
    <itemDatas>
      <itemData name="Normal Text" defStyleNum="dsNormal"/>
    </itemDatas>
  </highlighting>
</language>
)";
	}

	void write_canary(const std::filesystem::path& aDirectory)
	{
		std::filesystem::create_directories(aDirectory);
		auto file = std::ofstream{ aDirectory / "bpr-canary.xml", std::ios::binary };
		expect(file.is_open(), "the canary definition is written");
		file << canary_definition();
	}

	void packaged_resource_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		expect(aCatalog.packaged_definitions() > 0U,
			   "the packaged resource supplies definitions");
		expect(aCatalog.filesystem_definitions() == 0U,
			   "no definition is loaded from the host filesystem");
		expect(aCatalog.custom_search_paths() == 0U,
			   "the catalog adds no search path");

		// Qt's test mode redirects the generic data location into a reserved
		// sandbox, so the canary below is planted where a standard-path lookup
		// would look without touching a real data directory. Where that sandbox
		// is depends on the platform: `~/.qttest` on macOS and Linux, and on
		// Windows `qttest` under the local application data directory — which
		// an AppContainer redirects, so it is asked of Qt rather than composed
		// from a home directory (BPR-0439).
		const auto writable = QStandardPaths::writableLocation(
			QStandardPaths::GenericDataLocation);
#if defined(Q_OS_WIN)
		QStandardPaths::setTestModeEnabled(false);
		const auto sandbox = QStandardPaths::writableLocation(
								 QStandardPaths::GenericDataLocation)
			+ QStringLiteral("/qttest");
		QStandardPaths::setTestModeEnabled(true);
#else
		const auto sandbox = QDir::homePath() + QStringLiteral("/.qttest");
#endif
		if (!writable.startsWith(sandbox)) {
			std::cerr << "FAILED: the generic data location '"
					  << writable.toStdString() << "' is outside the Qt test sandbox '"
					  << sandbox.toStdString() << "'\n";
			std::exit(EXIT_FAILURE);
		}
		expect(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)
				   .contains(writable),
			   "the redirected location is one the repository would scan");

		// The control repository loads exactly the same file through an explicit
		// search path, so 'not found' below is a real negative rather than a
		// malformed fixture.
		const auto plantedRoot = owned_tree{
			std::filesystem::path{ writable.toStdString() } / "org.kde.syntax-highlighting"
		};
		write_canary(plantedRoot.path() / "syntax");

		const auto control = owned_tree{
			std::filesystem::temp_directory_path() / "bpr-syntax-canary-control"
		};
		write_canary(control.path() / "syntax");
		auto controlRepository = KSyntaxHighlighting::Repository{ };
		controlRepository.addCustomSearchPath(
			QString::fromStdString(control.path().string()));
		expect(controlRepository.definitionForName(
									QStringLiteral("BprStandardPathCanary"))
				   .isValid(),
			   "the canary definition loads through an explicit search path");
		expect(controlRepository.definitionForFileName(QStringLiteral("probe.bprcanary"))
				   .isValid(),
			   "the canary definition claims its own extension");

		auto subject = syntax::packaged_definition_catalog{ };
		expect(!subject.definition_named("BprStandardPathCanary").has_value(),
			   "the packaged lookup does not consult standard paths by name");
		expect(subject.definition_for_path("probe.bprcanary").Name
				   == syntax::plain_text_definition_name,
			   "the packaged lookup does not consult standard paths by file name");
		expect(subject.filesystem_definitions() == 0U,
			   "planting a definition in a standard path loads nothing");

		expect(!KSyntaxHighlighting::Repository{ }
					.definitionForName(QStringLiteral("None"))
					.isValid(),
			   "the plain-text sentinel shadows no packaged definition");
		(void)aCatalog;
	}

	// ---------------------------------------------------------------------
	// Identifier roles in C and C++ (BPR-0726)
	// ---------------------------------------------------------------------

	/// \brief Reports whether exactly one span covers the named word on the line
	/// with the supplied role.
	[[nodiscard]] auto word_has_role(
		const app::token_span_list& aSpans,
		const std::string_view      aText,
		const std::uint32_t         aLine,
		const std::string_view      aWord,
		const core::appearance_role aRole,
		const std::size_t           aOccurrence = 0U) -> bool
	{
		auto begin = std::size_t{ 0U };
		for (auto line = std::uint32_t{ 0U }; line < aLine; ++line) {
			begin = aText.find('\n', begin) + 1U;
		}
		const auto end    = aText.find('\n', begin);
		const auto row    = aText.substr(begin, end == std::string_view::npos ? end : end - begin);
		auto       column = row.find(aWord);
		for (auto skipped = std::size_t{ 0U }; skipped < aOccurrence; ++skipped) {
			column = row.find(aWord, column + 1U);
		}
		if (column == std::string_view::npos) {
			std::cerr << "FAILED: '" << aWord << "' is not on line " << aLine << "\n";
			std::exit(EXIT_FAILURE);
		}
		const auto matches = std::count_if(
			aSpans.begin(),
			aSpans.end(),
			[&](const app::token_span& aSpan) {
				return aSpan.Line == aLine && aSpan.Column == column
					&& aSpan.Length == aWord.size() && aSpan.Role == aRole;
			});
		if (matches != 1) {
			std::cerr << "spans on line " << aLine << " ('" << aWord << "' at column "
					  << column << ", expected role " << static_cast<int>(aRole) << "):\n";
			for (const auto& span : aSpans) {
				if (span.Line == aLine) {
					std::cerr << "  column " << span.Column << " length " << span.Length
							  << " role " << static_cast<int>(span.Role) << "\n";
				}
			}
		}
		return matches == 1;
	}

	/// \brief Reports whether no span on the line carries a heuristic role for
	/// the named word.
	[[nodiscard]] auto word_stays_normal(
		const app::token_span_list& aSpans,
		const std::string_view      aText,
		const std::uint32_t         aLine,
		const std::string_view      aWord) -> bool
	{
		auto begin = std::size_t{ 0U };
		for (auto line = std::uint32_t{ 0U }; line < aLine; ++line) {
			begin = aText.find('\n', begin) + 1U;
		}
		const auto end    = aText.find('\n', begin);
		const auto row    = aText.substr(begin, end == std::string_view::npos ? end : end - begin);
		const auto column = row.find(aWord);
		expect(column != std::string_view::npos, "the word is on the line");
		return std::none_of(aSpans.begin(), aSpans.end(), [&](const app::token_span& aSpan) {
			return aSpan.Line == aLine && aSpan.Column == column
				&& aSpan.Length == aWord.size()
				&& (aSpan.Role == core::appearance_role::syntax_function
					|| aSpan.Role == core::appearance_role::syntax_type
					|| aSpan.Role == core::appearance_role::syntax_preprocessor);
		});
	}

	void identifier_role_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		const auto text  = read_fixture("identifier_roles.cpp");
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(17U);
		blobs.install(oid, text);
		auto       classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		const auto definition = aCatalog.definition_for_path("identifier_roles.cpp");
		expect(definition.Name == "C++", "the fixture selects the C++ definition");
		expect((definition.Version >> 16U) == syntax::identifier_heuristic_revision,
			   "the C++ record carries the identifier post-pass revision, so a "
			   "span set cached before the post-pass is keyed away");
		expect((aCatalog.definition_named("C")->Version >> 16U)
				   == syntax::identifier_heuristic_revision,
			   "the C record carries the identifier post-pass revision");
		expect((aCatalog.definition_named("Python")->Version >> 16U) == 0U,
			   "a definition without the post-pass keeps its upstream revision");

		auto previous = definition;
		previous.Version -= syntax::identifier_heuristic_revision << 16U;
		expect(classifier.classify(request(oid, previous), { }).Status
				   == app::syntax_classification_status::unavailable,
			   "a key from before the post-pass is refused rather than served");

		const auto whole = classifier.classify(request(oid, definition), { });
		expect(whole.Status == app::syntax_classification_status::available,
			   "the identifier fixture classifies");
		const auto& spans = whole.Spans;

		using role           = core::appearance_role;
		const auto classLine = line_of(text, "BPR-CLASS-LINE");
		expect(word_has_role(spans, text, classLine, "ScreenBufferTests", role::syntax_type),
			   "a name after class is a type");

		const auto declarationLine = line_of(text, "BPR-DECLARATION-LINE");
		expect(word_has_role(spans, text, declarationLine, "TextAttribute", role::syntax_type),
			   "a PascalCase name before a reference is a type");
		expect(word_has_role(spans, text, declarationLine, "attribute", role::syntax_function),
			   "a member declaration before ( is a function");

		const auto freeLine = line_of(text, "BPR-FREE-LINE");
		expect(word_has_role(spans, text, freeLine, "ProcessString", role::syntax_function),
			   "a free function before ( is a function");
		expect(word_has_role(spans, text, freeLine, "std::wstring", role::syntax_builtin),
			   "the grammar's own role for std:: is kept, not overridden");
		expect(word_stays_normal(spans, text, freeLine, "text"),
			   "a plain parameter name stays normal");

		const auto stringLine = line_of(text, "BPR-STRING-LINE");
		expect(word_has_role(spans, text, stringLine, "Log", role::syntax_type),
			   "a scope before :: is a type");
		expect(word_has_role(spans, text, stringLine, "Comment", role::syntax_function),
			   "a scoped call is a function");
		expect(word_has_role(spans, text, stringLine,
							 "L\"BPR-STRING-LINE ProcessString(TextAttribute) MACRO_NAME\"",
							 role::syntax_string),
			   "the L prefix belongs to the string, as one span");
		expect(word_stays_normal(spans, text, stringLine, "ProcessString")
				   && word_stays_normal(spans, text, stringLine, "TextAttribute")
				   && word_stays_normal(spans, text, stringLine, "MACRO_NAME"),
			   "no heuristic role is named inside a string");

		const auto macroLine = line_of(text, "BPR-MACRO-LINE");
		expect(word_has_role(spans, text, macroLine, "VERIFY_ARE_EQUAL", role::syntax_preprocessor),
			   "an ALL_CAPS name is a macro");
		expect(word_has_role(spans, text, macroLine, "ServiceLocator", role::syntax_type),
			   "a scope in an argument is a type");
		expect(word_has_role(spans, text, macroLine, "instance", role::syntax_function)
				   && word_has_role(spans, text, macroLine, "count", role::syntax_function)
				   && word_has_role(spans, text, macroLine, "size", role::syntax_function),
			   "member calls are functions");

		const auto commentLine = line_of(text, "BPR-COMMENT-LINE");
		expect(word_stays_normal(spans, text, commentLine, "ProcessString")
				   && word_stays_normal(spans, text, commentLine, "TextAttribute")
				   && word_stays_normal(spans, text, commentLine, "VERIFY_ARE_EQUAL")
				   && word_stays_normal(spans, text, commentLine, "Log"),
			   "no heuristic role is named inside a line comment");
		const auto defineLine = line_of(text, "BPR-MACRO-DEFINE");
		expect(word_has_role(spans, text, defineLine, "#define BEGIN_TEST_METHOD_PROPERTIES",
							 role::syntax_preprocessor),
			   "a preprocessor line keeps the grammar's own span");
		expect(word_stays_normal(spans, text, defineLine, "ScreenBufferTests"),
			   "no heuristic role is named inside a block comment");
	}
} // namespace

int main()
{
	QStandardPaths::setTestModeEnabled(true);

	const auto catalog = syntax::packaged_definition_catalog{ };
	definition_selection_contract(catalog);
	complete_blob_state_contract(catalog);
	identifier_role_contract(catalog);
	plain_text_and_failure_contract(catalog);
	appearance_independence_contract(catalog);
	packaged_resource_contract(catalog);

	std::cout << "syntax-classification-contract-tests:pass\n";
	return EXIT_SUCCESS;
}
