// [[BPR-1793]]. The Markdown reader against cmark-gfm's own suites: every
// example of the GFM spec, the extensions test and the regression tests,
// read by the reader and compared with cmark-gfm's expected HTML read by Qt's
// HTML import, as document shape (block kind, then styled runs). The
// comparison is approximate, so it is a ratchet: each example the reader is
// known to read differently is listed with its reason in
// known-differences.json, a new difference fails, and a listed example that
// now matches fails until it is removed from the list.

#include "../support/test_expectation.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifndef BPR_CMARK_SPEC_FIXTURES
	#error "BPR_CMARK_SPEC_FIXTURES must name the cmark-gfm spec fixture directory"
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	struct spec_example
	{
		QString Name;
		QString Section;
		QString Markdown;
		QString Html;
	};

	auto read_file(const QString& aName) -> QString
	{
		QFile file{ QStringLiteral(BPR_CMARK_SPEC_FIXTURES "/") + aName };
		expect(file.open(QIODevice::ReadOnly), "fixture opens: " + aName.toStdString());
		return QString::fromUtf8(file.readAll());
	}

	/// The examples of one spec file, as cmark-gfm's spec_tests.py reads
	/// them: a run of 32 backticks and "example" opens one, a lone "." parts
	/// its Markdown from its HTML, and "→" stands for a tab. An example
	/// marked "disabled" is skipped, as cmark-gfm skips it.
	auto examples_of(const QString& aFile) -> std::vector<spec_example>
	{
		const auto                fence = QString(32, QLatin1Char{ '`' });
		std::vector<spec_example> examples;
		QString                   section;
		auto                      state = 0;
		spec_example              current;
		auto                      number   = 0;
		auto                      disabled = false;
		for (const auto& line : read_file(aFile).split(QLatin1Char{ '\n' })) {
			if (state == 0) {
				if (line.startsWith(fence + QStringLiteral(" example"))) {
					state    = 1;
					disabled = line.contains(QStringLiteral(" disabled"));
					current  = spec_example{ .Name = aFile.section(QLatin1Char{ '.' }, 0, 0) + QLatin1Char{ ':' } + QString::number(++number), .Section = section };
				} else if (line.startsWith(QLatin1Char{ '#' })) {
					section = line.section(QLatin1Char{ ' ' }, 1).trimmed();
				}
			} else if (state == 1) {
				if (line == QStringLiteral(".")) {
					state = 2;
				} else {
					current.Markdown += line + QLatin1Char{ '\n' };
				}
			} else if (line.trimmed() == fence) {
				state = 0;
				current.Markdown.replace(QChar{ 0x2192 }, QLatin1Char{ '\t' });
				current.Html.replace(QChar{ 0x2192 }, QLatin1Char{ '\t' });
				if (!disabled) {
					examples.push_back(current);
				}
			} else {
				current.Html += line + QLatin1Char{ '\n' };
			}
		}
		return examples;
	}

	auto fold(QString aText) -> QString
	{
		aText.replace(QChar::LineSeparator, QLatin1Char{ ' ' });
		aText.replace(QChar::Nbsp, QLatin1Char{ ' ' });
		aText.remove(QChar::ObjectReplacementCharacter);
		return aText.simplified();
	}

	/// A document as block kinds and styled runs.
	auto shape(QTextDocument& aDocument) -> QStringList
	{
		QStringList blocks;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			const auto format = block.blockFormat();
			const auto text   = fold(block.text());
			const auto rule   = format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
			if (!block.isVisible() || (text.isEmpty() && !rule)) {
				continue;
			}
			auto*   table = QTextCursor{ block }.currentTable();
			QString kind  = QStringLiteral("p");
			if (rule) {
				kind = QStringLiteral("hr");
			} else if (format.headingLevel() > 0) {
				kind = QStringLiteral("h%1").arg(format.headingLevel());
			} else if (format.hasProperty(QTextFormat::BlockCodeFence) || (format.nonBreakableLines() && table == nullptr)) {
				kind = QStringLiteral("code");
			}
			if (table != nullptr) {
				const auto cell = table->cellAt(block.position());
				kind += QStringLiteral(" cell%1,%2").arg(cell.row()).arg(cell.column());
			}
			if (auto* list = block.textList()) {
				const auto style   = list->format().style();
				const auto ordered = style == QTextListFormat::ListDecimal || style == QTextListFormat::ListLowerAlpha || style == QTextListFormat::ListUpperAlpha || style == QTextListFormat::ListLowerRoman
					|| style == QTextListFormat::ListUpperRoman;
				kind += QStringLiteral(" %1%2").arg(ordered ? QStringLiteral("ol") : QStringLiteral("ul")).arg(list->format().indent());
				if (format.marker() != QTextBlockFormat::MarkerType::NoMarker) {
					kind += format.marker() == QTextBlockFormat::MarkerType::Checked ? QStringLiteral(" [x]") : QStringLiteral(" [ ]");
				}
			}
			if (const auto quote = format.intProperty(QTextFormat::BlockQuoteLevel); quote > 0) {
				kind += QStringLiteral(" q%1").arg(quote);
			}
			QString runs;
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				const auto chars    = fragment.charFormat();
				QString    flags;
				if (chars.fontWeight() >= QFont::DemiBold && format.headingLevel() == 0) {
					flags += QLatin1Char{ 'B' };
				}
				if (chars.fontItalic()) {
					flags += QLatin1Char{ 'I' };
				}
				if (chars.fontStrikeOut()) {
					flags += QLatin1Char{ 'S' };
				}
				const auto families = chars.fontFamilies().toStringList().join(QLatin1Char{ ',' });
				if ((chars.fontFixedPitch() || families.contains(QStringLiteral("ourier")) || families.contains(QStringLiteral("monospace"))) && !kind.startsWith(QStringLiteral("code"))) {
					flags += QLatin1Char{ 'C' };
				}
				if (chars.isAnchor() && !chars.anchorHref().isEmpty()) {
					flags += QStringLiteral("L(") + chars.anchorHref() + QLatin1Char{ ')' };
				}
				const auto piece = fold(fragment.text());
				if (!piece.isEmpty()) {
					runs += flags.isEmpty() ? piece + QLatin1Char{ ' ' } : QLatin1Char{ '[' } + flags + QLatin1Char{ ':' } + piece + QStringLiteral("] ");
				}
			}
			blocks.push_back(kind + QStringLiteral(" | ") + runs.trimmed());
		}
		return blocks;
	}

	/// Qt's HTML import has no task marker for GitHub's checkbox `<input>`;
	/// its own list-item classes carry one.
	auto with_task_markers(QString aHtml) -> QString
	{
		aHtml.replace(QRegularExpression{ QStringLiteral(R"(<li>\s*<input checked="" disabled="" type="checkbox"\s*/?>\s*)") }, QStringLiteral(R"(<li class="checked">)"));
		aHtml.replace(QRegularExpression{ QStringLiteral(R"(<li>\s*<input disabled="" type="checkbox"\s*/?>\s*)") }, QStringLiteral(R"(<li class="unchecked">)"));
		return aHtml;
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication                app{ aArgc, aArgv };
	bpr::qt::markdown_body_view view;
	view.resize(740, 900);
	view.show();
	QApplication::processEvents();
	const auto known = QJsonDocument::fromJson(read_file(QStringLiteral("known-differences.json")).toUtf8()).object().value(QStringLiteral("examples")).toObject();
	expect(!known.isEmpty(), "the known differences are listed");
	std::map<QString, QString> found;
	// BPR_CMARK_SPEC_DUMP names a file for every difference, both shapes and
	// the Markdown, for classifying it in known-differences.json.
	QJsonObject dump;
	auto        total = 0, matching = 0;
	for (const auto& file : { QStringLiteral("spec.txt"), QStringLiteral("extensions.txt"), QStringLiteral("regression.txt") }) {
		const auto examples = examples_of(file);
		expect(!examples.empty(), "the suite has examples: " + file.toStdString());
		for (const auto& example : examples) {
			view.set_body(example.Markdown.toStdString(), example.Name.toStdString());
			QApplication::processEvents();
			auto expected = QTextDocument{ };
			expected.setHtml(with_task_markers(example.Html));
			++total;
			const auto ours   = shape(*view.rendered_view()->document());
			const auto theirs = shape(expected);
			// `<IGNORE>` marks a does-not-crash example, which cmark-gfm does not
			// compare: reading it is the test.
			if (ours == theirs || example.Html.trimmed() == QStringLiteral("<IGNORE>")) {
				++matching;
			} else {
				found.emplace(example.Name, example.Section);
				dump.insert(example.Name, QJsonObject{ { QStringLiteral("section"), example.Section }, { QStringLiteral("markdown"), example.Markdown }, { QStringLiteral("rendered"), view.rendered_available() }, { QStringLiteral("ours"), QJsonArray::fromStringList(ours) }, { QStringLiteral("expected"), QJsonArray::fromStringList(theirs) } });
			}
		}
	}
	if (const auto path = qEnvironmentVariable("BPR_CMARK_SPEC_DUMP"); !path.isEmpty()) {
		auto file = QFile{ path };
		expect(file.open(QIODevice::WriteOnly) && file.write(QJsonDocument{ dump }.toJson()) > 0, "differences written");
	}
	std::cout << "cmark-gfm-spec examples=" << total << " matching=" << matching << " known=" << known.size() << '\n';
	expect(total == 726, "every example of the three suites is read");
	auto unexpected = 0, stale = 0;
	for (const auto& [name, section] : found) {
		if (!known.contains(name)) {
			++unexpected;
			std::cout << "NEW DIFFERENCE " << name.toStdString() << " (" << section.toStdString() << ")\n";
		}
	}
	for (auto it = known.begin(); it != known.end(); ++it) {
		if (!found.contains(it.key())) {
			++stale;
			std::cout << "NOW MATCHES, remove from known-differences.json: " << it.key().toStdString() << '\n';
		}
	}
	expect(unexpected == 0, "no example reads differently from cmark-gfm unless it is listed with its reason");
	expect(stale == 0, "every listed difference still differs");
	std::cout << "markdown-cmark-spec-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
