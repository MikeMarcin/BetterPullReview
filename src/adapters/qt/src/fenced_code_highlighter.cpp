#include "fenced_code_highlighter.hpp"

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>

#include <utility>

namespace bpr::qt {
	namespace {
		/// The block state packs the line's role, its fence's character and
		/// its fence's length, so a closing fence is matched to its opening.
		constexpr int role_bits   = 4;
		constexpr int tilde_bit   = 1 << 3;
		constexpr int length_unit = 1 << role_bits;

		struct fence_line
		{
			QChar Mark;
			int   Length = 0;
			bool  Bare   = false;
		};

		/// \brief The fence `aText` opens with, if any: at most three spaces,
		/// then three or more of one fence character. `Bare` says nothing but
		/// spaces follows it, as a closing fence needs.
		[[nodiscard]] auto fence_of(const QString& aText) -> fence_line
		{
			auto at = qsizetype{ 0 };
			while (at < aText.size() && at < 3 && aText[at] == QLatin1Char{ ' ' }) {
				++at;
			}
			if (at >= aText.size() || (aText[at] != QLatin1Char{ '`' } && aText[at] != QLatin1Char{ '~' })) {
				return { };
			}
			const auto mark  = aText[at];
			const auto start = at;
			while (at < aText.size() && aText[at] == mark) {
				++at;
			}
			const auto length = static_cast<int>(at - start);
			if (length < 3) {
				return { };
			}
			return { .Mark = mark, .Length = length, .Bare = aText.mid(at).trimmed().isEmpty() };
		}

		[[nodiscard]] auto encode(const fenced_code_highlighter::line_role aRole, const QChar aMark, const int aLength) -> int
		{
			return static_cast<int>(aRole) | (aMark == QLatin1Char{ '~' } ? tilde_bit : 0) | (aLength * length_unit);
		}

		[[nodiscard]] auto decode_role(const int aState) -> fenced_code_highlighter::line_role
		{
			if (aState < 0) {
				return fenced_code_highlighter::line_role::prose;
			}
			return static_cast<fenced_code_highlighter::line_role>(aState & (tilde_bit - 1));
		}
	} // namespace

	fenced_code_highlighter::fenced_code_highlighter(QTextDocument* aDocument, const QFont& aCodeFont, const QColor& aQuiet)
		: QSyntaxHighlighter{ aDocument }
		, CodeFont{ aCodeFont }
		, Quiet{ aQuiet }
	{ }

	void fenced_code_highlighter::set_style(const QFont& aFont, const QColor& aQuiet)
	{
		if (CodeFont == aFont && Quiet == aQuiet) {
			return;
		}
		CodeFont = aFont;
		Quiet    = aQuiet;
		rehighlight();
	}

	void fenced_code_highlighter::set_code_colours(code_colours aColours)
	{
		Colours = std::move(aColours);
		rehighlight();
	}

	auto fenced_code_highlighter::role_of(const QTextBlock& aBlock) -> line_role
	{
		return aBlock.isValid() ? decode_role(aBlock.userState()) : line_role::prose;
	}

	void fenced_code_highlighter::highlightBlock(const QString& aText)
	{
		const auto previous = previousBlockState();
		const auto before   = decode_role(previous);
		const auto inside   = before == line_role::opening_fence || before == line_role::code;
		const auto fence    = fence_of(aText);
		auto       role     = line_role::prose;
		if (inside) {
			const auto openMark   = (previous & tilde_bit) != 0 ? QLatin1Char{ '~' } : QLatin1Char{ '`' };
			const auto openLength = previous / length_unit;
			const auto closes     = fence.Length > 0 && fence.Bare && fence.Mark == openMark && fence.Length >= openLength;
			role                  = closes ? line_role::closing_fence : line_role::code;
			setCurrentBlockState(encode(role, openMark, openLength));
		} else if (fence.Length > 0) {
			role = line_role::opening_fence;
			setCurrentBlockState(encode(role, fence.Mark, fence.Length));
		} else {
			setCurrentBlockState(encode(line_role::prose, QChar{ }, 0));
			return;
		}
		auto format = QTextCharFormat{ };
		format.setFont(CodeFont, QTextCharFormat::FontPropertiesAll);
		if (role != line_role::code) {
			// The fences are punctuation: quiet, so the code reads first.
			format.setForeground(Quiet);
		}
		setFormat(0, static_cast<int>(aText.size()), format);
		if (role == line_role::code && Colours) {
			for (const auto& range : Colours(currentBlock())) {
				auto coloured = format;
				coloured.merge(range.format);
				setFormat(range.start, range.length, coloured);
			}
		}
	}
} // namespace bpr::qt
