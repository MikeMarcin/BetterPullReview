#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REFERENCE_LINKS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_REFERENCE_LINKS_HPP

#include "bpr/core/agent_reference.hpp"
#include "bpr/core/tour_references.hpp"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class QWidget;

namespace bpr::qt {
	/// \brief [[BPR-1998]], RFC-BPR-0101. The files a reference in an agent
	/// reply may name, read when a reply is shown.
	using agent_reference_files_source = std::function<core::agent_reference_files()>;
	/// \brief Opens a file of the pull request in Review at one-based lines
	/// `aFirst` to `aLast` of its head (or, `aBefore`, of the change's base),
	/// revealing the range.
	using agent_reference_navigation = std::function<void(const QString& aPath, std::uint32_t aFirst, std::uint32_t aLast, bool aBefore)>;

	/// \brief Where one link goes: one file, or several to choose from.
	struct agent_reference_target
	{
		QStringList   Paths;
		std::uint32_t First  = 0U;
		std::uint32_t Last   = 0U;
		bool          Before = false;

		auto operator==(const agent_reference_target&) const -> bool = default;
	};

	/// \brief The link a reference opens by: the path (several joined by
	/// `|`), `#L` for the head or `#B` for the base, the first line, and
	/// `-L` (or `-B`) and the last line for a range.
	[[nodiscard]] auto agent_reference_link_text(const agent_reference_target& aTarget) -> QString;
	[[nodiscard]] auto agent_reference_link_target(const QString& aLink) -> std::optional<agent_reference_target>;

	/// \brief `aMarkdown` with every reference that will open made a link,
	/// as the core's recogniser decides. Copy keeps the text as written.
	[[nodiscard]] auto agent_markdown_with_reference_links(const QString& aMarkdown, const core::agent_reference_files& aFiles, const QString& aContextFile = { }) -> QString;

	/// \brief [[BPR-1996]], RFC-BPR-0100. The commit-pinned permalink of
	/// one-based lines of a pull request's file, at the tour's pinned head (or,
	/// `aBefore`, its base), made by the host; none when it cannot be made.
	using tour_permalink_source = std::function<std::optional<std::string>(const std::string& aPath, std::uint32_t aFirst, std::uint32_t aLast, bool aBefore)>;

	/// \brief [[BPR-1996]], RFC-BPR-0100 AC06. A tour text as the application
	/// shows it: each reference that resolves to one file is a link that opens
	/// its lines in Review; one alone on its line is, with `aPermalink`, the
	/// permalink of at most `core::tour_reference_snippet_lines` of them (a
	/// reader shows it as a snippet) followed by Show in Review. A reference
	/// that does not resolve is left as written.
	[[nodiscard]] auto tour_markdown_with_references(const std::string& aMarkdown, const core::agent_reference_files& aFiles, const tour_permalink_source& aPermalink) -> std::string;

	/// \brief Chooses one of several files a shared name matches; none when
	/// the reviewer chose none.
	using agent_reference_chooser = std::function<std::optional<QString>(const QStringList& aPaths)>;

	/// \brief Follows one link: opens its one file, or offers its files to
	/// choose from (`aChoose`, or a menu at the pointer). Answers whether the
	/// link was a reference.
	auto follow_agent_reference(QWidget* aOwner, const QString& aLink, const agent_reference_navigation& aNavigate, const agent_reference_chooser& aChoose = { }) -> bool;
} // namespace bpr::qt

#endif
