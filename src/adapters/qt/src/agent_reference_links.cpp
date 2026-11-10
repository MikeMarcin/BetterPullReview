#include "bpr/qt/agent_reference_links.hpp"

#include <QCursor>
#include <QMenu>
#include <QUrl>
#include <QWidget>

#include <algorithm>

namespace bpr::qt {
	namespace {
		/// A path as a link destination can carry it: what Markdown or the
		/// target's own separators would read is percent-encoded.
		[[nodiscard]] auto encoded(const QString& aPath) -> QString
		{
			return QString::fromUtf8(QUrl::toPercentEncoding(aPath, "/._-+~"));
		}
	} // namespace

	auto agent_reference_link_text(const agent_reference_target& aTarget) -> QString
	{
		auto paths = QStringList{ };
		for (const auto& path : aTarget.Paths) {
			paths << encoded(path);
		}
		const auto side = aTarget.Before ? QStringLiteral("B") : QStringLiteral("L");
		auto       link = paths.join(QLatin1Char{ '|' }) + QLatin1Char{ '#' } + side + QString::number(aTarget.First);
		if (aTarget.Last > aTarget.First) {
			link += QLatin1Char{ '-' } + side + QString::number(aTarget.Last);
		}
		return link;
	}

	auto agent_reference_link_target(const QString& aLink) -> std::optional<agent_reference_target>
	{
		const auto hash = aLink.lastIndexOf(QLatin1Char{ '#' });
		if (hash <= 0 || hash + 2 >= aLink.size() || aLink.contains(QStringLiteral("://")) || (aLink[hash + 1] != QLatin1Char{ 'L' } && aLink[hash + 1] != QLatin1Char{ 'B' })) {
			return std::nullopt;
		}
		const auto before = aLink[hash + 1] == QLatin1Char{ 'B' };
		const auto lines  = aLink.mid(hash + 2).split(QLatin1Char{ '-' });
		auto       ok     = false;
		const auto first  = lines.front().toUInt(&ok);
		if (!ok || first == 0U || lines.size() > 2) {
			return std::nullopt;
		}
		auto last = first;
		if (lines.size() == 2) {
			auto       lastOk = false;
			const auto range  = lines.back().startsWith(QLatin1Char{ 'L' }) || lines.back().startsWith(QLatin1Char{ 'B' }) ? lines.back().mid(1) : lines.back();
			last              = range.toUInt(&lastOk);
			if (!lastOk || last < first) {
				return std::nullopt;
			}
		}
		auto target = agent_reference_target{ .First = first, .Last = last, .Before = before };
		for (const auto& path : aLink.left(hash).split(QLatin1Char{ '|' })) {
			if (path.isEmpty()) {
				return std::nullopt;
			}
			target.Paths << QUrl::fromPercentEncoding(path.toUtf8());
		}
		return target;
	}

	auto agent_markdown_with_reference_links(const QString& aMarkdown, const core::agent_reference_files& aFiles, const QString& aContextFile) -> QString
	{
		auto       bytes = aMarkdown.toUtf8();
		const auto links = core::agent_markdown_reference_links(std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }, aFiles, aContextFile.toStdString());
		if (links.empty()) {
			return aMarkdown;
		}
		// From the end, so each edit leaves the offsets before it as they were.
		for (auto it = links.rbegin(); it != links.rend(); ++it) {
			auto target = agent_reference_target{ .First = it->First, .Last = it->Last, .Before = it->Before };
			for (const auto& path : it->Paths) {
				target.Paths << QString::fromStdString(path);
			}
			const auto begin   = static_cast<qsizetype>(it->Begin);
			const auto length  = static_cast<qsizetype>(it->Length);
			const auto written = bytes.mid(begin, length);
			bytes.replace(begin, length, QByteArray{ "[" } + written + "](" + agent_reference_link_text(target).toUtf8() + ")");
		}
		return QString::fromUtf8(bytes);
	}

	auto follow_agent_reference(QWidget* const aOwner, const QString& aLink, const agent_reference_navigation& aNavigate, const agent_reference_chooser& aChoose) -> bool
	{
		const auto target = agent_reference_link_target(aLink);
		if (!target || !aNavigate) {
			return target.has_value();
		}
		if (target->Paths.size() == 1) {
			aNavigate(target->Paths.front(), target->First, target->Last, target->Before);
			return true;
		}
		// A name several files share: the reviewer chooses which.
		auto chosen = std::optional<QString>{ };
		if (aChoose) {
			chosen = aChoose(target->Paths);
		} else {
			auto menu = QMenu{ aOwner };
			menu.setObjectName(QStringLiteral("agentReferenceChoices"));
			menu.setAccessibleName(QObject::tr("Open which file"));
			for (const auto& path : target->Paths) {
				menu.addAction(path);
			}
			if (const auto* const picked = menu.exec(QCursor::pos()); picked != nullptr) {
				chosen = picked->text();
			}
		}
		if (chosen && target->Paths.contains(*chosen)) {
			aNavigate(*chosen, target->First, target->Last, target->Before);
		}
		return true;
	}
} // namespace bpr::qt
