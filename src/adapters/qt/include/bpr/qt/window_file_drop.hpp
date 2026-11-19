#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WINDOW_FILE_DROP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WINDOW_FILE_DROP_HPP

#include <QObject>
#include <QPointer>
#include <QString>

#include <cstdint>
#include <functional>

class QEvent;
class QMimeData;
class QWidget;

/// \file
/// \brief [[BPR-2282]]. A file dropped onto a window opens its import.
///
/// The window takes a drag that carries exactly one local file, shows a
/// quiet highlight while it hovers and, on the drop, hands the file's path to
/// the composition, which reads it through the same host path its menu
/// import uses and opens the import dialog. The window reads no file: what a
/// drag carries is said by the composition's `admit` (Qt here never parses an
/// address), and what the file is, by its content, is the host's to read.
namespace bpr::qt {
	/// \brief What a drag carries, as far as the window needs to know.
	struct dropped_file_admission
	{
		enum class kind : std::uint8_t {
			/// No local file: the drag is not the window's, and passes on to
			/// whatever widget takes it (text dragged inside an editor).
			none,
			/// Exactly one local file, at `Path`.
			one_file,
			/// Two or more local files: refused, with a word why.
			several_files,
		};
		kind    Kind = kind::none;
		QString Path;
	};

	/// \brief Takes file drops for one top-level window.
	///
	/// \details It filters the application's drag events for every widget of
	/// its window ahead of the widget itself, so a drop anywhere on the
	/// window, an editor included, is the window's. A drag carrying no local
	/// file is left to the widget under it. While a modal dialog is open a
	/// file drag is refused and a drop does nothing. `Dropped` runs on the
	/// next turn of the event loop, after the platform's drag has finished.
	class window_file_drop final : public QObject
	{
	public:
		struct handlers
		{
			/// Says what a drag's data carries; never reads a file.
			std::function<dropped_file_admission(const QMimeData&)> Admit;
			/// The one file dropped.
			std::function<void(const QString&)> Dropped;
			/// A short, non-modal word: why a drag was refused.
			std::function<void(const QString&)> Say;
		};

		/// \brief Lives as long as `aWindow`; makes it accept drops.
		window_file_drop(QWidget& aWindow, handlers aHandlers);

		/// \brief Whether an admitted file is hovering over the window.
		[[nodiscard]] auto hovering() const noexcept -> bool { return Hovering; }
		/// \brief The highlight drawn while a file hovers.
		[[nodiscard]] auto highlight() const noexcept -> QWidget* { return Highlight; }
		/// \brief What the window says for a drag of several files.
		[[nodiscard]] static auto several_files_text() -> QString;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		[[nodiscard]] auto belongs(const QObject* aWatched) const -> bool;
		void               hover(bool aOn);

		QPointer<QWidget> Window;
		QWidget*          Highlight = nullptr;
		handlers          Handlers;
		bool              Hovering = false;
	};
} // namespace bpr::qt
#endif
