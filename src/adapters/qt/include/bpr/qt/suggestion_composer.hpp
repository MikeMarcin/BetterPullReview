#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_COMPOSER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SUGGESTION_COMPOSER_HPP

#include "bpr/application/appearance_runtime_ports.hpp"
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
		/// The file's path and its complete bytes, with the selection at
		/// `Start`, so the proposal is classified inside its file. With no
		/// file, or one these bytes do not match, the context alone is used.
		std::string   Path;
		std::string   File;
		std::uint64_t Start = 0;
	};

	/// \brief Which text a classification describes: the source as selected,
	/// or the source with the proposal in place of the selection.
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

	/// \brief Suggest change: a focused composer prefilled from a selection
	/// (BPR-1156).
	/// \details It holds the suggested code, an optional rationale in the
	/// note's own Markdown, whether the code is exact or only illustrative,
	/// and a before/after preview of the selection in its context. It reads
	/// no file and writes nothing: Save hands the exact replacement bytes,
	/// intent and rationale to whoever composed the widget. The editor and the
	/// preview are coloured from classifications it asks for
	/// (`SyntaxWanted`), painted through the review panes' own role colours
	/// and span-to-format rule.
	class suggestion_composer final : public QWidget
	{
	public:
		explicit suggestion_composer(QWidget* aParent = nullptr);

		/// \brief Starts a suggestion from a selection; the editor holds its exact bytes.
		void begin(const suggestion_composer_source& aSource);
		/// \brief States how the source relates to newer material, without moving it.
		void set_source_status(suggestion_source_status aStatus);
		void set_code_font(const QFont& aFont);
		/// \brief Takes the syntax role colours of the applied appearance.
		void adopt_appearance(const appearance_profile& aProfile);
		/// \brief Colours the editor and preview from one classification.
		/// \returns Whether the spans were adopted: only a classification of
		/// the text the composer shows for `aSide` now is.
		auto adopt_syntax(suggestion_syntax_side aSide, std::string_view aDocument, application::shared_token_spans aSpans) -> bool;

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
		/// Asks for a classification: both texts when a suggestion begins,
		/// the proposed text after every edit.
		std::function<void(const suggestion_syntax_request&)> SyntaxWanted;

	private:
		/// One preview row: the text it came from and where it begins there.
		struct preview_row
		{
			suggestion_syntax_side Side  = suggestion_syntax_side::original;
			std::uint64_t          Begin = 0;
			std::string            Content;
		};

		void               text_changed();
		void               refresh_preview();
		void               request_syntax(suggestion_syntax_side aSide);
		void               apply_syntax();
		[[nodiscard]] auto formats_of(suggestion_syntax_side aSide, std::uint64_t aBegin, const std::string& aContent) const -> QList<QTextLayout::FormatRange>;

		suggestion_composer_source Source;
		/// Where the context begins in both texts, and where the selection does.
		std::uint64_t                  Base      = 0;
		std::uint64_t                  Selection = 0;
		std::array<std::string, 2>     Texts;
		std::array<classified_text, 2> Classified;
		std::vector<preview_row>       PreviewRows;
		code_role_colors               SyntaxColors{ };
		suggestion_source_status       Status       = suggestion_source_status::current;
		QLabel*                        Location     = nullptr;
		QLabel*                        StatusLabel  = nullptr;
		suggestion_editor*             Editor       = nullptr;
		QCheckBox*                     Illustrative = nullptr;
		QPlainTextEdit*                Rationale    = nullptr;
		QPlainTextEdit*                Preview      = nullptr;
		QPushButton*                   Save         = nullptr;
		QPushButton*                   Cancel       = nullptr;
	};
}

#endif
