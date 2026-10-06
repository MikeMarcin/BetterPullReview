#include "bpr/qt/review_submission_editor.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractTextDocumentLayout>
#include <QAccessible>
#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QSignalBlocker>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		constexpr auto decision_order = std::array{ core::review_disposition::comment, core::review_disposition::approve, core::review_disposition::request_changes };

		[[nodiscard]] auto decision_index(const core::review_disposition aDisposition) noexcept -> std::size_t
		{
			switch (aDisposition) {
			case core::review_disposition::comment:         return 0U;
			case core::review_disposition::approve:         return 1U;
			case core::review_disposition::request_changes: return 2U;
			}
			return 0U;
		}

		[[nodiscard]] auto text_of(const std::string& aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		/// [[BPR-1085]]. A 16 px icon drawn in one ink, on the marks' own
		/// geometry (BPR-1083): the round "unknown" state (states are round,
		/// labels square), the dashed "not sent" square my drafts carry beside
		/// the code (BPR-1290) with a pencil stroke, or the attention dot.
		enum class preflight_icon : std::uint8_t {
			unknown,
			draft,
			attention,
		};
		[[nodiscard]] auto preflight_pixmap(const preflight_icon aIcon, const QPalette& aPalette, const qreal aRatio, const QColor& aAttention = { }) -> QPixmap
		{
			const auto ratio  = std::max<qreal>(1.0, aRatio);
			const auto pixels = static_cast<int>(std::ceil(feedback_mark_size * ratio));
			auto       image  = QImage{ pixels, pixels, QImage::Format_ARGB32_Premultiplied };
			image.fill(Qt::transparent);
			image.setDevicePixelRatio(ratio);
			{
				auto painter = QPainter{ &image };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ink   = aPalette.color(QPalette::WindowText);
				const auto inset = feedback_mark_border / 2.0;
				const auto frame = QRectF{ inset, inset, feedback_mark_size - feedback_mark_border, feedback_mark_size - feedback_mark_border };
				switch (aIcon) {
				case preflight_icon::unknown: {
					painter.setPen(QPen{ ink, feedback_mark_border });
					painter.setBrush(Qt::NoBrush);
					painter.drawEllipse(frame);
					auto font = painter.font();
					font.setPixelSize(10);
					font.setBold(true);
					painter.setFont(font);
					painter.drawText(QRectF{ 0.0, 0.0, feedback_mark_size, feedback_mark_size }, Qt::AlignCenter, QStringLiteral("?"));
					break;
				}
				case preflight_icon::draft: {
					// The accepted open border: flat dashes, as a non-blocking
					// mark draws them, so the corners stay square-cut.
					auto border = QPen{ ink, feedback_mark_border };
					border.setCapStyle(Qt::FlatCap);
					border.setDashPattern({ feedback_mark_dash / feedback_mark_border, feedback_mark_dash_gap / feedback_mark_border });
					painter.setPen(border);
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(frame, feedback_mark_corner - inset, feedback_mark_corner - inset);
					// A pencil: its shaft, short of the border, and its tip.
					painter.setPen(QPen{ ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
					painter.drawLine(QPointF{ 6.5, 9.5 }, QPointF{ 10.5, 5.5 });
					painter.setPen(Qt::NoPen);
					painter.setBrush(ink);
					painter.drawPolygon(QPolygonF{ { QPointF{ 4.6, 11.4 }, QPointF{ 5.4, 8.9 }, QPointF{ 7.1, 10.6 } } });
					break;
				}
				case preflight_icon::attention: {
					painter.setPen(Qt::NoPen);
					painter.setBrush(aAttention);
					painter.drawEllipse(QRectF{ 4.0, 4.0, 8.0, 8.0 });
					break;
				}
				}
			}
			return QPixmap::fromImage(image);
		}

		/// [[BPR-1085]]. One chip, several marks each with its count (owner,
		/// preflight B). Drawn rather than composed from labels so it stays
		/// one keyboard stop with one accessible name.
		class preflight_chip final : public QToolButton
		{
		public:
			using QToolButton::QToolButton;

			void set_segments(std::vector<std::pair<QPixmap, QString>> aSegments)
			{
				Segments = std::move(aSegments);
				updateGeometry();
				update();
			}
			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = fontMetrics();
				auto       width   = padding * 2 + arrow;
				for (const auto& [mark, count] : Segments) {
					width += static_cast<int>(feedback_mark_size) + gap + metrics.horizontalAdvance(count) + spacing;
				}
				return { width, std::max(static_cast<int>(feedback_mark_size) + 8, metrics.height() + 8) };
			}
			[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QStylePainter{ this };
				auto option  = QStyleOptionToolButton{ };
				initStyleOption(&option);
				option.text.clear();
				option.icon = QIcon{ };
				painter.drawComplexControl(QStyle::CC_ToolButton, option);
				const auto metrics = fontMetrics();
				auto       x       = padding;
				const auto middle  = height() / 2;
				painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
				for (const auto& [mark, count] : Segments) {
					const auto side = static_cast<int>(feedback_mark_size);
					painter.drawPixmap(QRect{ x, middle - side / 2, side, side }, mark);
					x += side + gap;
					const auto advance = metrics.horizontalAdvance(count);
					painter.drawText(QRect{ x, 0, advance, height() }, Qt::AlignVCenter | Qt::AlignLeft, count);
					x += advance + spacing;
				}
			}

		private:
			static constexpr int                     padding = 6;
			static constexpr int                     gap     = 3;
			static constexpr int                     spacing = 8;
			static constexpr int                     arrow   = 10;
			std::vector<std::pair<QPixmap, QString>> Segments;
		};

		[[nodiscard]] auto collection_words(const core::feedback_collection aCollection) -> QString
		{
			switch (aCollection) {
			case core::feedback_collection::complete:    return { };
			case core::feedback_collection::partial:     return review_submission_editor::tr("Not every comment was read");
			case core::feedback_collection::stale:       return review_submission_editor::tr("Comments are from an earlier read");
			case core::feedback_collection::unavailable: return review_submission_editor::tr("Comments could not be read");
			}
			return { };
		}

		[[nodiscard]] auto uncertainty_lines(const core::review_preflight& aPreflight) -> QStringList
		{
			const auto& doubt = aPreflight.Uncertain;
			auto        lines = QStringList{ };
			if (const auto words = collection_words(doubt.Collection); !words.isEmpty()) {
				lines.push_back(words);
			}
			if (doubt.ResolutionUnknown > 0U) {
				lines.push_back(review_submission_editor::tr("%n of yours: GitHub didn't say if resolved", nullptr, static_cast<int>(doubt.ResolutionUnknown)));
			}
			if (doubt.Unclassified > 0U) {
				lines.push_back(review_submission_editor::tr("%n of yours: label not readable", nullptr, static_cast<int>(doubt.Unclassified)));
			}
			if (doubt.UnknownAuthor > 0U) {
				lines.push_back(review_submission_editor::tr("%n blocking or unreadable, author unknown", nullptr, static_cast<int>(doubt.UnknownAuthor)));
			}
			return lines;
		}

		[[nodiscard]] auto guidance_line(const core::review_preflight_guidance& aGuidance) -> QString
		{
			return review_submission_editor::tr("%n %1, not marked blocking", nullptr, static_cast<int>(aGuidance.Published + aGuidance.Pending)).arg(feedback_label_text(aGuidance.Label));
		}
	} // namespace

	auto review_preflight_summary(const std::optional<core::review_preflight>& aPreflight) -> QString
	{
		if (!aPreflight) {
			return review_submission_editor::tr("Your feedback on this revision has not been read: unknown, not none.");
		}
		auto lines = QStringList{ review_submission_editor::tr("Yours on GitHub: %n open blocking", nullptr, static_cast<int>(aPreflight->Blockers.size())) };
		if (!aPreflight->Pending.empty()) {
			lines.push_back(review_submission_editor::tr("Yours, not sent: %n blocking", nullptr, static_cast<int>(aPreflight->Pending.size())));
		}
		for (const auto& guidance : aPreflight->Guidance) {
			lines.push_back(guidance_line(guidance));
		}
		lines.append(uncertainty_lines(*aPreflight));
		if (aPreflight->Others > 0U) {
			lines.push_back(review_submission_editor::tr("Others: %n open blocking (not yours)", nullptr, static_cast<int>(aPreflight->Others)));
		}
		return lines.join(QLatin1Char{ '\n' });
	}

	review_submission_editor::review_submission_editor(review_submission_editor_dispatch aDispatch, QWidget* aParent)
		: QWidget{ aParent }
		, Dispatch{ std::move(aDispatch) }
	{
		setObjectName(QStringLiteral("wholeReviewEditor"));
		auto* layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		auto* title = new QLabel{ tr("Compose review"), this };
		layout->addWidget(title);
		auto* purpose = new QLabel{ tr("Write a summary for this pull request and choose Comment, Approve, or Request changes. Nothing is published until you choose Submit review."), this };
		purpose->setObjectName(QStringLiteral("wholeReviewPurpose"));
		purpose->setWordWrap(true);
		purpose->setMinimumWidth(0);
		purpose->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(purpose);
		DraftState = new QLabel{ this };
		DraftState->setObjectName(QStringLiteral("wholeReviewDraftState"));
		DraftState->setWordWrap(true);
		layout->addWidget(DraftState);
		Target = new QTextEdit{ this };
		Target->setObjectName(QStringLiteral("wholeReviewTarget"));
		Target->setReadOnly(true);
		Target->setAcceptRichText(false);
		Target->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
		Target->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Target->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Target->setFrameShape(QFrame::NoFrame);
		Target->setAccessibleName(tr("Review target and acting account"));
		Target->setMinimumWidth(0);
		Target->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		connect(Target->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
				this, [this](QSizeF aSize) { Target->setFixedHeight(static_cast<int>(std::ceil(aSize.height())) + 2 * Target->frameWidth()); });

		layout->addWidget(Target);
		// [[BPR-1085]]. The three native decisions are explicit, exclusive
		// buttons (owner, preflight B); each carries its native disposition.
		Disposition    = new QButtonGroup{ this };
		auto* decision = new QHBoxLayout;
		decision->setSpacing(4);
		const auto names = std::array{ tr("Comment"), tr("Approve"), tr("Request changes") };
		const auto ids   = std::array{ QStringLiteral("wholeReviewComment"), QStringLiteral("wholeReviewApprove"), QStringLiteral("wholeReviewRequestChanges") };
		for (std::size_t index = 0U; index < Decisions.size(); ++index) {
			auto* button = new QToolButton{ this };
			button->setObjectName(ids[index]);
			button->setText(names[index]);
			button->setCheckable(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setAccessibleName(names[index]);
			Disposition->addButton(button, static_cast<int>(index));
			decision->addWidget(button);
			Decisions[index] = button;
		}
		Disposition->setExclusive(true);
		decision->addStretch();
		layout->addLayout(decision);
		auto* preflightRow = new QHBoxLayout;
		Preflight          = new preflight_chip{ this };
		Preflight->setObjectName(QStringLiteral("wholeReviewPreflight"));
		Preflight->setFocusPolicy(Qt::StrongFocus);
		Preflight->setAccessibleName(tr("Your blocking feedback"));
		Preflight->setPopupMode(QToolButton::InstantPopup);
		PreflightMenu = new QMenu{ Preflight };
		PreflightMenu->setObjectName(QStringLiteral("wholeReviewPreflightMenu"));
		PreflightMenu->setToolTipsVisible(true);
		Preflight->setMenu(PreflightMenu);
		preflightRow->addWidget(Preflight);
		preflightRow->addStretch();
		layout->addLayout(preflightRow);
		Body = new QPlainTextEdit{ this };
		Body->setObjectName(QStringLiteral("wholeReviewSummary"));
		Body->setAccessibleName(tr("Review summary"));
		Body->setPlaceholderText(tr("Write a review summary…"));
		Body->setTabChangesFocus(true);
		Body->setMinimumHeight(160);
		layout->addWidget(Body, 1);
		Submit         = new QAction{ tr("Submit review"), this };
		Discard        = new QAction{ tr("Discard"), this };
		Reconcile      = new QAction{ tr("Check status"), this };
		NewReview      = new QAction{ tr("New review"), this };
		const auto row = [this, layout](QAction* aFirst, QAction* aSecond) {
			auto* actions = new QHBoxLayout;
			for (auto* action : { aFirst, aSecond }) {
				auto* button = new QToolButton{ this };
				button->setDefaultAction(action);
				button->setFocusPolicy(Qt::StrongFocus);
				button->setAccessibleName(action->text());
				actions->addWidget(button);
			}
			actions->addStretch();
			layout->addLayout(actions);
		};
		row(Submit, Discard);
		row(Reconcile, NewReview);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("wholeReviewStatus"));
		Status->setAccessibleName(tr("Review submission status"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Status->setMinimumWidth(0);
		Status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->insertWidget(3, Status);
		layout->addStretch();
		connect(Body, &QPlainTextEdit::textChanged, this, [this] { emit_command(core::review_submission_command_kind::edit); });
		connect(Disposition, &QButtonGroup::idClicked, this, [this] { emit_command(core::review_submission_command_kind::choose_disposition); });
		connect(Submit, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::begin_submit); });
		connect(Discard, &QAction::triggered, this, [this] { emit_command(core::review_submission_command_kind::discard); });
		connect(Reconcile, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::reconcile); });
		connect(NewReview, &QAction::triggered, this, [this] { emit_operation(review_submission_editor_operation::new_review); });
		setFocusProxy(Body);
		present({ .Status = tr("Sign in and open an active pull request to write a review. Back returns to the pull request.") });
	}
	void review_submission_editor::present(const review_submission_editor_projection& aProjection)
	{
		const QSignalBlocker bodyBlock{ Body };
		const QSignalBlocker dispositionBlock{ Disposition };
		Draft           = aProjection.Draft;
		Shown           = aProjection;
		const auto text = Draft ? QString::fromUtf8(Draft->Body) : QString{ };
		if (Body->toPlainText() != text) {
			Body->setPlainText(text);
		}
		if (Draft) {
			Decisions[decision_index(Draft->Disposition)]->setChecked(true);
		}
		if (Target->toPlainText() != aProjection.TargetDescription) {
			Target->setPlainText(aProjection.TargetDescription);
		}
		Target->setVisible(!aProjection.TargetDescription.isEmpty());
		Body->setVisible(Draft.has_value());
		for (auto* button : Decisions) {
			button->setVisible(Draft.has_value());
		}
		Preflight->setVisible(Draft.has_value());
		QString state;
		if (!Draft) {
			state = tr("No review draft is available.");
		} else {
			switch (Draft->State) {
			case core::review_submission_state::drafting:
				state = core::kept(*Draft) ? tr("Your review draft is kept. It has not been published.") : tr("Start a review draft. Write your summary below.");
				break;
			case core::review_submission_state::submitting:
				state = tr("Submission is in progress or awaiting confirmation. Check status before trying again.");
				break;
			case core::review_submission_state::submitted:
				state = tr("This review was submitted. Choose New review to write another.");
				break;
			case core::review_submission_state::discarded:
				state = tr("This draft was discarded. Choose New review to start again.");
				break;
			}
		}
		DraftState->setText(state);
		Status->setText(aProjection.Status);
		const auto connected = Draft.has_value() && static_cast<bool>(Dispatch);
		Body->setReadOnly(!connected || !aProjection.Controls.Edit);
		for (auto* button : Decisions) {
			button->setEnabled(connected && aProjection.Controls.Edit);
		}
		Submit->setEnabled(connected && aProjection.Controls.Submit);
		Discard->setEnabled(connected && aProjection.Controls.Discard);
		Reconcile->setEnabled(connected && aProjection.Controls.Reconcile);
		NewReview->setEnabled(connected && aProjection.Controls.NewReview);
		const auto unavailable = !Draft || !aProjection.Controls.Edit
			? aProjection.Status
			: tr("Write a nonblank summary for Comment or Request changes; Approve may have an empty summary.");
		Body->setToolTip(Body->isReadOnly() ? aProjection.Status : tr("Your summary is kept as a draft until you submit it."));
		present_preflight(aProjection);
		Submit->setToolTip(Submit->isEnabled() ? tr("Publish this review with the displayed account and revision.") : unavailable);
		Discard->setToolTip(Discard->isEnabled() ? tr("Discard this unpublished review draft.") : tr("Only an editable, unpublished draft can be discarded. %1").arg(aProjection.Status));
		Reconcile->setToolTip(Reconcile->isEnabled() ? tr("Check whether this exact review was submitted.") : tr("Check status is available only for a review awaiting confirmation."));
		NewReview->setToolTip(NewReview->isEnabled() ? tr("Start a new draft for the current pull request revision.") : tr("Finish or discard the current draft before starting another. %1").arg(aProjection.Status));
		const auto* chosen       = Disposition->checkedButton();
		const auto  announcement = aProjection.TargetDescription + QStringLiteral("\n") + (chosen != nullptr ? chosen->text() : QString{ }) + QStringLiteral(". ") + aProjection.Status;
		if (announcement != LastAnnouncement) {
			LastAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ this, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (PresentationObserver) {
			PresentationObserver();
		}
	}
	void review_submission_editor::present_preflight(const review_submission_editor_projection& aProjection)
	{
		const auto& preflight = aProjection.Preflight;
		const auto  ratio     = devicePixelRatioF();
		const auto  theme     = MarkTheme.value_or(default_feedback_mark_theme(palette()));
		const auto  mark      = [&](core::feedback_label aLabel, feedback_mark_emphasis aEmphasis) {
			return feedback_mark_pixmap(aLabel, aEmphasis, feedback_mark_colours_for(theme, aLabel, palette()), ratio);
		};
		const auto blocking = feedback_mark_emphasis{ .Weight = feedback_mark_weight::blocking };
		const auto summary  = review_preflight_summary(preflight);
		auto       segments = std::vector<std::pair<QPixmap, QString>>{ };
		if (preflight) {
			segments.emplace_back(mark(core::feedback_label::issue, blocking), QString::number(preflight->Blockers.size()));
			if (!preflight->Pending.empty()) {
				segments.emplace_back(preflight_pixmap(preflight_icon::draft, palette(), ratio), QString::number(preflight->Pending.size()));
			}
			if (!preflight->Exact) {
				// An incomplete read shows its mark even when it counts nothing.
				segments.emplace_back(preflight_pixmap(preflight_icon::unknown, palette(), ratio), QString::number(core::uncertain(*preflight)));
			}
		} else {
			segments.emplace_back(preflight_pixmap(preflight_icon::unknown, palette(), ratio), QString{ });
		}
		static_cast<preflight_chip*>(Preflight)->set_segments(std::move(segments));
		Preflight->setToolTip(summary);
		Preflight->setAccessibleDescription(summary);

		// Approve keeps working while I have open blocking feedback; its dot
		// and tooltip say what approving does not do (owner, preflight B).
		auto* approve = Decisions[decision_index(core::review_disposition::approve)];
		if (const auto open = preflight ? core::open_blocking(*preflight) : 0U; open > 0U) {
			approve->setIcon(QIcon{ preflight_pixmap(preflight_icon::attention, palette(), ratio, attention_colour()) });
			approve->setToolTip(tr("Approve. You have %n open blocking comment(s) of your own; approving does not resolve them.", nullptr, static_cast<int>(open)));
		} else {
			approve->setIcon(QIcon{ });
			approve->setToolTip(tr("Approve this pull request."));
		}
		approve->setAccessibleDescription(approve->toolTip());
		Decisions[decision_index(core::review_disposition::comment)]->setToolTip(tr("Comment without approving."));
		Decisions[decision_index(core::review_disposition::request_changes)]->setToolTip(tr("Request changes."));

		PreflightMenu->clear();
		const auto reveal = [this](QAction* aAction, const std::string& aId) {
			connect(aAction, &QAction::triggered, this, [this, aId] {
				if (Reveal) {
					Reveal(aId);
				}
			});
		};
		const auto row = [&](const std::string& aId, const QPixmap& aMark, const QString& aState) {
			const auto found = aProjection.References.find(aId);
			const auto words = found == aProjection.References.end() ? text_of(aId) : found->second.Where + QStringLiteral(" · ") + found->second.Subject;
			auto*      added = PreflightMenu->addAction(QIcon{ aMark }, aState.isEmpty() ? words : words + QStringLiteral(" · ") + aState);
			added->setToolTip(tr("Show it"));
			reveal(added, aId);
		};
		const auto note = [&](const QPixmap& aMark, const QString& aWords) {
			auto* added = aMark.isNull() ? PreflightMenu->addAction(aWords) : PreflightMenu->addAction(QIcon{ aMark }, aWords);
			added->setEnabled(false);
		};
		if (!preflight) {
			note(preflight_pixmap(preflight_icon::unknown, palette(), ratio), summary);
			return;
		}
		PreflightMenu->addSection(tr("Yours, on GitHub"));
		for (const auto& blocker : preflight->Blockers) {
			row(blocker.Conversation, mark(blocker.Label, blocking), blocker.Resolution == core::feedback_resolution::unknown ? tr("state unknown") : QString{ });
		}
		if (preflight->Blockers.empty()) {
			note({ }, preflight->Exact ? tr("None open") : tr("None found"));
		}
		if (!preflight->Pending.empty()) {
			PreflightMenu->addSection(tr("Yours, not sent"));
			for (const auto& pending : preflight->Pending) {
				row(pending.Draft, mark(pending.Label, blocking), { });
			}
		}
		if (!preflight->Guidance.empty() || !preflight->Exact || preflight->Others > 0U) {
			PreflightMenu->addSeparator();
		}
		for (const auto& guidance : preflight->Guidance) {
			note(mark(guidance.Label, { }), guidance_line(guidance));
		}
		for (const auto& line : uncertainty_lines(*preflight)) {
			note(preflight_pixmap(preflight_icon::unknown, palette(), ratio), line);
		}
		if (preflight->Others > 0U) {
			note({ }, tr("Others: %n open blocking (not yours)", nullptr, static_cast<int>(preflight->Others)));
		}
	}
	void review_submission_editor::set_presentation_observer(std::function<void()> aObserver)
	{
		PresentationObserver = std::move(aObserver);
	}
	void review_submission_editor::set_item_reveal(std::function<void(const std::string&)> aReveal)
	{
		Reveal = std::move(aReveal);
	}
	void review_submission_editor::set_label_marks(const feedback_mark_theme& aTheme)
	{
		MarkTheme = aTheme;
		present_preflight(Shown);
	}
	void review_submission_editor::set_attention_colour(const QColor& aColour)
	{
		if (Attention == aColour) {
			return;
		}
		Attention = aColour;
		present_preflight(Shown);
	}
	auto review_submission_editor::attention_colour() const -> QColor
	{
		if (Attention.has_value()) {
			return *Attention;
		}
		const auto variant = palette().color(QPalette::Base).lightnessF() < 0.5 ? core::appearance_variant::dark : core::appearance_variant::light;
		return to_presentation_color(builtin_appearance_palette(variant)[core::index_of(core::appearance_role::state_attention)]);
	}
	void review_submission_editor::emit_command(core::review_submission_command_kind aKind)
	{
		if (!Draft || !Dispatch) {
			return;
		}
		auto command = core::review_submission_command{ .Id = Draft->Id, .Kind = aKind };
		if (aKind == core::review_submission_command_kind::edit) {
			command.Body = Body->toPlainText().toUtf8().toStdString();
		}
		if (aKind == core::review_submission_command_kind::choose_disposition) {
			const auto chosen   = Disposition->checkedId();
			command.Disposition = decision_order[chosen >= 0 && chosen < static_cast<int>(decision_order.size()) ? static_cast<std::size_t>(chosen) : 0U];
		}
		auto intent = review_submission_editor_intent{ .Command = std::move(command) };
		if (aKind == core::review_submission_command_kind::begin_submit) {
			intent.Prepared = Shown.Prepared;
		}
		Dispatch(std::move(intent));
	}
	void review_submission_editor::emit_operation(review_submission_editor_operation aOperation)
	{
		if (Draft && Dispatch) {
			Dispatch({ .Operation = aOperation, .Command = { .Id = Draft->Id } });
		}
	}
	auto review_submission_editor::body_editor() const noexcept -> QPlainTextEdit* { return Body; }
	auto review_submission_editor::disposition_button(core::review_disposition aDisposition) const noexcept -> QToolButton* { return Decisions[decision_index(aDisposition)]; }
	auto review_submission_editor::preflight_button() const noexcept -> QToolButton* { return Preflight; }
	auto review_submission_editor::preflight_menu() const noexcept -> QMenu* { return PreflightMenu; }
	auto review_submission_editor::submit_action() const noexcept -> QAction* { return Submit; }
	auto review_submission_editor::discard_action() const noexcept -> QAction* { return Discard; }
	auto review_submission_editor::reconcile_action() const noexcept -> QAction* { return Reconcile; }
	auto review_submission_editor::new_review_action() const noexcept -> QAction* { return NewReview; }
	auto review_submission_editor::status_label() const noexcept -> QLabel* { return Status; }
	auto review_submission_editor::target_editor() const noexcept -> QTextEdit* { return Target; }
	auto review_submission_editor::shown_draft() const noexcept -> const std::optional<core::review_submission_draft>& { return Draft; }
	auto review_submission_editor::shown_projection() const noexcept -> const review_submission_editor_projection& { return Shown; }
}
