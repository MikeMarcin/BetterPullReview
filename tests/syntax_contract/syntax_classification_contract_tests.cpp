#include "../support/test_expectation.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"

#if defined(_WIN32)
	#include "../sqlite_contract/scratch_root.hpp"
#endif

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

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
#include <utility>
#include <vector>

namespace {
	namespace app    = bpr::application;
	namespace core   = bpr::core;
	namespace syntax = bpr::adapters::syntax;

	using bpr::tests::expect;

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
		const auto expectations = std::array<expectation, 9U>{ {
			{ "multiline_comment.cpp", "C++" },
			{ "windows_resource.rc", "Windows Resource" },
			{ "WINDOWS_RESOURCE.RC", "Windows Resource" },
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
				bpr::tests::fail();
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
		const auto marker = std::string{ "BPR_DIAGNOSTIC_PRIVATE_CANARY syntax source" };
		blobs.install(blob(14U), marker + std::string(syntax::maximum_classified_blob_bytes, 'x'));
		const auto oversized = classifier.classify(request(blob(14U), aCatalog.definition_for_path("source.cpp")), { });
		expect(oversized.Status == app::syntax_classification_status::limit_exceeded && oversized.Spans.empty(), "oversized canary source produces only a typed syntax refusal");
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
			bpr::tests::fail();
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

		// [[BPR-1126]]. At a name nobody could have taken first, owner-private,
		// and removed only by this course. It was `bpr-syntax-canary-control`
		// in the temporary directory, and whatever was there was removed first.
#if defined(_WIN32)
		// Qt gives a temporary directory on Windows a list of its own, which
		// names no AppContainer. This row runs in one, so it could neither
		// write in that root nor remove it. The SQLite contracts' helper makes
		// the root with the platform's owner-private descriptor, which also
		// grants the container the process runs in.
		const auto control = bpr::tests::sqlite_contract::scratch_root{ "syntax-canary-control" };
		write_canary(control.path() / "syntax");
		auto controlRepository = KSyntaxHighlighting::Repository{ };
		controlRepository.addCustomSearchPath(QString::fromStdWString(control.path().native()));
#else
		const auto control = QTemporaryDir{ QDir::tempPath() + QStringLiteral("/bpr-syntax-canary-control-XXXXXX") };
		expect(control.isValid(), "the control root is made at a name nothing had");
		write_canary(std::filesystem::path{ control.path().toStdU16String() } / "syntax");
		auto controlRepository = KSyntaxHighlighting::Repository{ };
		controlRepository.addCustomSearchPath(control.path());
#endif
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

	/// \brief Returns the text of one zero-based line, without its terminator.
	[[nodiscard]] auto row_of(const std::string_view aText, const std::uint32_t aLine)
		-> std::string_view
	{
		auto begin = std::size_t{ 0U };
		for (auto line = std::uint32_t{ 0U }; line < aLine; ++line) {
			begin = aText.find('\n', begin) + 1U;
		}
		const auto end = aText.find('\n', begin);
		return aText.substr(begin, end == std::string_view::npos ? end : end - begin);
	}

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
		const auto row    = row_of(aText, aLine);
		auto       column = row.find(aWord);
		for (auto skipped = std::size_t{ 0U }; skipped < aOccurrence; ++skipped) {
			column = row.find(aWord, column + 1U);
		}
		if (column == std::string_view::npos) {
			std::cerr << "FAILED: '" << aWord << "' is not on line " << aLine << "\n";
			bpr::tests::fail();
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

	/// \brief Reports whether one span of the supplied role covers the whole of
	/// the named word.
	///
	/// \details A directive keyword is not a span of its own: it sits inside the
	/// longer grey run that reaches to the first argument. This is how such a
	/// word is pinned, while `word_has_role` pins a word that must own its exact
	/// range.
	[[nodiscard]] auto word_inside_role(
		const app::token_span_list& aSpans,
		const std::string_view      aText,
		const std::uint32_t         aLine,
		const std::string_view      aWord,
		const core::appearance_role aRole) -> bool
	{
		const auto row    = row_of(aText, aLine);
		const auto column = row.find(aWord);
		expect(column != std::string_view::npos, "the word is on the line");
		const auto first = static_cast<std::uint32_t>(column);
		const auto last  = first + static_cast<std::uint32_t>(aWord.size());
		return std::any_of(aSpans.begin(), aSpans.end(), [&](const app::token_span& aSpan) {
			return aSpan.Line == aLine && aSpan.Role == aRole && aSpan.Column <= first
				&& aSpan.Column + aSpan.Length >= last;
		});
	}

	/// \brief Reports whether no span on the line carries a heuristic role for
	/// the named word.
	[[nodiscard]] auto word_stays_normal(
		const app::token_span_list& aSpans,
		const std::string_view      aText,
		const std::uint32_t         aLine,
		const std::string_view      aWord) -> bool
	{
		const auto row    = row_of(aText, aLine);
		const auto column = row.find(aWord);
		expect(column != std::string_view::npos, "the word is on the line");
		return std::none_of(aSpans.begin(), aSpans.end(), [&](const app::token_span& aSpan) {
			return aSpan.Line == aLine && aSpan.Column == column
				&& aSpan.Length == aWord.size()
				&& (aSpan.Role == core::appearance_role::syntax_function
					|| aSpan.Role == core::appearance_role::syntax_type
					|| aSpan.Role == core::appearance_role::syntax_macro
					|| aSpan.Role == core::appearance_role::syntax_preprocessor);
		});
	}

	/// \brief [[BPR-0744]]: a directive keyword is grey and its arguments are
	/// not.
	///
	/// \details This reverses what [[BPR-0726]] pinned here. The owner's
	/// direction of 2026-09-11 is that the directive itself stays grey while an
	/// ALL_CAPS argument paints as the macro it is and an `#include` header name
	/// paints as the reference paints it, which is the string role. Everything
	/// else a directive still covers — `once`, `defined`, the operators, the
	/// parentheses and the words of an `#error` message — stays grey.
	void directive_role_contract(
		const std::string_view      aText,
		const app::token_span_list& aSpans)
	{
		using role = core::appearance_role;

		const auto includeAngle = line_of(aText, "BPR-INCLUDE-ANGLE");
		expect(word_inside_role(aSpans, aText, includeAngle, "#include", role::syntax_preprocessor),
			   "the #include keyword stays the directive grey");
		expect(word_has_role(aSpans, aText, includeAngle, "<string>", role::syntax_string),
			   "an angle-bracketed include path is the string role at its own range");

		const auto includeQuoted = line_of(aText, "BPR-INCLUDE-QUOTED");
		expect(word_inside_role(aSpans, aText, includeQuoted, "#include", role::syntax_preprocessor),
			   "the #include keyword stays grey on a quoted include too");
		expect(word_has_role(aSpans, aText, includeQuoted, "\"fixture/detail/config.hpp\"",
							 role::syntax_string),
			   "a quoted include path is the string role at its own range");

		const auto ifndefLine = line_of(aText, "BPR-IFNDEF-LINE");
		expect(word_inside_role(aSpans, aText, ifndefLine, "#ifndef", role::syntax_preprocessor),
			   "the #ifndef keyword is the directive grey");
		expect(word_has_role(aSpans, aText, ifndefLine, "BPR_FIXTURE_GUARD", role::syntax_macro),
			   "the name after #ifndef is a macro at its own exact range");

		const auto defineGuard = line_of(aText, "BPR-DEFINE-GUARD");
		expect(word_inside_role(aSpans, aText, defineGuard, "#define", role::syntax_preprocessor),
			   "the #define keyword is the directive grey");
		expect(word_has_role(aSpans, aText, defineGuard, "BPR_FIXTURE_GUARD", role::syntax_macro),
			   "the name after #define is a macro at its own exact range");
		expect(word_has_role(aSpans, aText, defineGuard, "1", role::syntax_number),
			   "a numeric replacement keeps the grammar's own number role");

		const auto defineLine = line_of(aText, "BPR-MACRO-DEFINE");
		expect(word_inside_role(aSpans, aText, defineLine, "#define", role::syntax_preprocessor),
			   "the #define keyword of a function-like macro is grey");
		expect(word_has_role(aSpans, aText, defineLine, "BEGIN_TEST_METHOD_PROPERTIES",
							 role::syntax_macro),
			   "a function-like macro name is a macro at its own exact range");

		const auto ifLine = line_of(aText, "BPR-IF-LINE");
		expect(word_inside_role(aSpans, aText, ifLine, "#if", role::syntax_preprocessor),
			   "the #if keyword is the directive grey");
		expect(word_has_role(aSpans, aText, ifLine, "BPR_FIXTURE_USE_MODULES", role::syntax_macro)
				   && word_has_role(aSpans, aText, ifLine, "BPR_FIXTURE_LEGACY", role::syntax_macro),
			   "every ALL_CAPS word of a condition is a macro at its own range");
		expect(word_inside_role(aSpans, aText, ifLine, "&&", role::syntax_preprocessor)
				   && word_inside_role(aSpans, aText, ifLine, "defined", role::syntax_preprocessor),
			   "the operators and `defined` of a condition stay grey");

		const auto pragmaLine = line_of(aText, "BPR-PRAGMA-LINE");
		expect(word_inside_role(aSpans, aText, pragmaLine, "#pragma once",
								role::syntax_preprocessor),
			   "`once` after #pragma stays grey rather than taking a role of its own");

		const auto errorLine = line_of(aText, "BPR-ERROR-LINE");
		expect(word_inside_role(aSpans, aText, errorLine,
								"\"BPR-ERROR-LINE MODULES are UNSUPPORTED\"",
								role::syntax_preprocessor),
			   "the words of an #error message stay grey, never macros");

		// One line carries all three populations of predefined name, so the
		// distinction between them cannot drift unnoticed. `__GNUC__` is one the
		// packaged grammar lists and styles for itself; before [[BPR-0744]] that
		// left it a near-grey `syntax_other` on the same line where `__APPLE__`
		// was already purple, which is the artefact this pins shut.
		const auto standardLine = line_of(aText, "BPR-STDMACRO-LINE");
		expect(word_has_role(aSpans, aText, standardLine, "__GNUC__", role::syntax_macro),
			   "an ALL_CAPS predefined macro is a macro, not an unclassified other");
		expect(word_has_role(aSpans, aText, standardLine, "__cplusplus", role::syntax_macro)
				   && word_has_role(aSpans, aText, standardLine, "__clang__", role::syntax_macro),
			   "conditional macro names are purple regardless of capitalization");
		for (const auto marker : { "BPR-LOWER-IFDEF", "BPR-LOWER-IFNDEF", "BPR-LOWER-ELIF", "BPR-LOWER-UNDEF" }) {
			expect(word_has_role(aSpans, aText, line_of(aText, marker), "mixedCase", role::syntax_macro),
				   "each macro directive recognizes mixed-case names");
		}
		const auto operators = line_of(aText, "BPR-CONDITION-OPERATORS");
		for (const auto word : { "defined", "true", "false", "__has_include", "__has_cpp_attribute", "__has_builtin" }) {
			expect(word_inside_role(aSpans, aText, operators, word, role::syntax_preprocessor),
				   "conditional operators and boolean literals retain their directive role");
		}
		for (const auto word : { "lowercase", "other_name", "bare_name", "__has_private" }) {
			expect(word_has_role(aSpans, aText, operators, word, role::syntax_macro),
				   "macro names around builtin tests retain exact macro ranges");
		}
		for (const auto word : { "nodiscard", "__builtin_expect", "and", "'x'", "L'y'" }) {
			expect(word_inside_role(aSpans, aText, operators, word, role::syntax_preprocessor),
				   "attribute and builtin-test operands are not macros");
		}
		expect(word_inside_role(aSpans, aText, operators, "<vector>", role::syntax_preprocessor),
			   "a tested header retains its packaged directive role");
		const auto commentOperand   = line_of(aText, "BPR-COMMENT-OPERAND");
		const auto commentProtected = word_inside_role(aSpans, aText, commentOperand, "nodiscard", role::syntax_preprocessor);
		const auto continued        = line_of(aText, "BPR-CONTINUED-CONDITION");
		const auto continuedMacro   = word_has_role(aSpans, aText, continued, "__clang__", role::syntax_macro);
		if (!commentProtected) {
			std::cerr << "FAILED: a comment between builtin test and operand must not make its operand a macro\n";
		}
		expect(commentProtected && continuedMacro,
			   "comment-separated builtin operands and continued conditional macros retain their roles");
		expect(word_has_role(aSpans, aText, continued, "__cplusplus", role::syntax_macro),
			   "predefined lowercase macros retain their role across continuations");
		for (const auto marker : { "BPR-COMMENT-OPERAND", "BPR-CONTINUED-TEST-OPEN", "BPR-CONTINUED-TEST-COMMENT" }) {
			const auto line = line_of(aText, marker);
			expect(word_inside_role(aSpans, aText, line, "nodiscard", role::syntax_preprocessor)
					   && word_has_role(aSpans, aText, line, "after_test", role::syntax_macro),
				   "builtin protection spans comments and continuations and ends at the closing parenthesis");
		}
		const auto continuedOperand = line_of(aText, "BPR-CONTINUED-TEST-OPERAND");
		expect(word_inside_role(aSpans, aText, continuedOperand, "__builtin_expect", role::syntax_preprocessor)
				   && word_has_role(aSpans, aText, continuedOperand, "after_test", role::syntax_macro),
			   "a builtin operand continued after its opening remains protected");
		const auto replacement = line_of(aText, "BPR-REPLACEMENT");
		expect(!word_inside_role(aSpans, aText, replacement, "ordinary_name", role::syntax_macro),
			   "define replacement identifiers are not broadened");
	}

	/// \brief [[BPR-0747]]: the `//` comment of a null directive is a comment,
	/// and a malformed directive is still an error.
	///
	/// \details Both grammars paint the rest of a line as an error when no
	/// directive keyword follows its `#`, which is how Qt's aligned
	/// `#        // …` read as salmon. The two controls reach the same error
	/// state by its neighbouring paths: a `#define` with no name meets the
	/// same `//`, and an unknown directive carries its trailing comment into
	/// its error run.
	void null_directive_contract(
		const std::string_view      aDefinition,
		const std::string_view      aText,
		const app::token_span_list& aSpans)
	{
		using role           = core::appearance_role;
		const auto expectFor = [aDefinition](const bool aCondition, const std::string_view aMessage) {
			if (!aCondition) {
				std::cerr << aDefinition << ": ";
			}
			expect(aCondition, aMessage);
		};

		const auto nullLine = line_of(aText, "BPR-NULL-DIRECTIVE-COMMENT");
		expectFor(word_has_role(aSpans, aText, nullLine,
								"// BPR-NULL-DIRECTIVE-COMMENT TARGET_OS_IOS is only available in newer SDKs,",
								role::syntax_comment),
				  "a comment after a null directive is a comment at its own exact range");
		expectFor(word_inside_role(aSpans, aText, nullLine, "#", role::syntax_preprocessor),
				  "the # of a null directive is the directive grey");
		expectFor(word_has_role(aSpans, aText, line_of(aText, "BPR-MALFORMED-DEFINE"),
								"// BPR-MALFORMED-DEFINE", role::syntax_error),
				  "a #define without a name stays an error where a comment follows it");
		expectFor(word_has_role(aSpans, aText, line_of(aText, "BPR-UNKNOWN-DIRECTIVE"),
								"bogus BPR_FIXTURE_GUARD // BPR-UNKNOWN-DIRECTIVE", role::syntax_error),
				  "an unknown directive stays an error through its trailing comment");
	}

	void builtin_type_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		using role      = core::appearance_role;
		const auto text = std::string{
			"void CUIforETWDlg::Update() {}\n"
			"bool enabled;\n"
			"wchar_t character;\n"
			"NameToString names;\n"
			"widget_t widget;\n"
			"value_type value;\n"
			"widget_t anotherWidget;\n"
			"value_type anotherValue;\n"
		};
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(22U);
		blobs.install(oid, text);
		auto classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		for (const auto name : { "C", "C++" }) {
			const auto definition = *aCatalog.definition_named(name);
			const auto result     = classifier.classify(request(oid, definition), { });
			expect(result.Status == app::syntax_classification_status::available, "C-family types classify");
			for (const auto word : { "void", "bool", "wchar_t" }) {
				expect(word_has_role(result.Spans, text, line_of(text, word), word, role::syntax_builtin),
					   "C-family language types use exact built-in spans");
			}
			for (const auto word : { "CUIforETWDlg", "NameToString" }) {
				expect(word_has_role(result.Spans, text, line_of(text, word), word, role::syntax_type),
					   "inferred user types keep exact type spans");
			}
			expect(word_has_role(result.Spans, text, 0U, "Update", role::syntax_function),
				   "function roles remain distinct from built-in and user types");
			if (std::string_view{ name } == "C++") {
				for (const auto line : { 4U, 6U }) {
					expect(word_has_role(result.Spans, text, line, "widget_t", role::syntax_type),
						   "upstream *_t DataType spans remain exact user-type spans");
				}
				for (const auto line : { 5U, 7U }) {
					expect(word_has_role(result.Spans, text, line, "value_type", role::syntax_type),
						   "upstream *_type DataType spans remain exact user-type spans");
				}
			}
			auto previous = definition;
			previous.Version -= 1U << 16U;
			expect(classifier.classify(request(oid, previous), { }).Status == app::syntax_classification_status::unavailable,
				   "the immediately previous heuristic revision is refused");
		}
	}

	void windows_resource_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		using role            = core::appearance_role;
		const auto text       = read_fixture("windows_resource.rc");
		const auto definition = aCatalog.definition_for_path("windows_resource.rc");
		expect(aCatalog.definition_named("Windows Resource") == definition,
			   "the resource identity resolves by name for classification and cache reuse");
		expect(definition.Version == aCatalog.definition_named("C")->Version,
			   "resource revision binds both the packaged C grammar and current heuristic revision");
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(23U);
		blobs.install(oid, text);
		auto       classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		const auto result     = classifier.classify(request(oid, definition), { });
		expect(result.Status == app::syntax_classification_status::available, "resource fixture classifies");
		const auto& spans = result.Spans;
		expect(word_has_role(spans, text, 1U, "\"Resource.h\"", role::syntax_string)
				   && word_inside_role(spans, text, 1U, "#include", role::syntax_preprocessor),
			   "resource quoted include and directive preserve independent ranges");
		expect(word_has_role(spans, text, 4U, "<windows.h>", role::syntax_string),
			   "resource angle include is a string");
		for (const auto line : { 2U, 3U }) {
			expect(word_has_role(spans, text, line, "APSTUDIO_INVOKED", role::syntax_macro),
				   "resource directive macro operands keep exact macro spans");
		}
		struct keyword_expectation
		{
			std::uint32_t    Line;
			std::string_view Word;
		};
		const auto keywords = std::array<keyword_expectation, 16U>{ { { 7U, "LANGUAGE" }, { 8U, "ICON" }, { 9U, "MENU" }, { 10U, "BEGIN" }, { 11U, "POPUP" }, { 13U, "MENUITEM" }, { 14U, "END" }, { 16U, "ACCELERATORS" }, { 18U, "ASCII" }, { 18U, "ALT" }, { 20U, "DIALOGEX" }, { 21U, "STYLE" }, { 22U, "CAPTION" }, { 23U, "FONT" }, { 25U, "LTEXT" }, { 26U, "PUSHBUTTON" } } };
		for (const auto& keyword : keywords) {
			expect(word_has_role(spans, text, keyword.Line, keyword.Word, role::syntax_keyword), "resource keywords have exact keyword spans");
		}
		expect(word_has_role(spans, text, 8U, "IDI_SMALL", role::syntax_macro),
			   "ALL_CAPS resource identifiers retain their distinct macro role");
		for (const auto marker : { "MainMenu MENU", "MainKeys ACCELERATORS", "MainDialog DIALOGEX" }) {
			const auto word   = std::string_view{ marker }.substr(0, std::string_view{ marker }.find(' '));
			const auto line   = line_of(text, marker);
			const auto column = row_of(text, line).find(word);
			expect(std::none_of(spans.begin(), spans.end(), [&](const auto& span) {
					   return span.Line == line && span.Column < column + word.size() && span.Column + span.Length > column;
				   }),
				   "ordinary mixed-case resource identifiers have no styled overlap");
		}
		expect(word_has_role(spans, text, 0U, row_of(text, 0U), role::syntax_comment),
			   "resource // comment stays wholly a comment despite embedded keywords");
		expect(word_has_role(spans, text, 11U, "\"MENU BEGIN café\"", role::syntax_string),
			   "resource strings are not rescanned for keywords");
		const auto utf8Line = line_of(text, "UTF-8 offset");
		expect(word_has_role(spans, text, utf8Line, "MENUITEM", role::syntax_keyword),
			   "resource keyword offsets remain UTF-8 byte offsets after a multibyte string");
		expect(word_has_role(spans, text, line_of(text, "#if MENU"), "MENU", role::syntax_macro)
				   && word_has_role(spans, text, 6U, "MENU", role::syntax_macro, 1U),
			   "resource keywords in directives remain macro operands");
		expect(std::none_of(spans.begin(), spans.end(), [](const auto& span) { return span.Role == role::syntax_type; }),
			   "resource identifiers do not acquire blanket type roles");
		const auto commentLine = line_of(text, "LANGUAGE POPUP */");
		const auto hunk        = classifier.classify_hunk(request(oid, definition), { .First = commentLine, .Count = 1U }, { });
		expect(line_is_wholly(hunk.Spans, commentLine, length_of_line(text, commentLine), role::syntax_comment),
			   "a resource hunk retains preceding block comment state");
		const auto asC = classifier.classify(request(oid, *aCatalog.definition_named("C")), { });
		expect(word_has_role(asC.Spans, text, 7U, "LANGUAGE", role::syntax_macro),
			   "resource keyword refinement never affects ordinary C");
		expect(classifier.classify(request(oid, definition, 1U), { }).Status
				   == app::syntax_classification_status::limit_exceeded,
			   "resource refinement respects the span bound");
		blobs.install(oid, std::string{ "\xFF", 1U });
		const auto malformed = classifier.classify(request(oid, definition), { });
		expect(malformed.Status == app::syntax_classification_status::unavailable && malformed.Spans.empty(),
			   "invalid UTF-8 resource bytes fail safely without invented offsets");
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
		expect((definition.Version >> 16U) == syntax::cpp_identifier_heuristic_revision,
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
		expect(word_has_role(spans, text, macroLine, "VERIFY_ARE_EQUAL", role::syntax_macro),
			   "an ALL_CAPS name is a macro, in its own role");
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
		expect(word_stays_normal(spans, text, defineLine, "ScreenBufferTests"),
			   "no heuristic role is named inside a block comment");

		directive_role_contract(text, spans);
		null_directive_contract("C++", text, spans);

		// The C grammar reaches its error state by the same path, and the
		// post-pass gate covers both definitions.
		const auto asC = classifier.classify(request(oid, *aCatalog.definition_named("C")), { });
		expect(asC.Status == app::syntax_classification_status::available,
			   "the identifier fixture classifies under the C definition");
		null_directive_contract("C", text, asC.Spans);
	}

	void cpp_context_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		using role       = core::appearance_role;
		const auto text  = read_fixture("cpp_identifier_context.cpp");
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(32U);
		blobs.install(oid, text);
		auto       classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		const auto definition = aCatalog.definition_for_path("context.cpp");
		const auto whole      = classifier.classify(request(oid, definition), { });
		expect(whole.Status == app::syntax_classification_status::available, "C++ context fixture is available");
		const auto has = [&](const std::string_view aMarker, const std::string_view aWord, const role aRole) {
			return word_inside_role(whole.Spans, text, line_of(text, aMarker), aWord, aRole);
		};
		expect(has("namespace GCT", "GCT", role::syntax_namespace)
				   && has("::Network {", "Network", role::syntax_namespace)
				   && has("namespace simple", "simple", role::syntax_namespace),
			   "multiline uppercase and mixed nested namespaces have namespace roles");
		expect(has("namespace Alias", "Alias", role::syntax_namespace), "namespace alias name has a namespace role without alias identity propagation");
		expect(has("using namespace GCT", "GCT", role::syntax_namespace)
				   && has("safe split", "Network", role::syntax_namespace)
				   && has("using namespace External", "NAMESPACE", role::syntax_namespace),
			   "using namespace chains retain structural roles across comments and newlines");
		for (const auto name : { "Node", "ID", "Player", "DNA" }) {
			const auto marker = std::string{ "    " } + name;
			expect(has(marker, name, role::syntax_enum_member), "every supplied enumerator has the independent enum-member role");
		}
		for (const auto name : { "FIRST", "SECOND", "THIRD", "FOURTH", "FIFTH", "LAST" }) {
			const auto marker = std::string{ "    " } + name;
			expect(has(marker, name, role::syntax_enum_member), "nested initializers and attributes preserve enumerator boundaries");
		}
		expect(has("enum class Reason", "Reason", role::syntax_type), "enum name keeps its type role");
		expect(has("FIRST =", "ENUM_VALUE", role::syntax_macro)
				   && has("SECOND =", "ENUM_VALUE", role::syntax_macro)
				   && has("GCT_ASSERT", "GCT_ASSERT", role::syntax_macro)
				   && has("auto standalone", "ENUM_VALUE", role::syntax_macro)
				   && has("auto predefined", "__GNUC__", role::syntax_macro)
				   && has("#define ENUM_VALUE", "ENUM_VALUE", role::syntax_macro),
			   "true macro references, calls, directive operands and predefined macros retain roles");
		expect(!has("THIRD =", "char", role::syntax_enum_member), "template initializer operands do not become enumerators");
		expect(has("enum Lambda", "NEXT", role::syntax_enum_member), "an initializer lambda's local shadow does not lose the following enumerator");
		for (const auto& [markerText, member] : std::array<std::pair<std::string_view, std::string_view>, 8U>{ { { "auto chosen", "ID" }, { "auto dna", "DNA" }, { "auto first", "FIRST" }, { "auto qualified", "DNA" }, { "auto lower", "Lower" }, { "auto reopened", "Player" }, { "auto visible", "ID" }, { "namespace simple", "UPPER" } } }) {
			expect(has(markerText, member, role::syntax_enum_member), "qualified enum identity matches declaration scope and membership");
		}
		expect(has("auto alias_qualified", "Alias", role::syntax_namespace), "namespace alias use has a namespace role without propagating its target");
		expect(has("auto qualified", "GCT", role::syntax_namespace)
				   && has("auto qualified", "Network", role::syntax_namespace),
			   "recognized namespace qualifiers keep namespace roles");
		for (const auto markerText : { "auto unrelated", "auto unknown", "auto after_comment", "auto hidden =", "auto hidden_parameter", "auto alias_unknown", "auto ambiguous", "auto duplicate_member", "auto alias_qualified", "auto template_unknown", "auto object_qualified", "enum Lambda" }) {
			const auto name = std::string_view{ markerText }.find("unrelated") != std::string_view::npos
					|| std::string_view{ markerText }.find("comment") != std::string_view::npos
				? "DNA"
				: "ID";
			expect(!has(markerText, name, role::syntax_enum_member) && !has(markerText, name, role::syntax_macro),
				   "unrelated, unknown, shadowed, aliased and ambiguous qualified names remain neutral");
		}
		expect(has("auto qualifier", "UPPER_SCOPE", role::syntax_type)
				   && has("auto qualified_call", "CALL_MACRO", role::syntax_macro)
				   && has("auto ordinary_call", "function", role::syntax_function),
			   "uncertain qualifiers precede uppercase fallback while invocations retain conventions");
		expect(has("auto after =", "AFTER_ENUM", role::syntax_macro), "enum context ends at its matching brace");
		expect(!has("#define LOOKALIKE", "FAKE", role::syntax_namespace)
				   && !has("namespace FALSE_NAMESPACE", "FALSE_NAMESPACE", role::syntax_namespace)
				   && !has("namespace RAW", "RAW", role::syntax_namespace)
				   && !has("/* enum class Comment", "COMMENT", role::syntax_namespace),
			   "directives, continuations, raw strings and comments cannot supply structural facts");
		const auto hunkLine  = line_of(text, "    ID = 1");
		const auto hunk      = classifier.classify_hunk(request(oid, definition), { hunkLine, 3U }, { });
		auto       projected = app::token_span_list{ };
		std::copy_if(whole.Spans.begin(), whole.Spans.end(), std::back_inserter(projected), [hunkLine](const auto& aSpan) {
			return aSpan.Line >= hunkLine && aSpan.Line < hunkLine + 3U;
		});
		expect(hunk.Spans == projected, "hunk beginning after an enum opener exactly projects whole-blob roles");
		auto old    = definition;
		old.Version = (syntax::identifier_heuristic_revision << 16U) | (old.Version & 0xFFFFU);
		expect(classifier.classify(request(oid, old), { }).Status == app::syntax_classification_status::unavailable, "old C++ identifier cache keys are refused");
		const auto bounded = classifier.classify(request(oid, definition, 1U), { });
		expect(bounded.Status == app::syntax_classification_status::limit_exceeded && bounded.Spans.empty(), "span exhaustion publishes no partial roles");
		auto stop = std::stop_source{ };
		stop.request_stop();
		const auto cancelled = classifier.classify(request(oid, definition), stop.get_token());
		expect(cancelled.Status == app::syntax_classification_status::cancelled && cancelled.Spans.empty(), "cancellation publishes no partial roles");
		const auto asC = classifier.classify(request(oid, *aCatalog.definition_named("C")), { });
		expect(!word_inside_role(asC.Spans, text, line_of(text, "    ID = 1"), "ID", role::syntax_enum_member), "C retains its previous roles");
		std::cout << "cpp-context-contract:pass declarations/uses/hunks/protected/macro/cache/bounds\n";
	}

	/// \brief [[BPR-1474]]: RFC-BPR-0081's confirmed gap classes, each through its
	/// committed gap fixture. Every expectation failed on revision 6.
	void cpp_gap_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		using role            = core::appearance_role;
		auto       blobs      = fixture_blobs{ };
		auto       classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		auto       seed       = std::uint8_t{ 96U };
		const auto definition = aCatalog.definition_for_path("gap.cpp");
		const auto classify   = [&](const std::string_view aName) {
			auto       text = read_fixture(std::string{ "evaluation/gap/" } + std::string{ aName });
			const auto oid  = blob(seed++);
			blobs.install(oid, text);
			auto result = classifier.classify(request(oid, definition), { });
			expect(result.Status == app::syntax_classification_status::available, "the gap fixture classifies");
			return std::pair{ std::move(text), std::move(result.Spans) };
		};
		const auto exact = [](const auto& aRun, const std::string_view aMarker, const std::string_view aWord, const role aRole) {
			return word_has_role(aRun.second, aRun.first, line_of(aRun.first, aMarker), aWord, aRole);
		};
		const auto structural = [](const auto& aRun, const std::string_view aMarker, const std::string_view aWord) {
			const auto line = line_of(aRun.first, aMarker);
			return word_inside_role(aRun.second, aRun.first, line, aWord, role::syntax_enum_member)
				|| word_inside_role(aRun.second, aRun.first, line, aWord, role::syntax_namespace);
		};

		const auto h1 = classify("h1_inline_namespace.cpp");
		expect(exact(h1, "namespace A::inline B", "A", role::syntax_namespace)
				   && exact(h1, "namespace A::inline B", "B", role::syntax_namespace)
				   && !structural(h1, "namespace A::inline B", "inline"),
			   "H1: an inline nested namespace head names both namespaces");
		expect(exact(h1, "auto mode", "A", role::syntax_namespace) && exact(h1, "auto mode", "B", role::syntax_namespace)
				   && exact(h1, "auto mode", "Fast", role::syntax_enum_member),
			   "H1: a use through the inline namespace resolves");

		const auto h2 = classify("h2_attributed_namespace.cpp");
		expect(exact(h2, "namespace [[deprecated]]", "Legacy", role::syntax_namespace)
				   && exact(h2, "auto code", "Legacy", role::syntax_namespace)
				   && exact(h2, "auto code", "FAILED", role::syntax_enum_member),
			   "H2: an attributed namespace head is recognized");

		const auto h3 = classify("h3_enum_comparisons.cpp");
		expect(exact(h3, "enum Limits", "High", role::syntax_enum_member) && exact(h3, "enum Limits", "Last", role::syntax_enum_member)
				   && exact(h3, "auto limit", "High", role::syntax_enum_member) && !structural(h3, "enum Limits", "Y"),
			   "H3: comparisons in initializers are not a template argument list");

		const auto h4 = classify("h4_x_macro.cpp");
		expect(exact(h4, "enum Colour", "COLOUR_LIST", role::syntax_macro) && !structural(h4, "auto colour", "RED"),
			   "H4: an X-macro invocation in an enum body keeps its macro role");

		const auto h5 = classify("h5_conditional_enum.cpp");
		expect(exact(h5, "\tIdle", "Idle", role::syntax_enum_member) && exact(h5, "\tBusy", "Busy", role::syntax_enum_member),
			   "H5: the enum opened in both branches has its enumerators");
		expect(!structural(h5, "struct Pair", "second") && !structural(h5, "int a, b, c;", "b")
				   && !structural(h5, "int a, b, c;", "c"),
			   "H5: the enum opened in both branches closes at its brace");

		const auto h6 = classify("h6_brace_shadow.cpp");
		expect(!structural(h6, "auto hidden", "ID") && exact(h6, "auto visible", "DNA", role::syntax_enum_member),
			   "H6: a brace-initialized local shadows the enum");

		const auto h7 = classify("h7_non_ascii.cpp");
		expect(exact(h7, "enum Drink", "caf\xC3\xA9", role::syntax_enum_member)
				   && exact(h7, "enum Drink", "th\xC3\xA9", role::syntax_enum_member)
				   && exact(h7, "auto drink", "caf\xC3\xA9", role::syntax_enum_member),
			   "H7: a non-ASCII enumerator is painted whole");

		const auto guards = classify("extern_c_guards.cpp");
		expect(exact(guards, "auto flag", "FLAG_READ", role::syntax_enum_member) && exact(guards, "auto tail", "END", role::syntax_enum_member),
			   "an enum inside a split extern \"C\" block is found from outside it");

		auto old    = definition;
		old.Version = ((syntax::cpp_identifier_heuristic_revision - 1U) << 16U) | (old.Version & 0xFFFFU);
		expect(classifier.classify(request(blob(seed), old), { }).Status == app::syntax_classification_status::unavailable,
			   "spans cached under the previous revision are refused");
		std::cout << "cpp-gap-contract:pass H1-H7 extern-c revision\n";
	}

	/// \brief [[BPR-1559]]: template calls, arguments and template types, through
	/// the public classifier over a committed corpus. Every positive expectation
	/// failed on revision 7.
	void cpp_template_contract(const syntax::packaged_definition_catalog& aCatalog)
	{
		using role       = core::appearance_role;
		const auto text  = read_fixture("cpp_templates.cpp");
		auto       blobs = fixture_blobs{ };
		const auto oid   = blob(140U);
		blobs.install(oid, text);
		auto       classifier = syntax::packaged_syntax_classifier{ aCatalog, blobs };
		const auto definition = aCatalog.definition_for_path("templates.cpp");
		const auto whole      = classifier.classify(request(oid, definition), { });
		expect(whole.Status == app::syntax_classification_status::available, "the template fixture classifies");
		const auto is = [&](const std::string_view aMarker, const std::string_view aWord, const role aRole,
							const std::size_t aOccurrence = 0U) {
			return word_has_role(whole.Spans, text, line_of(text, aMarker), aWord, aRole, aOccurrence);
		};
		const auto function = role::syntax_function;
		const auto type     = role::syntax_type;

		expect(is("T10", "getComponent", function) && is("T10", "Type", type),
			   "the owner's report: a free template call names its function and its type argument");
		expect(is("T11", "getComponent", function) && is("T11", "Type", type)
				   && is("T12", "getComponent", function) && is("T12", "Type", type),
			   "member template calls through . and -> are functions with type arguments");
		expect(is("T13", "engine", role::syntax_namespace) && is("T13", "getComponent", function)
				   && is("T13", "Type", type),
			   "a qualified template call keeps its namespace and names function and argument");
		expect(is("T14", "get", function) && is("T14", "Node", type),
			   "a call disambiguated by the template keyword is a function");
		expect(is("T15", "make", function) && is("T15", "int", role::syntax_builtin),
			   "a built-in type argument keeps the built-in role");
		expect(is("T16", "ns", type) && is("T16", "Kind", type), "a qualified argument is a type at every step");
		expect(is("T17", "Outer", type) && is("T17", "Inner", type) && is("T17", "A", type) && is("T17", "B", type),
			   "nested argument lists, closed by >>>, name every argument a type");
		expect(is("T18", "Factory", type) && is("T18", "Widget", type) && is("T18", "create", function),
			   "a static member call through a template names the template, its argument and the call");
		expect(is("T19", "Widget", type) && is("T19", "Gadget", type) && is("T19", "const", role::syntax_attribute),
			   "pointer, reference and cv-qualified arguments are types");
		expect(is("T20", "Shape", type) && is("T20", "3", role::syntax_number) && is("T20", "MAX_SIZE", role::syntax_macro),
			   "non-type arguments keep their own roles: a number and an ALL_CAPS constant");
		expect(is("T21", "std::make_unique", role::syntax_builtin) && is("T21", "Widget", type),
			   "the grammar's role for a std:: name is kept and its argument is a type");
		expect(is("T22", "Base", type), "a cast's target type is a type");
		expect(is("T23", "Result", type), "a function type's result is a type, not a call");

		expect(is("T00", "Component", type) && is("T00", "getComponent", function),
			   "a namespace-scope template declaration names its type and function");
		expect(is("T01", "T", type, 1U) && is("T01", "getComponent", function),
			   "a template member declaration's single-capital return type is a type");
		expect(is("T02", "T", type, 1U) && is("T02", "get", function), "a single-capital pointer type is a type");
		expect(is("T03", "T", type) && is("T03", "Entity", type) && is("T03", "getComponent", function),
			   "an out-of-line template definition names return type, class and function");
		expect(is("T04", "Transform", type) && is("T04", "getComponent", function) && is("T04", "Transform", type, 1U),
			   "an explicit specialization names its function and its argument");

		expect(is("T30", "Foo", type) && is("T30", "Bar", type), "an instantiated template type and its argument are types");
		expect(is("T31", "Key", type) && is("T31", "Value", type), "arguments of nested standard templates are types");
		expect(is("T32", "Key", type) && is("T32", "Value", type), "arguments on both sides of a nested list are types");
		expect(is("T33", "object_pool", type) && is("T33", "Element", type),
			   "a lower-case template followed by a declarator is a type");
		expect(is("T34", "Registry", type) && is("T34", "Plugin", type) && is("T34", "registry", function),
			   "a reference to an instance names the template and its argument");

		// Negative control: comparisons and shifts are never template lists.
		for (const auto* const marker : { "N01", "N02", "N03", "N04", "N05", "N06", "N07", "N08", "N09" }) {
			const auto line     = line_of(text, marker);
			const auto inferred = std::count_if(whole.Spans.begin(), whole.Spans.end(), [line](const app::token_span& aSpan) {
				return aSpan.Line == line && (aSpan.Role == role::syntax_function || aSpan.Role == role::syntax_type);
			});
			if (inferred != 0) {
				std::cerr << "FAILED: comparison line " << marker << " carries an inferred role\n";
			}
			expect(inferred == 0, "a comparison or shift names no function or type");
		}

		// C has no templates, so it keeps its previous roles.
		const auto asC = classifier.classify(request(oid, *aCatalog.definition_named("C")), { });
		expect(asC.Status == app::syntax_classification_status::available, "the template fixture classifies as C");
		expect(word_stays_normal(asC.Spans, text, line_of(text, "T10"), "getComponent")
				   && word_stays_normal(asC.Spans, text, line_of(text, "T10"), "Type")
				   && word_stays_normal(asC.Spans, text, line_of(text, "T03"), "T"),
			   "C reads no template context and no single-capital type");

		auto old    = definition;
		old.Version = ((syntax::cpp_identifier_heuristic_revision - 1U) << 16U) | (old.Version & 0xFFFFU);
		expect(classifier.classify(request(oid, old), { }).Status == app::syntax_classification_status::unavailable,
			   "spans cached under revision 7 are refused");
		std::cout << "cpp-template-contract:pass calls/arguments/types/comparisons/C/revision\n";
	}

} // namespace

int main()
try {
	QStandardPaths::setTestModeEnabled(true);

	const auto catalog = syntax::packaged_definition_catalog{ };
	definition_selection_contract(catalog);
	complete_blob_state_contract(catalog);
	identifier_role_contract(catalog);
	cpp_context_contract(catalog);
	cpp_gap_contract(catalog);
	cpp_template_contract(catalog);
	builtin_type_contract(catalog);
	windows_resource_contract(catalog);
	plain_text_and_failure_contract(catalog);
	appearance_independence_contract(catalog);
	packaged_resource_contract(catalog);

	std::cout << "syntax-classification-contract-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
