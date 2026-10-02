#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_COMPOSER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_COMPOSER_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/suggestion_editor.hpp"

#include <QWidget>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace bpr::qt {
	class appearance_profile;

	/// \brief The file a suggestion edits, as the host read it from retained
	/// material, and where the reader's caret was.
	struct suggestion_composer_source
	{
		/// Which file and revision, for people.
		std::string Location;
		/// The file's path, which chooses its syntax, and its complete bytes.
		std::string Path;
		std::string File;
		/// Bytes [`Start`, `End`) of `File` are selected when editing starts;
		/// equal, they are only the caret.
		std::uint64_t Start = 0;
		std::uint64_t End   = 0;
	};

	/// \brief Which text a classification describes: the file as captured,
	/// or the file as edited.
	enum class suggestion_syntax_side : std::uint8_t {
		original,
		proposed,
	};

	/// \brief The composer asks for one text to be classified (BPR-1156).
	/// \details The composer runs no lexer. Whoever composed it classifies
	/// `Document`, chosen by `Path`, and hands the spans back through
	/// `suggestion_composer::adopt_syntax` with the same document.
	struct suggestion_syntax_request
	{
		suggestion_syntax_side Side = suggestion_syntax_side::original;
		std::string            Path;
		std::string            Document;
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

	/// \brief Suggest change: the whole file edited in place (BPR-1156,
	/// owner amendment of 2026-10-01).
	/// \details The edit happens in a `suggestion_editor` that whoever
	/// composed the widget lays out where the file is read: the editor starts
	/// with the whole captured file and the reader's caret or selection. The
	/// composer itself is the side panel: which file, an optional rationale in
	/// the note's own Markdown, whether the code is exact or only
	/// illustrative, and a before/after preview of every change. Each
	/// separate changed run of lines is one exact suggestion
	/// (`core::derive_edit_hunks`). It reads no file and writes nothing: Save
	/// hands the changes, intent and rationale to whoever composed the widget.
	/// The editor and the preview are coloured from classifications it asks
	/// for (`SyntaxWanted`), painted through the review panes' role colours
	/// and span-to-format rule; the preview's removed and added rows wear the
	/// diff panes' own row fills (`code_sample_row_fill`).
	class suggestion_composer final : public QWidget
	{
	public:
		explicit suggestion_composer(suggestion_editor& aEditor, QWidget* aParent = nullptr);

		/// \brief Starts editing a file at the reader's caret.
		/// \returns False, and leaves the editor alone, for a file too large
		/// to hold (`core::maximum_suggestion_editor_bytes`).
		auto begin(const suggestion_composer_source& aSource) -> bool;
		/// \brief States how the source relates to newer material, without moving it.
		void set_source_status(suggestion_source_status aStatus);
		void set_code_font(const QFont& aFont);
		/// \brief Takes the syntax and diff role colours of the applied appearance.
		void adopt_appearance(const appearance_profile& aProfile);
		/// \brief Colours the editor and preview from one classification.
		/// \returns Whether the spans were adopted: only a classification of
		/// the text the composer shows for `aSide` now is.
		auto adopt_syntax(suggestion_syntax_side aSide, std::string_view aDocument, application::shared_token_spans aSpans) -> bool;

		[[nodiscard]] auto editor() const noexcept -> suggestion_editor*;
		/// \brief The separate changes, in the original file's coordinates.
		[[nodiscard]] auto edits() const noexcept -> const std::vector<core::cycle_edit_hunk>&;
		/// \brief The source the suggestion began from.
		[[nodiscard]] auto source() const noexcept -> const suggestion_composer_source&;
		[[nodiscard]] auto rationale() const -> std::string;
		[[nodiscard]] auto illustrative() const -> bool;
		[[nodiscard]] auto source_status() const noexcept -> suggestion_source_status;
		/// \brief The preview text: each change in context, before then after.
		[[nodiscard]] auto preview_text() const -> QString;
		/// \brief The preview pane, whose rows carry the diff fills.
		[[nodiscard]] auto preview() const noexcept -> QPlainTextEdit*;
		/// \brief What the edit amounts to, or why it cannot be saved.
		[[nodiscard]] auto summary_text() const -> QString;
		[[nodiscard]] auto status_text() const -> QString;
		[[nodiscard]] auto save_button() const noexcept -> QPushButton*;

		std::function<void()> Saved;
		std::function<void()> Cancelled;
		/// Asks for a classification: both texts when a suggestion begins,
		/// the edited text after every edit.
		std::function<void(const suggestion_syntax_request&)> SyntaxWanted;

	private:
		void               text_changed();
		void               refresh_preview();
		void               request_syntax(suggestion_syntax_side aSide);
		void               apply_syntax();
		[[nodiscard]] auto formats_of(suggestion_syntax_side aSide, std::uint64_t aBegin, const std::string& aContent) const -> QList<QTextLayout::FormatRange>;

		suggestion_composer_source Source;
		/// The file as captured and as edited.
		std::array<std::string, 2>         Texts;
		std::array<classified_text, 2>     Classified;
		std::vector<core::cycle_edit_hunk> Hunks;
		std::vector<core::cycle_edit_row>  PreviewRows;
		code_role_colors                   SyntaxColors{ };
		suggestion_source_status           Status       = suggestion_source_status::current;
		suggestion_editor*                 Editor       = nullptr;
		QLabel*                            Location     = nullptr;
		QLabel*                            StatusLabel  = nullptr;
		QLabel*                            Summary      = nullptr;
		QCheckBox*                         Illustrative = nullptr;
		QPlainTextEdit*                    Rationale    = nullptr;
		QPlainTextEdit*                    Preview      = nullptr;
		QPushButton*                       Save         = nullptr;
		QPushButton*                       Cancel       = nullptr;
	};
}

#endif
