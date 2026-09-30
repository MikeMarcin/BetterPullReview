#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_COMPOSER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_COMPOSER_HPP

#include "bpr/qt/suggestion_editor.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <string>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	/// \brief The exact source a suggestion starts from, as the host read it
	/// from retained material.
	struct suggestion_composer_source
	{
		/// Where the selection is, for people: file, lines and revision.
		std::string Location;
		/// The bytes around the selection and the selection itself.
		std::string Before;
		std::string Original;
		std::string After;
	};

	/// \brief How the composer's source relates to newer material.
	/// \details The composer never moves: newer material or a lost anchor is
	/// stated, and the suggestion stays on its original exact bytes.
	enum class suggestion_source_status : std::uint8_t {
		current,
		/// A later capture exists; this source is still exactly what it was.
		newer_available,
		/// The file changed where the selection was; the suggestion needs revalidation.
		stale,
		/// The selected bytes occur more than once in newer material.
		ambiguous,
		/// The file is gone from newer material.
		deleted,
	};

	/// \brief Suggest change: a focused composer prefilled from a selection
	/// (BPR-1156).
	/// \details It holds the suggested code, an optional rationale in the
	/// note's own Markdown, whether the code is exact or only illustrative,
	/// and a before/after preview of the selection in its context. It reads
	/// no file and writes nothing: Save hands the exact replacement bytes,
	/// intent and rationale to whoever composed the widget.
	class suggestion_composer final : public QWidget
	{
	public:
		explicit suggestion_composer(QWidget* aParent = nullptr);

		/// \brief Starts a suggestion from a selection; the editor holds its exact bytes.
		void begin(const suggestion_composer_source& aSource);
		/// \brief States how the source relates to newer material, without moving it.
		void set_source_status(suggestion_source_status aStatus);
		void set_code_font(const QFont& aFont);

		[[nodiscard]] auto editor() const noexcept -> suggestion_editor*;
		[[nodiscard]] auto replacement() const -> std::string;
		[[nodiscard]] auto rationale() const -> std::string;
		[[nodiscard]] auto illustrative() const -> bool;
		[[nodiscard]] auto source_status() const noexcept -> suggestion_source_status;
		/// \brief The preview text: the selection's context before and after the edit.
		[[nodiscard]] auto preview_text() const -> QString;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;

		std::function<void()> Saved;
		std::function<void()> Cancelled;

	private:
		void refresh_preview();

		suggestion_composer_source Source;
		suggestion_source_status   Status       = suggestion_source_status::current;
		QLabel*                    Location     = nullptr;
		QLabel*                    StatusLabel  = nullptr;
		suggestion_editor*         Editor       = nullptr;
		QCheckBox*                 Illustrative = nullptr;
		QPlainTextEdit*            Rationale    = nullptr;
		QPlainTextEdit*            Preview      = nullptr;
		QPushButton*               Save         = nullptr;
		QPushButton*               Cancel       = nullptr;
	};
}

#endif
