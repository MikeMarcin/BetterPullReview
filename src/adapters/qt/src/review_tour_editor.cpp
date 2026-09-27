#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
namespace bpr::qt {
	namespace {
		auto paragraph(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse);
			return label;
		}
		void update_text(QPlainTextEdit* aEditor, const std::string& aText)
		{
			// QTextDocument uses paragraph separators internally. A save acknowledgement
			// must not replace an unchanged document just because its raw source uses CRLF.
			const auto text = QString::fromStdString(aText).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
			if (aEditor->toPlainText() != text) {
				const auto block = QSignalBlocker{ aEditor };
				aEditor->setPlainText(text);
			}
		}
	}
	review_tour_editor::review_tour_editor(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewTourEditor"));
		setAccessibleName(tr("Prepare review tour"));
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(8, 8, 8, 8);
		layout->addWidget(paragraph(tr("Prepare a private review tour. Explain the problem, then the solution. You can revisit any section; nothing here publishes."), this));
		Roadmap = new QTabWidget{ this };
		Roadmap->setObjectName(QStringLiteral("tourRoadmap"));
		Roadmap->setAccessibleName(tr("Review tour sections"));
		const auto field = [&](const QString& aTitle, const QString& aPrompt, const QString& aHelp, const QString& aName, core::tour_command_kind aKind, tour_editor_action aMapping) {
			auto* page   = new QWidget{ Roadmap };
			auto* column = new QVBoxLayout{ page };
			column->addWidget(paragraph(aPrompt, page));
			column->addWidget(paragraph(aHelp, page));
			auto* editor = new QPlainTextEdit{ page };
			editor->setObjectName(aName);
			editor->setAccessibleName(aTitle + tr(" Markdown source"));
			editor->setTabChangesFocus(false);
			editor->setMinimumHeight(130);
			column->addWidget(editor, 1);
			auto* map = new QPushButton{ tr("Use selected description text"), page };
			map->setToolTip(tr("Select text in the pull request description's Source view, then use it in this field."));
			column->addWidget(map);
			connect(map, &QPushButton::clicked, this, [this, aMapping] {if(Action){Action(aMapping);} });
			connect(editor, &QPlainTextEdit::textChanged, this, [this, editor, aKind] { edit(aKind, editor->toPlainText()); });
			Roadmap->addTab(page, aTitle);
			return editor;
		};
		Problem  = field(tr("Problem"), tr("What problem does this change solve?"), tr("Describe what was wrong, missing, or unnecessarily difficult before this change."), QStringLiteral("tourProblem"), core::tour_command_kind::problem, tour_editor_action::map_problem);
		Solution = field(tr("Solution"), tr("How does this change solve the problem?"), tr("Explain the central approach and any important limits. Save detailed implementation decisions for the tour."), QStringLiteral("tourSolution"), core::tour_command_kind::solution, tour_editor_action::map_solution);
		Preview  = new markdown_body_view{ Roadmap };
		Preview->setObjectName(QStringLiteral("tourPreview"));
		auto* previewPage   = new QWidget{ Roadmap };
		auto* previewLayout = new QVBoxLayout{ previewPage };
		previewLayout->addWidget(paragraph(tr("Local preview. No remote operations are scheduled. HTML and disclosures may differ from GitHub; original Markdown remains the source."), previewPage));
		previewLayout->addWidget(Preview, 1);
		Roadmap->addTab(previewPage, tr("Preview"));
		connect(Roadmap, &QTabWidget::currentChanged, this, [this] { refresh_preview(); });
		layout->addWidget(Roadmap, 1);
		Status = paragraph(tr("Open a pull request with an accepted account to prepare a private tour."), this);
		Status->setObjectName(QStringLiteral("tourSaveStatus"));
		Status->setAccessibleName(tr("Tour save status"));
		layout->addWidget(Status);
		Conflict = new QPlainTextEdit{ this };
		Conflict->setObjectName(QStringLiteral("tourConflict"));
		Conflict->setAccessibleName(tr("Saved copy from another window"));
		Conflict->setReadOnly(true);
		Conflict->hide();
		layout->addWidget(Conflict);
		Retry = new QPushButton{ tr("Retry local save"), this };
		connect(Retry, &QPushButton::clicked, this, [this] {if(Action){Action(tour_editor_action::retry_save);} });
		layout->addWidget(Retry);
		UseSaved = new QPushButton{ tr("Discard my edits and use saved copy"), this };
		connect(UseSaved, &QPushButton::clicked, this, [this] {if(Action){Action(tour_editor_action::use_saved_copy);} });
		layout->addWidget(UseSaved);
		present({ });
	}
	void review_tour_editor::set_dispatch(std::function<void(core::tour_command)> aDispatch, std::function<void(tour_editor_action)> aAction)
	{
		Dispatch = std::move(aDispatch);
		Action   = std::move(aAction);
	}
	void review_tour_editor::edit(core::tour_command_kind aKind, const QString& aText)
	{
		if (Applying || !Shown.Draft || !Dispatch) {
			return;
		}
		Dispatch({ .Draft = Shown.Draft->Id, .Owner = Shown.Draft->Owner, .ExpectedRevision = Shown.Draft->Revision, .Kind = aKind, .Text = aText.toUtf8().toStdString() });
	}
	void review_tour_editor::present(const application::tour_presentation& aProjection)
	{
		Applying            = true;
		const bool same     = Shown.Draft && aProjection.Draft && Shown.Draft->Owner == aProjection.Draft->Owner && Shown.Draft->Id == aProjection.Draft->Id;
		Shown               = aProjection;
		const bool editable = Shown.Draft && !Shown.Loading;
		Problem->setReadOnly(!editable);
		Solution->setReadOnly(!editable);
		if (Shown.Draft && (Shown.Refusal == core::tour_refusal::none || !same)) {
			update_text(Problem, Shown.Draft->Problem);
			update_text(Solution, Shown.Draft->Solution);
		} else if (!Shown.Draft) {
			update_text(Problem, { });
			update_text(Solution, { });
		}
		auto status = tr("Open a pull request with an accepted account to prepare a private tour.");
		if (Shown.Loading) {
			status = tr("Loading private tour…");
		} else if (Shown.Refusal != core::tour_refusal::none) {
			status = Shown.Refusal == core::tour_refusal::bounds ? tr("This edit exceeds the local tour limit. Your editor text is retained but has not been saved.") : tr("This edit was refused because its draft or source changed. Your text is retained.");
		} else if (Shown.Conflict) {
			status = tr("Another window saved this draft. Your edits are retained above; the saved copy is below. Choose which copy to keep.");
		} else if (Shown.Failure) {
			status = tr("Local save unavailable. Your edits remain in this window and are not yet durable. Retry the save.");
		} else if (Shown.Draft) {
			status = Shown.Saving ? tr("Saving locally…") : Shown.SavedRevision == Shown.Draft->Revision ? tr("Saved locally. Nothing published.")
																										 : tr("Private draft. Nothing published.");
		}
		Status->setText(status);
		Retry->setVisible(Shown.Failure.has_value() && !Shown.Conflict);
		UseSaved->setVisible(static_cast<bool>(Shown.Conflict));
		Conflict->setVisible(static_cast<bool>(Shown.Conflict));
		if (Shown.Conflict) {
			update_text(Conflict, "## Problem\n" + Shown.Conflict->Problem + "\n\n## Solution\n" + Shown.Conflict->Solution);
		}
		refresh_preview();
		Applying = false;
	}
	void review_tour_editor::refresh_preview()
	{
		if (Roadmap->currentIndex() != Roadmap->count() - 1 || !Shown.Draft) {
			return;
		}
		auto body = "## Problem\n\n" + Shown.Draft->Problem + "\n\n## Solution\n\n" + Shown.Draft->Solution;
		Preview->set_body(std::move(body), std::to_string(Shown.Draft->Revision));
	}
	void review_tour_editor::show_problem()
	{
		Roadmap->setCurrentIndex(0);
		Problem->setFocus();
	}
	void review_tour_editor::show_notice(const QString& aText) { Status->setText(aText); }
	auto review_tour_editor::minimumSizeHint() const -> QSize { return { 280, 240 }; }
}
