#include "markdown_github_html.hpp"

#include "bpr/core/markdown_navigation.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace bpr::qt::detail {
	namespace {
		struct markdown_emoji_entry
		{
			std::string_view Name;
			std::string_view Emoji;
		};

#include "markdown_emoji_table.inc"

		constexpr auto tag_limit       = qsizetype{ 8192 };
		constexpr auto attribute_limit = std::size_t{ 64 };

		auto ascii_letter(const QChar aChar) -> bool
		{
			const auto code = aChar.unicode();
			return (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z');
		}

		auto ascii_digit(const QChar aChar) -> bool
		{
			return aChar.unicode() >= '0' && aChar.unicode() <= '9';
		}

		auto html_space(const QChar aChar) -> bool
		{
			return aChar == QLatin1Char{ ' ' } || aChar == QLatin1Char{ '\t' } || aChar == QLatin1Char{ '\n' } || aChar == QLatin1Char{ '\r' } || aChar == QLatin1Char{ '\f' };
		}

		auto escaped(QStringView aText) -> QString
		{
			QString result;
			result.reserve(aText.size());
			for (const auto ch : aText) {
				if (ch == QLatin1Char{ '<' }) {
					result += QStringLiteral("&lt;");
				} else if (ch == QLatin1Char{ '>' }) {
					result += QStringLiteral("&gt;");
				} else if (ch == QLatin1Char{ '&' }) {
					result += QStringLiteral("&amp;");
				} else if (ch == QLatin1Char{ '"' }) {
					result += QStringLiteral("&quot;");
				} else {
					result += ch;
				}
			}
			return result;
		}

		/// The length of a character reference at `aText`, or zero.
		auto entity_length(QStringView aText) -> qsizetype
		{
			auto at = qsizetype{ 1 };
			if (at < aText.size() && aText[at] == QLatin1Char{ '#' }) {
				++at;
				const auto hex = at < aText.size() && (aText[at] == QLatin1Char{ 'x' } || aText[at] == QLatin1Char{ 'X' });
				at += hex ? 1 : 0;
				const auto digits = at;
				while (at < aText.size() && at - digits < 8 && (ascii_digit(aText[at]) || (hex && QStringLiteral("abcdefABCDEF").contains(aText[at])))) {
					++at;
				}
				return at > digits && at < aText.size() && aText[at] == QLatin1Char{ ';' } ? at + 1 : 0;
			}
			const auto name = at;
			while (at < aText.size() && at - name < 32 && (ascii_letter(aText[at]) || ascii_digit(aText[at]))) {
				++at;
			}
			return at > name && at < aText.size() && aText[at] == QLatin1Char{ ';' } ? at + 1 : 0;
		}

		auto span(const html_tag& aTag, const QString& aName) -> QString
		{
			const auto value = aTag.attribute(aName).value_or(QString{ }).trimmed();
			if (value.isEmpty() || value.size() > 3 || !std::ranges::all_of(value, ascii_digit) || value.toInt() < 1 || value.toInt() > 64) {
				return { };
			}
			return QStringLiteral(" %1=\"%2\"").arg(aName, value);
		}

		auto aligned_tag(const QString& aName) -> bool
		{
			static const auto names = std::array{ QStringLiteral("p"), QStringLiteral("div"), QStringLiteral("h1"), QStringLiteral("h2"), QStringLiteral("h3"), QStringLiteral("h4"), QStringLiteral("h5"), QStringLiteral("h6"),
												  QStringLiteral("td"), QStringLiteral("th"), QStringLiteral("tr") };
			return std::ranges::find(names, aName) != names.end();
		}

		/// The structure tag as the reader writes it, with only the
		/// attributes GitHub keeps and the reader shows.
		auto canonical_block_tag(const html_tag& aTag) -> QString
		{
			if (aTag.Name == QStringLiteral("hr")) {
				return QStringLiteral("<hr />");
			}
			auto result = QLatin1Char{ '<' } + aTag.Name;
			if (aligned_tag(aTag.Name)) {
				if (const auto align = html_alignment(aTag); !align.isEmpty()) {
					result += QStringLiteral(" align=\"%1\"").arg(align);
				}
			}
			if (aTag.Name == QStringLiteral("td") || aTag.Name == QStringLiteral("th")) {
				result += span(aTag, QStringLiteral("colspan")) + span(aTag, QStringLiteral("rowspan"));
			}
			return result + QLatin1Char{ '>' };
		}

		auto mark(const QChar aMark, const int aKind, const int aField = -1) -> QString
		{
			auto result = aMark + QString::number(aKind);
			if (aField >= 0) {
				result += html_mark_field + QString::number(aField);
			}
			return result + html_mark_close;
		}
	} // namespace

	auto html_alignment(const html_tag& aTag) -> QString
	{
		const auto value = aTag.attribute(QStringLiteral("align")).value_or(QString{ }).trimmed().toLower();
		return value == QStringLiteral("left") || value == QStringLiteral("center") || value == QStringLiteral("right") || value == QStringLiteral("justify") ? value : QString{ };
	}

	auto html_tag::attribute(const QString& aName) const -> std::optional<QString>
	{
		const auto found = std::ranges::find(Attributes, aName, &std::pair<QString, QString>::first);
		return found == Attributes.end() ? std::nullopt : std::optional{ found->second };
	}

	auto html_tag_at(const QStringView aText) -> std::optional<html_tag>
	{
		const auto limit = std::min(aText.size(), tag_limit);
		auto       at    = qsizetype{ 1 };
		auto       tag   = html_tag{ };
		if (limit < 3 || aText[0] != QLatin1Char{ '<' }) {
			return std::nullopt;
		}
		if (aText[at] == QLatin1Char{ '/' }) {
			tag.Closing = true;
			++at;
		}
		const auto start = at;
		if (at >= limit || !ascii_letter(aText[at])) {
			return std::nullopt;
		}
		while (at < limit && (ascii_letter(aText[at]) || ascii_digit(aText[at]) || aText[at] == QLatin1Char{ '-' })) {
			++at;
		}
		tag.Name        = aText.mid(start, at - start).toString().toLower();
		const auto skip = [&] {
			const auto from = at;
			while (at < limit && html_space(aText[at])) {
				++at;
			}
			return at - from;
		};
		if (tag.Closing) {
			skip();
			if (at < limit && aText[at] == QLatin1Char{ '>' }) {
				tag.Length = at + 1;
				return tag;
			}
			return std::nullopt;
		}
		for (;;) {
			const auto space = skip();
			if (at >= limit) {
				return std::nullopt;
			}
			if (aText[at] == QLatin1Char{ '>' }) {
				tag.Length = at + 1;
				return tag;
			}
			if (aText[at] == QLatin1Char{ '/' } && at + 1 < limit && aText[at + 1] == QLatin1Char{ '>' }) {
				tag.SelfClosing = true;
				tag.Length      = at + 2;
				return tag;
			}
			if (space == 0 || !(ascii_letter(aText[at]) || aText[at] == QLatin1Char{ '_' } || aText[at] == QLatin1Char{ ':' })) {
				return std::nullopt;
			}
			const auto nameStart = at;
			while (at < limit && (ascii_letter(aText[at]) || ascii_digit(aText[at]) || QStringLiteral("_.:-").contains(aText[at]))) {
				++at;
			}
			auto       name   = aText.mid(nameStart, at - nameStart).toString().toLower();
			auto       value  = QString{ };
			const auto before = at;
			skip();
			if (at < limit && aText[at] == QLatin1Char{ '=' }) {
				++at;
				skip();
				if (at >= limit) {
					return std::nullopt;
				}
				if (aText[at] == QLatin1Char{ '"' } || aText[at] == QLatin1Char{ '\'' }) {
					const auto quote = aText[at];
					const auto close = aText.mid(0, limit).indexOf(quote, at + 1);
					if (close < 0) {
						return std::nullopt;
					}
					value = decode_html_entities(aText.mid(at + 1, close - at - 1));
					at    = close + 1;
				} else {
					const auto valueStart = at;
					while (at < limit && !html_space(aText[at]) && !QStringLiteral("\"'=<>`").contains(aText[at])) {
						++at;
					}
					if (at == valueStart) {
						return std::nullopt;
					}
					value = decode_html_entities(aText.mid(valueStart, at - valueStart));
				}
			} else {
				at = before;
			}
			if (tag.Attributes.size() < attribute_limit) {
				tag.Attributes.emplace_back(std::move(name), std::move(value));
			}
		}
	}

	auto html_kind_of(const QString& aName) -> html_kind
	{
		struct entry
		{
			const char* Name;
			html_kind   Kind;
		};
		static constexpr auto table = std::array{
			// GFM's tag filter shows these as text; RFC-BPR-0084 reads the images.
			entry{ "script", html_kind::literal },
			entry{ "style", html_kind::literal },
			entry{ "iframe", html_kind::literal },
			entry{ "textarea", html_kind::literal },
			entry{ "title", html_kind::literal },
			entry{ "xmp", html_kind::literal },
			entry{ "noembed", html_kind::literal },
			entry{ "noframes", html_kind::literal },
			entry{ "plaintext", html_kind::literal },
			entry{ "img", html_kind::literal },
			entry{ "picture", html_kind::literal },
			entry{ "source", html_kind::literal },
			entry{ "b", html_kind::bold },
			entry{ "strong", html_kind::bold },
			entry{ "i", html_kind::italic },
			entry{ "em", html_kind::italic },
			entry{ "s", html_kind::strike },
			entry{ "strike", html_kind::strike },
			entry{ "del", html_kind::strike },
			entry{ "ins", html_kind::underline },
			entry{ "sub", html_kind::subscript },
			entry{ "sup", html_kind::superscript },
			entry{ "kbd", html_kind::keyboard },
			entry{ "code", html_kind::code },
			entry{ "tt", html_kind::code },
			entry{ "samp", html_kind::code },
			entry{ "a", html_kind::link },
			entry{ "br", html_kind::line_break },
			entry{ "mark", html_kind::highlight },
			entry{ "p", html_kind::block },
			entry{ "div", html_kind::block },
			entry{ "h1", html_kind::block },
			entry{ "h2", html_kind::block },
			entry{ "h3", html_kind::block },
			entry{ "h4", html_kind::block },
			entry{ "h5", html_kind::block },
			entry{ "h6", html_kind::block },
			entry{ "hr", html_kind::block },
			entry{ "blockquote", html_kind::block },
			entry{ "ul", html_kind::block },
			entry{ "ol", html_kind::block },
			entry{ "li", html_kind::block },
			entry{ "table", html_kind::block },
			entry{ "thead", html_kind::block },
			entry{ "tbody", html_kind::block },
			entry{ "tfoot", html_kind::block },
			entry{ "tr", html_kind::block },
			entry{ "th", html_kind::block },
			entry{ "td", html_kind::block },
			entry{ "caption", html_kind::block },
			entry{ "pre", html_kind::block },
			entry{ "dl", html_kind::block },
			entry{ "dt", html_kind::block },
			entry{ "dd", html_kind::block },
		};
		const auto found = std::ranges::find_if(table, [&aName](const entry& aEntry) { return aName == QLatin1StringView{ aEntry.Name }; });
		return found == table.end() ? html_kind::drop : found->Kind;
	}

	auto decode_html_entities(const QStringView aValue) -> QString
	{
		QString result;
		for (qsizetype at = 0; at < aValue.size();) {
			const auto length = aValue[at] == QLatin1Char{ '&' } ? entity_length(aValue.mid(at)) : 0;
			if (length == 0) {
				result += aValue[at++];
				continue;
			}
			const auto body = aValue.mid(at + 1, length - 2);
			auto       code = char32_t{ 0 };
			if (body.startsWith(QLatin1Char{ '#' })) {
				const auto hex = body.size() > 1 && (body[1] == QLatin1Char{ 'x' } || body[1] == QLatin1Char{ 'X' });
				code           = static_cast<char32_t>(body.mid(hex ? 2 : 1).toUInt(nullptr, hex ? 16 : 10));
			} else if (body == QStringLiteral("amp")) {
				code = '&';
			} else if (body == QStringLiteral("lt")) {
				code = '<';
			} else if (body == QStringLiteral("gt")) {
				code = '>';
			} else if (body == QStringLiteral("quot")) {
				code = '"';
			} else if (body == QStringLiteral("apos")) {
				code = '\'';
			} else if (body == QStringLiteral("nbsp")) {
				code = 0xA0;
			}
			// Zero, a surrogate or past Unicode is not a character: GitHub's
			// browser reads it as U+FFFD, and so does this.
			if (code == 0 && !body.startsWith(QLatin1Char{ '#' })) {
				result += aValue.mid(at, length);
			} else {
				const auto valid = code != 0 && code <= 0x10FFFF && (code < 0xD800 || code > 0xDFFF);
				result += QString::fromUcs4(valid ? &code : U"�", 1);
			}
			at += length;
		}
		return result;
	}

	auto kept_html_link(const html_tag& aTag) -> std::optional<QString>
	{
		auto href = aTag.attribute(QStringLiteral("href"));
		if (!href) {
			return std::nullopt;
		}
		const auto bytes = href->toUtf8();
		if (!core::markdown_html_link_kept(std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) })) {
			return std::nullopt;
		}
		return href;
	}

	auto html_link_holds_image(QStringView aAfter) -> bool
	{
		while (!aAfter.isEmpty() && html_space(aAfter.front())) {
			aAfter = aAfter.mid(1);
		}
		const auto inner = aAfter.startsWith(QLatin1Char{ '<' }) ? html_tag_at(aAfter) : std::nullopt;
		return inner && !inner->Closing && (inner->Name == QStringLiteral("img") || inner->Name == QStringLiteral("picture"));
	}

	auto inline_html_mark(const html_tag& aTag, const html_kind aKind, std::vector<QString>& aHrefs, std::vector<bool>& aLinks) -> QString
	{
		if (aKind == html_kind::line_break) {
			return QString{ html_break_mark };
		}
		if (aKind == html_kind::link) {
			if (aTag.Closing) {
				const auto kept = !aLinks.empty() && aLinks.back();
				if (!aLinks.empty()) {
					aLinks.pop_back();
				}
				return kept ? mark(html_close_mark, static_cast<int>(html_kind::link)) : QString{ };
			}
			const auto href = kept_html_link(aTag);
			aLinks.push_back(href.has_value() && !aTag.SelfClosing);
			if (!aLinks.back()) {
				if (aTag.SelfClosing) {
					aLinks.pop_back();
				}
				return { };
			}
			aHrefs.push_back(*href);
			return mark(html_open_mark, static_cast<int>(html_kind::link), static_cast<int>(aHrefs.size()) - 1);
		}
		const auto formatting = aKind == html_kind::bold || aKind == html_kind::italic || aKind == html_kind::strike || aKind == html_kind::underline || aKind == html_kind::subscript
			|| aKind == html_kind::superscript || aKind == html_kind::keyboard || aKind == html_kind::code || aKind == html_kind::highlight;
		if (!formatting || aTag.SelfClosing) {
			return { };
		}
		// A key cap holds a thin space at each end, as GitHub's padding does.
		if (aKind == html_kind::keyboard) {
			return aTag.Closing ? QChar{ 0x2009 } + mark(html_close_mark, static_cast<int>(aKind)) : mark(html_open_mark, static_cast<int>(aKind)) + QChar{ 0x2009 };
		}
		return mark(aTag.Closing ? html_close_mark : html_open_mark, static_cast<int>(aKind));
	}

	auto sanitize_html_block(const QStringView aBlock, std::vector<QString>& aHrefs) -> QString
	{
		QString              result;
		std::vector<QString> open;
		std::vector<bool>    links;
		auto                 literalLinks = 0;
		for (qsizetype at = 0; at < aBlock.size();) {
			const auto ch = aBlock[at];
			if (ch == QLatin1Char{ '&' }) {
				const auto length = entity_length(aBlock.mid(at));
				result += length > 0 ? aBlock.mid(at, length).toString() : QStringLiteral("&amp;");
				at += std::max<qsizetype>(length, 1);
				continue;
			}
			if (ch != QLatin1Char{ '<' }) {
				result += ch == QLatin1Char{ '>' } ? QStringLiteral("&gt;") : QString{ ch };
				++at;
				continue;
			}
			const auto tag = html_tag_at(aBlock.mid(at));
			if (!tag) {
				result += QStringLiteral("&lt;");
				++at;
				continue;
			}
			const auto raw  = aBlock.mid(at, tag->Length);
			const auto kind = html_kind_of(tag->Name);
			at += tag->Length;
			if (kind == html_kind::literal) {
				result += escaped(raw);
			} else if (kind == html_kind::link && !tag->Closing && html_link_holds_image(aBlock.mid(at))) {
				// RFC-BPR-0084 links the image it finds after this tag.
				++literalLinks;
				result += escaped(raw);
			} else if (kind == html_kind::link && tag->Closing && literalLinks > 0) {
				--literalLinks;
				result += escaped(raw);
			} else if (kind == html_kind::line_break) {
				result += QStringLiteral("<br />");
			} else if (kind == html_kind::block) {
				if (!tag->Closing) {
					result += canonical_block_tag(*tag);
					if (tag->Name != QStringLiteral("hr") && !tag->SelfClosing) {
						open.push_back(tag->Name);
					}
				} else if (const auto found = std::ranges::find(open.rbegin(), open.rend(), tag->Name); found != open.rend()) {
					for (auto count = std::distance(open.rbegin(), found) + 1; count > 0; --count) {
						result += QStringLiteral("</") + open.back() + QLatin1Char{ '>' };
						open.pop_back();
					}
				}
			} else if (kind != html_kind::drop) {
				result += inline_html_mark(*tag, kind, aHrefs, links);
			}
		}
		while (!open.empty()) {
			result += QStringLiteral("</") + open.back() + QLatin1Char{ '>' };
			open.pop_back();
		}
		return result;
	}

	auto html_shows_anything(const QString& aHtml) -> bool
	{
		if (aHtml.contains(QStringLiteral("<hr"))) {
			return true;
		}
		auto inTag = false;
		for (const auto ch : aHtml) {
			if (ch == QLatin1Char{ '<' }) {
				inTag = true;
			} else if (ch == QLatin1Char{ '>' }) {
				inTag = false;
			} else if (!inTag && !ch.isSpace() && (ch.unicode() < 0xFDE0 || ch.unicode() > 0xFDEF)) {
				return true;
			}
		}
		return false;
	}

	auto markdown_emoji(const QStringView aName) -> std::optional<QString>
	{
		const auto name  = aName.toUtf8();
		const auto key   = std::string_view{ name.constData(), static_cast<std::size_t>(name.size()) };
		const auto found = std::ranges::lower_bound(markdown_emoji_table, key, { }, &markdown_emoji_entry::Name);
		if (found == markdown_emoji_table.end() || found->Name != key) {
			return std::nullopt;
		}
		return QString::fromUtf8(found->Emoji.data(), static_cast<qsizetype>(found->Emoji.size()));
	}
} // namespace bpr::qt::detail
