#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROMPT_MACROS_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROMPT_MACROS_PANEL_HPP

#include "bpr/application/prompt_macro_host.hpp"
#include "bpr/core/prompt_macro.hpp"

#include <QDialog>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QToolButton;
class QVBoxLayout;

/// \file
/// \brief RFC-BPR-0125 milestones 3 and 4 ([[BPR-2146]], [[BPR-2147]]) with
/// RFC-BPR-0128's Quick ask row ([[BPR-2163]]). Settings › Agents & context ›
/// Suggested prompts: the roster, one list a place, and the sheet a row
/// opens. The core validates and decides; this presents and edits values.
namespace bpr::qt {
	/// \brief The sheet: one macro's edit.
	class prompt_macro_sheet final : public QDialog
	{
	public:
		/// \param aSiblings The other macros, for the duplicate-name advice.
		/// \param aShipped The built-in's shipped macro, for Reset.
		prompt_macro_sheet(core::prompt_macro aMacro, std::vector<core::prompt_macro> aSiblings, std::optional<core::prompt_macro> aShipped, std::optional<core::prompt_facts> aPreview,
						   QWidget* aParent = nullptr);

		/// \brief The macro as edited.
		[[nodiscard]] auto macro() const -> core::prompt_macro;
		/// \brief How the sheet was closed.
		enum class outcome : std::uint8_t {
			cancelled,
			saved,
			duplicated,
			deleted,
		};
		[[nodiscard]] auto result_outcome() const noexcept -> outcome { return Outcome; }

		[[nodiscard]] auto name_field() const noexcept -> QLineEdit* { return Name; }
		[[nodiscard]] auto template_field() const noexcept -> QPlainTextEdit* { return Template; }
		[[nodiscard]] auto quick_ask_box() const noexcept -> QCheckBox* { return QuickAsk; }
		[[nodiscard]] auto scope_button(core::prompt_scope aScope) const noexcept -> QToolButton* { return Scopes.at(static_cast<std::size_t>(aScope)); }
		[[nodiscard]] auto save_button() const noexcept -> QPushButton* { return Save; }
		[[nodiscard]] auto problems_text() const -> QString;
		/// \brief Closes the sheet as a button would (Save, Duplicate,
		/// Delete): what a test presses in place of the reviewer.
		void finish(outcome aOutcome);
		[[nodiscard]] auto preview_text() const -> QString;

	private:
		void refresh();
		void refill_subjects();
		void rebuild_conditions();
		void keyPressEvent(QKeyEvent* aEvent) override;
		void reject() override;

		core::prompt_macro                  Original;
		core::prompt_macro                  Edited;
		std::vector<core::prompt_macro>     Siblings;
		std::optional<core::prompt_macro>   Shipped;
		std::optional<core::prompt_facts>   Preview;
		outcome                             Outcome   = outcome::cancelled;
		bool                                Refreshing = false;
		QLineEdit*                          Name       = nullptr;
		QLabel*                             ChipReads  = nullptr;
		QComboBox*                          Place      = nullptr;
		QComboBox*                          Subject    = nullptr;
		QRadioButton*                       AsQuestion = nullptr;
		QRadioButton*                       AsReview   = nullptr;
		QCheckBox*                          QuickAsk   = nullptr;
		std::array<QToolButton*, 3>         Scopes{ };
		QPlainTextEdit*                     Template   = nullptr;
		QLabel*                             Bytes      = nullptr;
		QLabel*                             BuiltInNote = nullptr;
		QLabel*                             PreviewText = nullptr;
		QLabel*                             ShowsNow    = nullptr;
		QVBoxLayout*                        Conditions  = nullptr;
		QPushButton*                        AddCondition = nullptr;
		QLabel*                             Needs       = nullptr;
		QLabel*                             Problems    = nullptr;
		QPushButton*                        Save        = nullptr;
	};

	/// \brief The roster.
	class prompt_macros_panel final : public QWidget
	{
	public:
		explicit prompt_macros_panel(QWidget* aParent = nullptr);

		/// \brief The profile's set, and whether edits can be made.
		void present(const application::prompt_macro_projection& aProjection);
		/// \brief Saves the whole edited list, in order; the host stores the
		/// differences from the shipped set.
		void set_save_dispatch(std::function<void(std::vector<core::prompt_macro>)> aDispatch);
		/// \brief The window's facts, for the sheet's preview; none uses
		/// sample values.
		void set_preview_facts(std::function<std::optional<core::prompt_facts>(core::prompt_place)> aFacts);
		/// \brief Edit Quick Asks…: the roster is brought into view, focused.
		void reveal();

		[[nodiscard]] auto list(core::prompt_place aPlace) const noexcept -> QListWidget* { return Lists.at(static_cast<std::size_t>(aPlace)); }
		[[nodiscard]] auto macros() const noexcept -> const std::vector<core::prompt_macro>& { return Macros; }
		/// \brief A row's sentence: what it sends as and when it shows.
		[[nodiscard]] static auto sentence(const core::prompt_macro& aMacro) -> QString;
		/// \brief A row's state in words: Built-in, Changed, off, quick ask.
		[[nodiscard]] static auto state_words(const core::prompt_set_entry& aEntry) -> QString;
		/// \brief Opens the sheet on `aId`; for tests, `aEdit` changes the
		/// sheet before it is accepted in place of the reviewer.
		void edit(const std::string& aId, const std::function<void(prompt_macro_sheet&)>& aEdit = { });
		void add_prompt(core::prompt_place aPlace, const std::function<void(prompt_macro_sheet&)>& aEdit = { });
		void move(core::prompt_place aPlace, int aRow, int aBy);
		void toggle(const std::string& aId, bool aOn);
		[[nodiscard]] auto status_text() const -> QString;

	private:
		void rebuild();
		void save(std::vector<core::prompt_macro> aMacros);
		void open_sheet(core::prompt_macro aMacro, bool aNew, const std::function<void(prompt_macro_sheet&)>& aEdit);
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

		application::prompt_macro_projection                                   Projection;
		std::vector<core::prompt_macro>                                        Macros;
		std::function<void(std::vector<core::prompt_macro>)>                   SaveDispatch;
		std::function<std::optional<core::prompt_facts>(core::prompt_place)>   PreviewFacts;
		std::array<QListWidget*, core::prompt_place_count>                     Lists{ };
		QLabel*                                                                Status  = nullptr;
		QPushButton*                                                           Add     = nullptr;
		QPushButton*                                                           Restore = nullptr;
		bool                                                                   Filling = false;
	};
} // namespace bpr::qt

#endif
