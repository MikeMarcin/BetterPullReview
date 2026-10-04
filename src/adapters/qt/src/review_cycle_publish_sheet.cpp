#include "bpr/qt/review_cycle_publish_sheet.hpp"

#include "bpr/qt/keyboard_focus_ring.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string& aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto revision_text(const core::cycle_source_revision_id aRevision) -> QString
		{
			return QStringLiteral("R%1").arg(aRevision.value());
		}

		/// A commit as people name one: its first seven hexadecimal digits.
		auto short_commit(const core::git_commit_oid& aCommit) -> QString
		{
			const auto hex = std::string{ core::canonical_hex(aCommit).view().substr(0U, 7U) };
			return QString::fromStdString(hex);
		}

		/// A path row's two lines as one text, for a course or a reader to take.
		constexpr int path_text_role = Qt::UserRole + 2;

		auto change_word(const core::outgoing_change aChange) -> QString
		{
			switch (aChange) {
			case core::outgoing_change::added:        return QStringLiteral("added");
			case core::outgoing_change::modified:     return QStringLiteral("changed");
			case core::outgoing_change::mode_changed: return QStringLiteral("mode changed");
			case core::outgoing_change::deleted:      return QStringLiteral("deleted");
			case core::outgoing_change::unknown:      return QStringLiteral("not known");
			case core::outgoing_change::unchanged:    return { };
			}
			return { };
		}

		/// Whether the reviewer may choose this path: one the plan asks about,
		/// or starting work nothing touched, which can be included deliberately.
		auto choosable(const core::outgoing_row& aRow) -> bool
		{
			return aRow.Gap == core::outgoing_gap::depends_on_starting_work || aRow.Gap == core::outgoing_gap::unknown_start || aRow.Unrelated;
		}

		/// The second line of a path: what happens to it and where its change came from.
		auto row_words(const core::outgoing_row& aRow) -> QString
		{
			auto words = QString{ };
			switch (aRow.Gap) {
			case core::outgoing_gap::depends_on_starting_work:
				words = aRow.Choice ? (aRow.Selected ? QStringLiteral("Included, with the starting work it depends on") : QStringLiteral("Left out, as in the base commit"))
									: QStringLiteral("Needs a choice: the cycle's improvement depends on starting work left out");
				break;
			case core::outgoing_gap::unknown_start:     words = QStringLiteral("Needs a choice: changed since the base, but the first capture did not see it"); break;
			case core::outgoing_gap::final_unknown:     words = QStringLiteral("Stays as in the base commit: the latest capture did not read it"); break;
			case core::outgoing_gap::final_unavailable: words = QStringLiteral("Stays as in the base commit: the latest capture saw it but could not read it"); break;
			case core::outgoing_gap::filter_managed:    words = QStringLiteral("Cannot be built here: a Git filter such as LFS manages it. Save a patch instead."); break;
			case core::outgoing_gap::gitlink:           words = QStringLiteral("Cannot be built here: a submodule changed. Save a patch instead."); break;
			case core::outgoing_gap::path_conflict:     words = QStringLiteral("Cannot be built: a file and a folder would share this path"); break;
			case core::outgoing_gap::none:
				if (aRow.Unrelated && !aRow.Selected) {
					words = QStringLiteral("Stays out: starting work nothing in the cycle touched");
				} else if (!aRow.Selected) {
					words = QStringLiteral("Stays out");
				} else {
					const auto origin = aRow.StartingWork != core::outgoing_change::unchanged && aRow.Improvement != core::outgoing_change::unchanged
						? QStringLiteral("starting work and the cycle's improvement")
						: aRow.StartingWork != core::outgoing_change::unchanged ? QStringLiteral("starting work from before the cycle")
																				: QStringLiteral("the cycle's improvement");
					words             = QStringLiteral("Goes out %1: %2").arg(change_word(aRow.Outgoing), origin);
				}
				break;
			}
			if (aRow.RenamedTo) {
				words += QStringLiteral(". Moved to %1").arg(text_of(*aRow.RenamedTo));
			}
			if (aRow.LineEndingsOnly) {
				words += QStringLiteral(". Only its line endings differ");
			}
			if (aRow.Drifted) {
				words += QStringLiteral(". The target changed this file since the base commit");
			}
			return words;
		}

		auto stage_title(const application::review_cycle_publication_view& aView, const application::review_cycle_workbench_projection& aProjection) -> QString
		{
			switch (aView.Stage) {
			case application::review_cycle_publication_stage::choosing: {
				const auto latest = aProjection.Revisions.empty() ? QString{ } : QStringLiteral(" ") + revision_text(aProjection.Revisions.back());
				return QStringLiteral("Publish%1 as a pull request").arg(latest);
			}
			case application::review_cycle_publication_stage::planned:    return QStringLiteral("Publish this plan?");
			case application::review_cycle_publication_stage::publishing: return QStringLiteral("Publishing");
			case application::review_cycle_publication_stage::linked:
				return aView.Current && aView.Current->Journal.Link ? QStringLiteral("Pull request #%1 is open").arg(aView.Current->Journal.Link->Number) : QStringLiteral("The pull request is open");
			case application::review_cycle_publication_stage::stopped: return QStringLiteral("Publishing stopped");
			}
			return { };
		}

		auto effect_words(const core::publication_journal& aJournal) -> QString
		{
			auto confirmed = QStringList{ };
			auto open      = QStringList{ };
			for (const auto& effect : aJournal.Effects) {
				const auto name = QString::fromLatin1(core::name(effect.Kind).data(), static_cast<qsizetype>(core::name(effect.Kind).size())).replace(QLatin1Char('_'), QLatin1Char(' '));
				if (effect.State == core::publication_effect_state::confirmed) {
					if (!confirmed.contains(name)) {
						confirmed.append(name);
					}
				} else {
					open.append(QStringLiteral("%1 %2").arg(name, QString::fromLatin1(core::name(effect.State).data(), static_cast<qsizetype>(core::name(effect.State).size()))));
				}
			}
			auto words = confirmed.isEmpty() ? QStringLiteral("GitHub confirmed nothing yet.") : QStringLiteral("GitHub holds: %1.").arg(confirmed.join(QStringLiteral(", ")));
			if (!open.isEmpty()) {
				words += QStringLiteral(" Not confirmed: %1.").arg(open.join(QStringLiteral(", ")));
			}
			return words;
		}

		/// The short account of a recorded plan, or of the pull request it became.
		auto summary_text(const application::review_cycle_publication_view& aView) -> QString
		{
			if (!aView.Current) {
				return { };
			}
			const auto& plan        = aView.Current->Plan;
			const auto& destination = plan.Destination;
			const auto  repository  = text_of(destination.Owner) + QLatin1Char('/') + text_of(destination.Repository);
			auto        lines       = QStringList{ };
			if (aView.Stage == application::review_cycle_publication_stage::linked && aView.Current->Journal.Link) {
				const auto& link = *aView.Current->Journal.Link;
				lines.append(QStringLiteral("Pull request #%1 on %2: %3 into %4%5.")
								 .arg(link.Number)
								 .arg(repository, text_of(destination.NewBranch), text_of(link.BaseBranch), plan.Draft ? QStringLiteral(", opened as a draft") : QString{ }));
				lines.append(QStringLiteral("Its head is %1, the one commit built from %2 to %3 of this cycle.").arg(short_commit(link.Head), revision_text(plan.Start), revision_text(plan.Final)));
				if (link.Drifted) {
					lines.append(QStringLiteral("GitHub's head or base is not what the plan named: someone pushed, or the target moved. GitHub shows what is shared now."));
				}
				return lines.join(QLatin1Char('\n'));
			}
			lines.append(QStringLiteral("One commit whose only parent is %1: %2 added or changed, %3 deleted%4.")
							 .arg(short_commit(plan.Parent))
							 .arg(plan.Blobs.size())
							 .arg(plan.Deleted.size())
							 .arg(plan.Transformations.empty() ? QStringLiteral(", every byte as captured") : QStringLiteral(", %1 with CRLF line endings made LF").arg(plan.Transformations.size())));
			lines.append(QStringLiteral("Author and committer: %1 <%2>, unsigned.").arg(text_of(plan.Content.Author.Name), text_of(plan.Content.Author.Email)));
			lines.append(QStringLiteral("Into %1 on %2, from the new branch %3, %4.")
							 .arg(text_of(destination.BaseBranch), repository, text_of(destination.NewBranch), plan.Draft ? QStringLiteral("as a draft pull request") : QStringLiteral("ready for review")));
			lines.append(QStringLiteral("Title: %1").arg(text_of(plan.PullTitle)));
			if (!aView.Form.Problem.empty()) {
				lines.append(QStringLiteral("Problem: %1").arg(text_of(aView.Form.Problem)));
			}
			if (!aView.Form.Solution.empty()) {
				lines.append(QStringLiteral("Solution: %1").arg(text_of(aView.Form.Solution)));
			}
			if (aView.Stage == application::review_cycle_publication_stage::stopped || aView.Stage == application::review_cycle_publication_stage::publishing) {
				lines.append(effect_words(aView.Current->Journal));
			}
			return lines.join(QLatin1Char('\n'));
		}

		auto line_edit(QWidget* aParent, const QString& aName, const QString& aAccessible, const QString& aPlaceholder = { }) -> QLineEdit*
		{
			auto* const field = new QLineEdit{ aParent };
			field->setObjectName(aName);
			field->setAccessibleName(aAccessible);
			field->setPlaceholderText(aPlaceholder);
			return field;
		}

		auto words_label(QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			return label;
		}

		auto section(QWidget* aParent, const QString& aText) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        font  = label->font();
			font.setBold(true);
			label->setFont(font);
			label->setContentsMargins(0, 8, 0, 0);
			return label;
		}
	}

	review_cycle_publish_sheet::review_cycle_publish_sheet(QWidget* aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("cyclePublishSheet"));
		setWindowTitle(QStringLiteral("Publish as Pull Request"));
		setWindowModality(Qt::WindowModal);
#ifdef Q_OS_MACOS
		// AppKit attaches it to the window, under the toolbar, over the code.
		setWindowFlag(Qt::Sheet, true);
#endif
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 16, 20, 14);
		column->setSpacing(8);

		Title     = words_label(this, QStringLiteral("cyclePublishTitle"));
		auto font = Title->font();
		font.setBold(true);
		font.setPointSizeF(font.pointSizeF() * 1.25);
		Title->setFont(font);
		column->addWidget(Title);
		Intro = words_label(this, QStringLiteral("cyclePublishIntro"));
		column->addWidget(Intro);

		Body = new QScrollArea{ this };
		Body->setObjectName(QStringLiteral("cyclePublishBody"));
		Body->setWidgetResizable(true);
		Body->setFrameShape(QFrame::NoFrame);
		Body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Body->setFocusPolicy(Qt::NoFocus);
		Fields             = new QWidget{ Body };
		auto* const fields = new QVBoxLayout{ Fields };
		fields->setContentsMargins(0, 0, 8, 0);
		fields->setSpacing(6);

		// Once there is a plan or a pull request, a short account of it stands
		// above the baselines, and the form is put away: short by default, the exact plan on request.
		Summary = words_label(Fields, QStringLiteral("cyclePublishSummary"));
		Summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Summary->hide();
		fields->addWidget(Summary);

		// The baselines, each named for what it is.
		fields->addWidget(section(Fields, QStringLiteral("Starts from")));
		auto* const baselines = new QGridLayout{ };
		baselines->setHorizontalSpacing(10);
		baselines->setVerticalSpacing(4);
		auto* const baseLabel = new QLabel{ QStringLiteral("Base commit"), Fields };
		BaseLabel             = baseLabel;
		Base                  = line_edit(Fields, QStringLiteral("cyclePublishBase"), QStringLiteral("Base commit"), QStringLiteral("HEAD, a branch or a commit"));
		baseLabel->setBuddy(Base);
		BaseWords = words_label(Fields, QStringLiteral("cyclePublishBaseWords"));
		baselines->addWidget(baseLabel, 0, 0);
		baselines->addWidget(Base, 0, 1);
		baselines->addWidget(BaseWords, 1, 1);
		auto* const targetLabel = new QLabel{ QStringLiteral("Compared with"), Fields };
		TargetLabel             = targetLabel;
		Target                  = line_edit(Fields, QStringLiteral("cyclePublishTarget"), QStringLiteral("Target branch in this checkout"), QStringLiteral("refs/remotes/origin/main (optional)"));
		targetLabel->setBuddy(Target);
		TargetWords = words_label(Fields, QStringLiteral("cyclePublishTargetWords"));
		baselines->addWidget(targetLabel, 2, 0);
		baselines->addWidget(Target, 2, 1);
		baselines->addWidget(TargetWords, 3, 1);
		baselines->setColumnStretch(1, 1);
		fields->addLayout(baselines);
		CycleWords = words_label(Fields, QStringLiteral("cyclePublishCycleWords"));
		fields->addWidget(CycleWords);

		// What goes out.
		fields->addWidget(section(Fields, QStringLiteral("What goes out")));
		Modes    = new QButtonGroup{ this };
		BothMode = new QRadioButton{ QStringLiteral("The starting work the cycle touched, and its improvements"), Fields };
		BothMode->setObjectName(QStringLiteral("cyclePublishBoth"));
		ImprovementsMode = new QRadioButton{ QStringLiteral("Only the cycle's improvements"), Fields };
		ImprovementsMode->setObjectName(QStringLiteral("cyclePublishImprovements"));
		Modes->addButton(BothMode, static_cast<int>(core::outgoing_selection_mode::starting_work_and_improvements));
		Modes->addButton(ImprovementsMode, static_cast<int>(core::outgoing_selection_mode::improvements_only));
		fields->addWidget(BothMode);
		fields->addWidget(ImprovementsMode);
		Paths = new QListWidget{ Fields };
		Paths->setObjectName(QStringLiteral("cyclePublishPaths"));
		Paths->setAccessibleName(QStringLiteral("Changed files"));
		Paths->setWordWrap(true);
		Paths->setSelectionMode(QAbstractItemView::NoSelection);
		Paths->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Paths->setMinimumHeight(64);
		fields->addWidget(Paths);
		PathsProblem = words_label(Fields, QStringLiteral("cyclePublishPathsProblem"));
		PathsProblem->hide();
		fields->addWidget(PathsProblem);

		// Where, and as whom.
		Form                   = new QWidget{ Fields };
		auto* const formColumn = new QVBoxLayout{ Form };
		formColumn->setContentsMargins(0, 0, 0, 0);
		formColumn->setSpacing(6);
		fields->addWidget(Form);
		formColumn->addWidget(section(Form, QStringLiteral("Where")));
		AccountWords = words_label(Form, QStringLiteral("cyclePublishAccount"));
		formColumn->addWidget(AccountWords);
		auto* const where = new QFormLayout{ };
		auto* const repo  = new QHBoxLayout{ };
		Owner             = line_edit(Form, QStringLiteral("cyclePublishOwner"), QStringLiteral("Repository owner"), QStringLiteral("owner"));
		Repository        = line_edit(Form, QStringLiteral("cyclePublishRepository"), QStringLiteral("Repository name"), QStringLiteral("repository"));
		repo->addWidget(Owner, 1);
		repo->addWidget(new QLabel{ QStringLiteral("/"), Form });
		repo->addWidget(Repository, 2);
		where->addRow(QStringLiteral("Repository"), repo);
		BaseBranch = line_edit(Form, QStringLiteral("cyclePublishBaseBranch"), QStringLiteral("Pull request into"), QStringLiteral("main"));
		where->addRow(QStringLiteral("Into"), BaseBranch);
		NewBranch = line_edit(Form, QStringLiteral("cyclePublishNewBranch"), QStringLiteral("New branch"), QStringLiteral("a branch that does not exist yet"));
		where->addRow(QStringLiteral("New branch"), NewBranch);
		formColumn->addLayout(where);

		// The pull request's public text, and the commit's author.
		formColumn->addWidget(section(Form, QStringLiteral("Pull request")));
		auto* const text = new QFormLayout{ };
		PullTitle        = line_edit(Form, QStringLiteral("cyclePublishTitleField"), QStringLiteral("Pull request title"));
		text->addRow(QStringLiteral("Title"), PullTitle);
		Problem = new QPlainTextEdit{ Form };
		Problem->setObjectName(QStringLiteral("cyclePublishProblem"));
		Problem->setAccessibleName(QStringLiteral("Problem"));
		Problem->setPlaceholderText(QStringLiteral("What was wrong, for a reader of the pull request"));
		Problem->setTabChangesFocus(true);
		Problem->setFixedHeight(Problem->fontMetrics().lineSpacing() * 3 + 12);
		text->addRow(QStringLiteral("Problem"), Problem);
		Solution = new QPlainTextEdit{ Form };
		Solution->setObjectName(QStringLiteral("cyclePublishSolution"));
		Solution->setAccessibleName(QStringLiteral("Solution"));
		Solution->setPlaceholderText(QStringLiteral("What this change does about it"));
		Solution->setTabChangesFocus(true);
		Solution->setFixedHeight(Solution->fontMetrics().lineSpacing() * 3 + 12);
		text->addRow(QStringLiteral("Solution"), Solution);
		Draft = new QCheckBox{ QStringLiteral("Open as a draft"), Form };
		Draft->setObjectName(QStringLiteral("cyclePublishDraft"));
		text->addRow(QString{ }, Draft);
		auto* const author = new QHBoxLayout{ };
		AuthorName         = line_edit(Form, QStringLiteral("cyclePublishAuthorName"), QStringLiteral("Commit author name"), QStringLiteral("name"));
		AuthorEmail        = line_edit(Form, QStringLiteral("cyclePublishAuthorEmail"), QStringLiteral("Commit author email"), QStringLiteral("email"));
		author->addWidget(AuthorName, 1);
		author->addWidget(AuthorEmail, 1);
		text->addRow(QStringLiteral("Author"), author);
		formColumn->addLayout(text);

		PlanText = new QPlainTextEdit{ Fields };
		PlanText->setObjectName(QStringLiteral("cyclePublishPlan"));
		PlanText->setAccessibleName(QStringLiteral("The exact plan"));
		PlanText->setReadOnly(true);
		PlanText->setLineWrapMode(QPlainTextEdit::NoWrap);
		PlanText->setMinimumHeight(160);
		PlanText->hide();
		fields->addWidget(PlanText);
		fields->addStretch(1);
		Body->setWidget(Fields);
		column->addWidget(Body, 1);

		// The one structure the sheet spends its weight on: what goes to GitHub,
		// beside what stays on this computer, always in view.
		auto* const ledger = new QFrame{ this };
		ledger->setObjectName(QStringLiteral("cyclePublishLedger"));
		ledger->setFrameShape(QFrame::StyledPanel);
		auto* const ledgerRow = new QHBoxLayout{ ledger };
		ledgerRow->setContentsMargins(12, 8, 12, 8);
		ledgerRow->setSpacing(12);
		const auto ledgerColumn = [&](const QString& aHeading, QLabel*& aWords, const QString& aName) {
			auto* const part   = new QVBoxLayout{ };
			auto* const header = section(ledger, aHeading);
			header->setObjectName(aName + QStringLiteral("Heading"));
			header->setContentsMargins(0, 0, 0, 0);
			part->addWidget(header);
			aWords = words_label(ledger, aName);
			part->addWidget(aWords);
			part->addStretch(1);
			ledgerRow->addLayout(part, 1);
		};
		ledgerColumn(QStringLiteral("Goes to GitHub"), OutgoingWords, QStringLiteral("cyclePublishOutgoingWords"));
		auto* const divider = new QFrame{ ledger };
		divider->setFrameShape(QFrame::VLine);
		divider->setFrameShadow(QFrame::Sunken);
		ledgerRow->addWidget(divider);
		ledgerColumn(QStringLiteral("Stays on this computer"), PrivateWords, QStringLiteral("cyclePublishPrivateWords"));
		column->addWidget(ledger);

		Status = words_label(this, QStringLiteral("cyclePublishStatus"));
		Status->setAccessibleName(QStringLiteral("Publication status"));
		Status->hide();
		column->addWidget(Status);

		// Linking a pull request made with one's own Git tools.
		auto* const linkRow = new QHBoxLayout{ };
		LinkAddress         = line_edit(this, QStringLiteral("cyclePublishLinkAddress"), QStringLiteral("Pull request address"), QStringLiteral("The pull request's address, pasted from your browser"));
		LinkConfirm         = new QPushButton{ QStringLiteral("Link"), this };
		LinkConfirm->setObjectName(QStringLiteral("cyclePublishLinkConfirm"));
		LinkConfirm->setAutoDefault(false);
		linkRow->addWidget(LinkAddress, 1);
		linkRow->addWidget(LinkConfirm);
		LinkAddress->hide();
		LinkConfirm->hide();
		column->addLayout(linkRow);

		auto* const actions = new QHBoxLayout{ };
		Primary             = new QPushButton{ QStringLiteral("Review Plan"), this };
		Primary->setObjectName(QStringLiteral("cyclePublishPrimary"));
		Primary->setDefault(true);
		actions->addWidget(Primary);
		ExactPlan = new QPushButton{ QStringLiteral("Read the Exact Plan"), this };
		ExactPlan->setObjectName(QStringLiteral("cyclePublishExactPlan"));
		ExactPlan->setAutoDefault(false);
		ExactPlan->setCheckable(true);
		ExactPlan->setToolTip(QStringLiteral("Everything publishing sends, with the digest your confirmation names"));
		actions->addWidget(ExactPlan);
		Change = new QPushButton{ QStringLiteral("Change…"), this };
		Change->setObjectName(QStringLiteral("cyclePublishChange"));
		Change->setAutoDefault(false);
		Change->setToolTip(QStringLiteral("Back to the choices; this plan is dropped and a new one is made"));
		actions->addWidget(Change);
		CancelPublishing = new QPushButton{ QStringLiteral("Cancel Publishing"), this };
		CancelPublishing->setObjectName(QStringLiteral("cyclePublishCancelPublication"));
		CancelPublishing->setAutoDefault(false);
		CancelPublishing->setToolTip(QStringLiteral("Stop this publication; anything GitHub already holds stays there"));
		actions->addWidget(CancelPublishing);
		actions->addStretch(1);
		column->addLayout(actions);

		auto* const foot = new QHBoxLayout{ };
		SavePatch        = new QPushButton{ QStringLiteral("Save Patch…"), this };
		SavePatch->setObjectName(QStringLiteral("cyclePublishSavePatch"));
		SavePatch->setFlat(true);
		SavePatch->setAutoDefault(false);
		SavePatch->setToolTip(QStringLiteral("Save what would go out as a patch, to commit and push with your own Git tools"));
		foot->addWidget(SavePatch);
		Link = new QPushButton{ QStringLiteral("Link Existing Pull Request…"), this };
		Link->setObjectName(QStringLiteral("cyclePublishLink"));
		Link->setFlat(true);
		Link->setAutoDefault(false);
		Link->setToolTip(QStringLiteral("Link a pull request you published yourself; it is read first and marked externally published"));
		foot->addWidget(Link);
		foot->addStretch(1);
		Close = new QPushButton{ QStringLiteral("Cancel"), this };
		Close->setObjectName(QStringLiteral("cyclePublishClose"));
		Close->setAutoDefault(false);
		Close->setToolTip(QStringLiteral("Close; a recorded plan stays as it is (Esc)"));
		foot->addWidget(Close);
		column->addLayout(foot);

		// Every field change is the form's; the host decides what it invalidates.
		for (auto* const field : { Base, Target, Owner, Repository, BaseBranch, NewBranch, PullTitle, AuthorName, AuthorEmail }) {
			connect(field, &QLineEdit::editingFinished, this, [this, field] {
				if (!Presenting && field->isModified()) {
					field->setModified(false);
					send_form();
				}
			});
		}
		connect(Modes, &QButtonGroup::idClicked, this, [this] {
			if (!Presenting) {
				send_form();
			}
		});
		connect(Draft, &QCheckBox::toggled, this, [this] {
			if (!Presenting) {
				send_form();
			}
		});
		connect(Primary, &QPushButton::clicked, this, [this] { primary(); });
		connect(ExactPlan, &QPushButton::toggled, this, [this](const bool aOpen) {
			PlanText->setVisible(aOpen);
			if (aOpen) {
				Body->ensureWidgetVisible(PlanText);
			}
		});
		connect(Change, &QPushButton::clicked, this, [this] {
			if (Dispatch.Revise) {
				Dispatch.Revise();
			}
		});
		connect(CancelPublishing, &QPushButton::clicked, this, [this] {
			if (Dispatch.CancelPublication) {
				Dispatch.CancelPublication();
			}
		});
		connect(SavePatch, &QPushButton::clicked, this, [this] {
			send_form();
			if (Dispatch.SavePatch) {
				Dispatch.SavePatch();
			}
		});
		connect(Link, &QPushButton::clicked, this, [this] {
			LinkAddress->show();
			LinkConfirm->show();
			LinkAddress->setFocus(Qt::OtherFocusReason);
		});
		const auto link = [this] {
			if (Dispatch.Link && !LinkAddress->text().trimmed().isEmpty()) {
				Dispatch.Link(utf8(LinkAddress->text().trimmed()));
			}
		};
		connect(LinkConfirm, &QPushButton::clicked, this, link);
		connect(LinkAddress, &QLineEdit::returnPressed, this, link);
		connect(Close, &QPushButton::clicked, this, &QDialog::reject);
#ifdef Q_OS_MACOS
		// Control-C cancels on macOS, as in the code view (BPR-1157); elsewhere Ctrl+C is Copy.
		auto* const keys = new QShortcut{ QKeySequence{ Qt::META | Qt::Key_C }, this };
		keys->setContext(Qt::WindowShortcut);
		connect(keys, &QShortcut::activated, this, &QDialog::reject);
#endif
		ring_keyboard_focus(*this, QStringLiteral("cycleKeyboardFocus"));
	}

	void review_cycle_publish_sheet::set_dispatch(review_cycle_publish_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto review_cycle_publish_sheet::current_form() const -> application::review_cycle_publication_form
	{
		auto form        = Shown.Form;
		form.Base        = utf8(Base->text().trimmed());
		form.Target      = utf8(Target->text().trimmed());
		form.Mode        = ImprovementsMode->isChecked() ? core::outgoing_selection_mode::improvements_only : core::outgoing_selection_mode::starting_work_and_improvements;
		form.Owner       = utf8(Owner->text().trimmed());
		form.Repository  = utf8(Repository->text().trimmed());
		form.BaseBranch  = utf8(BaseBranch->text().trimmed());
		form.NewBranch   = utf8(NewBranch->text().trimmed());
		form.Title       = utf8(PullTitle->text().trimmed());
		form.Problem     = utf8(Problem->toPlainText());
		form.Solution    = utf8(Solution->toPlainText());
		form.Draft       = Draft->isChecked();
		form.AuthorName  = utf8(AuthorName->text().trimmed());
		form.AuthorEmail = utf8(AuthorEmail->text().trimmed());
		// A path's check box is an explicit choice; a mode change starts the
		// choices again from what the new mode proposes.
		if (form.Mode == Shown.Form.Mode) {
			for (int index = 0; index < Paths->count(); ++index) {
				auto* const       item  = Paths->item(index);
				const auto* const row   = Paths->itemWidget(item);
				const auto* const check = row != nullptr ? row->findChild<QCheckBox*>(QStringLiteral("cyclePublishPathChoice")) : nullptr;
				if (check == nullptr) {
					continue;
				}
				const auto path    = utf8(item->data(Qt::UserRole).toString());
				const auto choice  = check->isChecked() ? core::outgoing_choice::include : core::outgoing_choice::exclude;
				const auto present = std::ranges::find(form.Overrides, path, &core::outgoing_override::Path);
				const auto shownAs = item->data(Qt::UserRole + 1).toBool();
				if (present != form.Overrides.end()) {
					present->Choice = choice;
				} else if ((choice == core::outgoing_choice::include) != shownAs) {
					form.Overrides.push_back({ .Path = path, .Choice = choice });
				}
			}
		} else {
			form.Overrides.clear();
		}
		return form;
	}

	void review_cycle_publish_sheet::send_form() const
	{
		if (Dispatch.Edit) {
			Dispatch.Edit(current_form());
		}
	}

	void review_cycle_publish_sheet::primary()
	{
		switch (Shown.Stage) {
		case application::review_cycle_publication_stage::choosing:
			// The words still in the fields are part of what is planned.
			send_form();
			if (Dispatch.Prepare) {
				Dispatch.Prepare();
			}
			return;
		case application::review_cycle_publication_stage::planned:
			if (current_form() != Shown.Form) {
				send_form();
				return;
			}
			if (Dispatch.Publish) {
				Dispatch.Publish();
			}
			return;
		case application::review_cycle_publication_stage::stopped:
			if (Shown.Current && Shown.Current->Journal.Phase == core::publication_phase::uncertain && Dispatch.Recover) {
				Dispatch.Recover();
			} else {
				reject();
			}
			return;
		case application::review_cycle_publication_stage::linked:
			if (Shown.Current && Shown.Current->Journal.Link && Dispatch.OpenPullRequest) {
				Dispatch.OpenPullRequest();
			}
			return;
		case application::review_cycle_publication_stage::publishing: return;
		}
	}

	void review_cycle_publish_sheet::fill_paths(const application::review_cycle_publication_view& aView)
	{
		Paths->clear();
		PathsProblem->setVisible(!aView.OutgoingProblem.empty());
		PathsProblem->setText(text_of(aView.OutgoingProblem));
		if (!aView.Outgoing) {
			Paths->setVisible(false);
			return;
		}
		const auto& plan = *aView.Outgoing;
		Paths->setVisible(!plan.Rows.empty());
		for (const auto& row : plan.Rows) {
			// Each path is a small row: its name, bold, over what happens to it,
			// in the palette's quieter colour; a check box first when it takes a choice.
			auto* const item = new QListWidgetItem{ Paths };
			item->setData(Qt::UserRole, text_of(row.Path));
			item->setData(Qt::UserRole + 1, row.Selected);
			item->setData(path_text_role, text_of(row.Path) + QLatin1Char('\n') + row_words(row));
			item->setData(Qt::AccessibleTextRole, text_of(row.Path) + QStringLiteral(". ") + row_words(row));
			item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsUserCheckable);
			auto* const widget = new QWidget{ Paths };
			auto* const line   = new QHBoxLayout{ widget };
			line->setContentsMargins(6, 3, 6, 3);
			line->setSpacing(8);
			if (choosable(row)) {
				auto* const check = new QCheckBox{ widget };
				check->setObjectName(QStringLiteral("cyclePublishPathChoice"));
				check->setAccessibleName(QStringLiteral("Include %1").arg(text_of(row.Path)));
				check->setChecked(row.Selected);
				check->setEnabled(aView.Stage == application::review_cycle_publication_stage::choosing && !aView.Busy);
				connect(check, &QCheckBox::toggled, this, [this] {
					if (!Presenting) {
						send_form();
					}
				});
				line->addWidget(check, 0, Qt::AlignTop);
			}
			auto* const words = new QVBoxLayout{ };
			words->setSpacing(0);
			auto* const name   = new QLabel{ text_of(row.Path), widget };
			auto        strong = name->font();
			strong.setBold(true);
			name->setFont(strong);
			name->setTextFormat(Qt::PlainText);
			auto* const what = new QLabel{ row_words(row), widget };
			what->setTextFormat(Qt::PlainText);
			what->setWordWrap(true);
			what->setForegroundRole(QPalette::PlaceholderText);
			words->addWidget(name);
			words->addWidget(what);
			line->addLayout(words, 1);
			item->setSizeHint(widget->sizeHint());
			Paths->setItemWidget(item, widget);
		}
		if (plan.NoChange) {
			PathsProblem->setText(QStringLiteral("Nothing differs from the base commit, so there is nothing to publish."));
			PathsProblem->show();
		}
	}

	void review_cycle_publish_sheet::present(const application::review_cycle_publication_view& aView, const application::review_cycle_workbench_projection& aProjection)
	{
		Presenting                 = true;
		const auto previous        = Shown;
		Shown                      = aView;
		const auto        stage    = aView.Stage;
		const auto        planned  = stage == application::review_cycle_publication_stage::planned;
		const auto        linked   = stage == application::review_cycle_publication_stage::linked;
		const auto        stopped  = stage == application::review_cycle_publication_stage::stopped;
		const auto* const recorded = aView.Current ? &aView.Current->Plan : nullptr;
		const auto        setText  = [](QLineEdit* aField, const std::string& aText) {
			// Text the reviewer is typing stays theirs until it is sent.
			if (!aField->hasFocus() || !aField->isModified()) {
				aField->setText(text_of(aText));
				aField->setModified(false);
			}
		};
		Title->setText(stage_title(aView, aProjection));
		switch (stage) {
		case application::review_cycle_publication_stage::choosing:
			Intro->setText(QStringLiteral("One new commit on a new branch, built from what you choose here. Your checkout is not changed, and nothing leaves this computer until you publish."));
			break;
		case application::review_cycle_publication_stage::planned:
			Intro->setText(aView.Account ? QStringLiteral("Nothing has been sent. Publishing sends exactly this commit, branch and pull request, as %1 on %2.").arg(text_of(aView.Account->Login), text_of(aView.Account->Host))
										 : QStringLiteral("Nothing has been sent."));
			break;
		case application::review_cycle_publication_stage::publishing: Intro->setText(QStringLiteral("Sending the objects, then the branch, then the pull request.")); break;
		case application::review_cycle_publication_stage::linked:
			Intro->setText(QStringLiteral("Your revisions, notes, rounds and replies stay on this computer. GitHub now holds the pull request's shared state; open it to review it there, or to start a Review Tour."));
			break;
		case application::review_cycle_publication_stage::stopped: Intro->setText(QStringLiteral("Nothing is sent again without you. What GitHub already holds stays there.")); break;
		}

		const auto& form = aView.Form;
		setText(Base, form.Base);
		setText(Target, form.Target);
		BothMode->setChecked(form.Mode == core::outgoing_selection_mode::starting_work_and_improvements);
		ImprovementsMode->setChecked(form.Mode == core::outgoing_selection_mode::improvements_only);
		setText(Owner, form.Owner);
		setText(Repository, form.Repository);
		setText(BaseBranch, form.BaseBranch);
		setText(NewBranch, form.NewBranch);
		setText(PullTitle, form.Title);
		setText(AuthorName, form.AuthorName);
		setText(AuthorEmail, form.AuthorEmail);
		if (!Problem->hasFocus() && utf8(Problem->toPlainText()) != form.Problem) {
			Problem->setPlainText(text_of(form.Problem));
		}
		if (!Solution->hasFocus() && utf8(Solution->toPlainText()) != form.Solution) {
			Solution->setPlainText(text_of(form.Solution));
		}
		Draft->setChecked(form.Draft);
		// Choosing shows the form; a plan or a pull request shows its short account instead.
		const auto choosing = stage == application::review_cycle_publication_stage::choosing;
		for (auto* const field : std::initializer_list<QWidget*>{ Base, Target, BothMode, ImprovementsMode, Owner, Repository, BaseBranch, NewBranch, PullTitle, Problem, Solution, Draft, AuthorName, AuthorEmail }) {
			field->setEnabled(choosing && !aView.Busy);
		}
		for (auto* const choice : Paths->findChildren<QCheckBox*>(QStringLiteral("cyclePublishPathChoice"))) {
			choice->setEnabled(choosing && !aView.Busy);
		}
		Form->setVisible(choosing);
		// The baselines' words stay; what changes them is shown only while choosing.
		for (auto* const control : std::initializer_list<QWidget*>{ BaseLabel, Base, TargetLabel, Target, BothMode, ImprovementsMode }) {
			control->setVisible(choosing);
		}
		Summary->setVisible(!choosing && aView.Current.has_value());
		Summary->setText(summary_text(aView));

		// The baselines, from the outgoing plan or the recorded one.
		const auto* const plan = aView.Outgoing ? &*aView.Outgoing : nullptr;
		if (plan != nullptr) {
			BaseWords->setText(QStringLiteral("The new commit's only parent: %1%2.")
								   .arg(short_commit(plan->BaseCommit), plan->BaseReference.empty() ? QString{ } : QStringLiteral(", where %1 points").arg(text_of(plan->BaseReference))));
			if (plan->TargetHead) {
				TargetWords->setText(plan->TargetMoved ? QStringLiteral("%1 has moved on to %2, as this checkout last saw it. The pull request will show that difference too.")
															 .arg(text_of(plan->TargetReference), short_commit(*plan->TargetHead))
													   : QStringLiteral("%1 is at the same commit, as this checkout last saw it. GitHub's answer is read again when you publish.").arg(text_of(plan->TargetReference)));
			} else {
				TargetWords->setText(QStringLiteral("No target branch named. GitHub's answer for the branch you publish into is read when you publish."));
			}
			CycleWords->setText(QStringLiteral("Your cycle runs from %1, the first capture, to %2, the latest. The pull request shows the change from the base commit to the new one, not from %1 to %2.")
									.arg(revision_text(plan->StartRevision), revision_text(plan->FinalRevision)));
		} else if (recorded != nullptr) {
			BaseWords->setText(QStringLiteral("The new commit's only parent: %1.").arg(short_commit(recorded->Parent)));
			TargetWords->setText(recorded->TargetHead ? QStringLiteral("The target was at %1 when planned.").arg(short_commit(*recorded->TargetHead)) : QString{ });
			CycleWords->setText(QStringLiteral("Published from %1 to %2 of your cycle.").arg(revision_text(recorded->Start), revision_text(recorded->Final)));
		} else {
			BaseWords->clear();
			TargetWords->clear();
			CycleWords->setText(aView.Busy ? QStringLiteral("Reading what would go out…") : QString{ });
		}
		const auto pathsChanged = previous.Outgoing != aView.Outgoing || previous.OutgoingProblem != aView.OutgoingProblem || Paths->count() == 0;
		if (pathsChanged) {
			fill_paths(aView);
			// As tall as its rows, up to six, then it scrolls.
			auto height = 0;
			for (int index = 0; index < Paths->count() && index < 6; ++index) {
				height += Paths->item(index)->sizeHint().height();
			}
			Paths->setFixedHeight(std::max(height, Paths->fontMetrics().lineSpacing() * 2) + 2 * Paths->frameWidth() + 4);
		}

		AccountWords->setText(aView.Account ? QStringLiteral("As %1 on %2.").arg(text_of(aView.Account->Login), text_of(aView.Account->Host))
											: QStringLiteral("Not signed in to GitHub. You can still save a patch and link the pull request afterwards."));

		// What goes to GitHub, and what stays here.
		auto changed = std::size_t{ 0 };
		auto deleted = std::size_t{ 0 };
		if (recorded != nullptr) {
			changed = recorded->Blobs.size();
			deleted = recorded->Deleted.size();
		} else if (plan != nullptr) {
			for (const auto& row : plan->Rows) {
				if (row.Selected && row.Outgoing == core::outgoing_change::deleted) {
					++deleted;
				} else if (row.Selected && row.Outgoing != core::outgoing_change::unchanged && row.Outgoing != core::outgoing_change::unknown) {
					++changed;
				}
			}
		}
		OutgoingWords->setText(QStringLiteral("One commit with %1 added or changed and %2 deleted %3, the branch %4, the title and the description. Nothing else.")
								   .arg(changed)
								   .arg(deleted)
								   .arg(changed + deleted == 1U ? QStringLiteral("file") : QStringLiteral("files"), form.NewBranch.empty() ? QStringLiteral("you name") : text_of(form.NewBranch)));
		const auto revisions = aProjection.Revisions.empty() ? QString{ }
			: aProjection.Revisions.size() == 1U
			? revision_text(aProjection.Revisions.front())
			: QStringLiteral("%1 to %2").arg(revision_text(aProjection.Revisions.front()), revision_text(aProjection.Revisions.back()));
		const auto notes     = aProjection.Notes.size();
		const auto rounds    = aProjection.Facts.Rounds.size();
		const auto replies   = aProjection.Responses.size();
		PrivateWords->setText(QStringLiteral("%1, %2 %3, %4 %5, %6 %7 and your decisions. None of it is uploaded.")
								  .arg(revisions)
								  .arg(notes)
								  .arg(notes == 1U ? QStringLiteral("note") : QStringLiteral("notes"))
								  .arg(rounds)
								  .arg(rounds == 1U ? QStringLiteral("round") : QStringLiteral("rounds"))
								  .arg(replies)
								  .arg(replies == 1U ? QStringLiteral("reply") : QStringLiteral("replies")));

		if (auto* const heading = findChild<QLabel*>(QStringLiteral("cyclePublishOutgoingWordsHeading"))) {
			heading->setText(linked ? QStringLiteral("Sent to GitHub") : QStringLiteral("Goes to GitHub"));
		}
		if (previous.Stage != stage) {
			// A new stage starts short: the exact plan is there on request.
			ExactPlan->setChecked(false);
		}
		PlanText->setPlainText(text_of(aView.Preview));
		ExactPlan->setVisible(!aView.Preview.empty());
		if (aView.Preview.empty()) {
			ExactPlan->setChecked(false);
		}
		Status->setVisible(!aView.Status.empty() || !aView.Instructions.empty());
		Status->setText(text_of(aView.Status) + (aView.Instructions.empty() ? QString{ } : QStringLiteral("\n") + text_of(aView.Instructions)));

		switch (stage) {
		case application::review_cycle_publication_stage::choosing:   Primary->setText(QStringLiteral("Review Plan")); break;
		case application::review_cycle_publication_stage::planned:    Primary->setText(QStringLiteral("Publish Pull Request")); break;
		case application::review_cycle_publication_stage::publishing: Primary->setText(QStringLiteral("Publishing…")); break;
		case application::review_cycle_publication_stage::linked:     Primary->setText(QStringLiteral("Open Pull Request")); break;
		case application::review_cycle_publication_stage::stopped:
			Primary->setText(aView.Current && aView.Current->Journal.Phase == core::publication_phase::uncertain ? QStringLiteral("Check GitHub") : QStringLiteral("Done"));
			break;
		}
		Primary->setEnabled(!aView.Busy && stage != application::review_cycle_publication_stage::publishing
							&& !(stage == application::review_cycle_publication_stage::choosing && (!aView.Account || (plan != nullptr && plan->NoChange))));
		Primary->setToolTip(planned ? QStringLiteral("Confirms plan %1 and sends exactly it").arg(text_of(aView.Digest.substr(0, std::min<std::size_t>(12U, aView.Digest.size())))) : QString{ });
		CancelPublishing->setVisible(stopped && aView.Current && aView.Current->Journal.Phase != core::publication_phase::cancelled);
		Change->setVisible(planned);
		Change->setEnabled(!aView.Busy);
		SavePatch->setVisible(!linked);
		SavePatch->setEnabled(!aView.Busy);
		// Another pull request made by hand can be linked whatever this one's state.
		Link->setVisible(stage != application::review_cycle_publication_stage::publishing);
		Link->setEnabled(!aView.Busy);
		LinkConfirm->setEnabled(!aView.Busy);
		Close->setText(linked || stopped ? QStringLiteral("Done") : QStringLiteral("Cancel"));
		Presenting = false;
		if (isVisible() && previous.Stage != stage) {
			focus_first();
		}
	}

	void review_cycle_publish_sheet::set_anchor(const QPoint aGlobal)
	{
		Anchor = aGlobal;
	}

	void review_cycle_publish_sheet::focus_first()
	{
		if (Primary->isEnabled()) {
			Primary->setFocus(Qt::OtherFocusReason);
		} else if (Owner->isEnabled() && Owner->text().isEmpty()) {
			Owner->setFocus(Qt::OtherFocusReason);
		} else {
			Close->setFocus(Qt::OtherFocusReason);
		}
	}

	void review_cycle_publish_sheet::done(const int aResult)
	{
		QDialog::done(aResult);
		if (Dispatch.Back) {
			Dispatch.Back();
		}
	}

	void review_cycle_publish_sheet::showEvent(QShowEvent* aEvent)
	{
		place();
		QDialog::showEvent(aEvent);
	}

	void review_cycle_publish_sheet::place()
	{
		const auto* const host = parentWidget() != nullptr ? parentWidget()->window() : nullptr;
		if (host == nullptr) {
			return;
		}
		// As wide as Share notes, as tall as the window allows under the
		// review bar; the middle scrolls.
		const auto area   = host->geometry();
		const auto top    = Anchor ? Anchor->y() : area.top() + 64;
		const auto room   = std::max(360, area.bottom() - top - 24);
		const auto width  = std::clamp(area.width() - 96, 560, 780);
		const auto height = std::min(room, 760);
		resize(width, height);
		const auto centre = Anchor ? Anchor->x() : area.center().x();
		move(centre - width / 2, top);
	}

	auto review_cycle_publish_sheet::title() const noexcept -> QLabel*
	{
		return Title;
	}
	auto review_cycle_publish_sheet::intro() const noexcept -> QLabel*
	{
		return Intro;
	}
	auto review_cycle_publish_sheet::base_words() const noexcept -> QLabel*
	{
		return BaseWords;
	}
	auto review_cycle_publish_sheet::target_words() const noexcept -> QLabel*
	{
		return TargetWords;
	}
	auto review_cycle_publish_sheet::cycle_words() const noexcept -> QLabel*
	{
		return CycleWords;
	}
	auto review_cycle_publish_sheet::base_field() const noexcept -> QLineEdit*
	{
		return Base;
	}
	auto review_cycle_publish_sheet::target_field() const noexcept -> QLineEdit*
	{
		return Target;
	}
	auto review_cycle_publish_sheet::starting_and_improvements() const noexcept -> QRadioButton*
	{
		return BothMode;
	}
	auto review_cycle_publish_sheet::improvements_only() const noexcept -> QRadioButton*
	{
		return ImprovementsMode;
	}
	auto review_cycle_publish_sheet::paths() const noexcept -> QListWidget*
	{
		return Paths;
	}
	auto review_cycle_publish_sheet::owner_field() const noexcept -> QLineEdit*
	{
		return Owner;
	}
	auto review_cycle_publish_sheet::repository_field() const noexcept -> QLineEdit*
	{
		return Repository;
	}
	auto review_cycle_publish_sheet::base_branch_field() const noexcept -> QLineEdit*
	{
		return BaseBranch;
	}
	auto review_cycle_publish_sheet::new_branch_field() const noexcept -> QLineEdit*
	{
		return NewBranch;
	}
	auto review_cycle_publish_sheet::account_words() const noexcept -> QLabel*
	{
		return AccountWords;
	}
	auto review_cycle_publish_sheet::title_field() const noexcept -> QLineEdit*
	{
		return PullTitle;
	}
	auto review_cycle_publish_sheet::problem_field() const noexcept -> QPlainTextEdit*
	{
		return Problem;
	}
	auto review_cycle_publish_sheet::solution_field() const noexcept -> QPlainTextEdit*
	{
		return Solution;
	}
	auto review_cycle_publish_sheet::draft_box() const noexcept -> QCheckBox*
	{
		return Draft;
	}
	auto review_cycle_publish_sheet::author_name_field() const noexcept -> QLineEdit*
	{
		return AuthorName;
	}
	auto review_cycle_publish_sheet::author_email_field() const noexcept -> QLineEdit*
	{
		return AuthorEmail;
	}
	auto review_cycle_publish_sheet::outgoing_words() const noexcept -> QLabel*
	{
		return OutgoingWords;
	}
	auto review_cycle_publish_sheet::private_words() const noexcept -> QLabel*
	{
		return PrivateWords;
	}
	auto review_cycle_publish_sheet::status() const noexcept -> QLabel*
	{
		return Status;
	}
	auto review_cycle_publish_sheet::primary_button() const noexcept -> QPushButton*
	{
		return Primary;
	}
	auto review_cycle_publish_sheet::exact_plan_button() const noexcept -> QPushButton*
	{
		return ExactPlan;
	}
	auto review_cycle_publish_sheet::cancel_publication_button() const noexcept -> QPushButton*
	{
		return CancelPublishing;
	}
	auto review_cycle_publish_sheet::save_patch_button() const noexcept -> QPushButton*
	{
		return SavePatch;
	}
	auto review_cycle_publish_sheet::link_button() const noexcept -> QPushButton*
	{
		return Link;
	}
	auto review_cycle_publish_sheet::link_field() const noexcept -> QLineEdit*
	{
		return LinkAddress;
	}
	auto review_cycle_publish_sheet::close_button() const noexcept -> QPushButton*
	{
		return Close;
	}
	auto review_cycle_publish_sheet::change_button() const noexcept -> QPushButton*
	{
		return Change;
	}
	auto review_cycle_publish_sheet::summary() const noexcept -> QLabel*
	{
		return Summary;
	}
	auto review_cycle_publish_sheet::plan_text() const noexcept -> QPlainTextEdit*
	{
		return PlanText;
	}
}
