#include "bpr/qt/colour_role_list.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QColorDialog>
#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The list's own insets, on the 8-point grid the row language uses.
		constexpr int ListInset    = 16;
		constexpr int RowSpacing   = 12;
		constexpr int RowHeight    = 26;
		constexpr int SwatchSide   = 18;
		constexpr int SwatchColumn = 22;
		/// What the accepted design shows: a list about ten rows tall that scrolls.
		constexpr int PreferredWellHeight = 300;
		constexpr int MinimumWellHeight   = 140;

		[[nodiscard]] auto role_at(const std::size_t aIndex) noexcept
			-> core::appearance_role
		{
			return static_cast<core::appearance_role>(
				static_cast<std::uint8_t>(aIndex));
		}

		[[nodiscard]] auto group_at(const std::size_t aIndex) noexcept
			-> appearance_role_group
		{
			return static_cast<appearance_role_group>(
				static_cast<std::uint8_t>(aIndex));
		}

		/// \brief Every role index, by group and then in the core's own order.
		///
		/// \details [[BPR-1085]]. A role is declared at the end of the core's
		/// vocabulary, where its ordinal stays durable, even when it lists in an
		/// earlier group: `state_attention` lists under Review states. Within a
		/// group the core's order holds, so the earlier roles keep their places.
		[[nodiscard]] auto display_order() -> const std::array<std::size_t, core::appearance_role_count>&
		{
			static const auto order = [] {
				auto indices = std::array<std::size_t, core::appearance_role_count>{ };
				for (auto index = std::size_t{ 0U }; index < indices.size(); ++index) {
					indices[index] = index;
				}
				std::ranges::stable_sort(indices, { }, [](const std::size_t aIndex) {
					return static_cast<std::size_t>(appearance_role_group_of(role_at(aIndex)));
				});
				return indices;
			}();
			return order;
		}

		/// \brief Says one colour the way the design writes it.
		///
		/// \details Six digits for an opaque colour, which is what every row of the
		/// board shows, and eight when the colour carries an alpha, because a value
		/// that hid its own transparency would read as a colour the reviewer is not
		/// looking at.
		[[nodiscard]] auto colour_text(const core::appearance_color aColour) -> QString
		{
			const auto colour = to_presentation_color(aColour);
			return colour.alpha() == 255 ? colour.name(QColor::HexRgb)
										 : colour.name(QColor::HexArgb);
		}

		/// \brief Paints one colour as a swatch, bordered in the inherited palette.
		///
		/// \details A pixmap rather than a styled widget: no style sheet, and the
		/// border comes from the surface's own `QPalette::Mid` so a swatch whose
		/// colour matches the card is still visible in every theme.
		[[nodiscard]] auto swatch_icon(
			const core::appearance_color aColour,
			const QPalette&              aPalette,
			const qreal                  aRatio) -> QIcon
		{
			auto pixmap = QPixmap{ QSize{ SwatchSide, SwatchSide } * aRatio };
			pixmap.setDevicePixelRatio(aRatio);
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing);
			const auto body = QRectF{ 0.5,
									  0.5,
									  static_cast<qreal>(SwatchSide) - 1.0,
									  static_cast<qreal>(SwatchSide) - 1.0 };
			auto       path = QPainterPath{ };
			path.addRoundedRect(body, 4.0, 4.0);
			painter.fillPath(path, to_presentation_color(aColour));
			painter.setPen(QPen{ aPalette.color(QPalette::Mid), 1.0 });
			painter.drawPath(path);
			return QIcon{ pixmap };
		}
	} // namespace

	auto platform_colour_picker() -> colour_picker
	{
		return [](const colour_pick_request& aRequest) -> std::optional<QColor> {
			auto dialog = QColorDialog{ aRequest.Initial, aRequest.Parent };
			// A semantic colour carries an alpha, so the picker must be able to
			// express one; the selection wash is the role that makes this visible.
			dialog.setOption(QColorDialog::ShowAlphaChannel);
			dialog.setWindowTitle(aRequest.Title);
			if (aRequest.Changed) {
				QObject::connect(
					&dialog,
					&QColorDialog::currentColorChanged,
					&dialog,
					[&aRequest](const QColor& aColour) {
						aRequest.Changed(aColour);
					});
			}
			return dialog.exec() == QDialog::Accepted
					&& dialog.selectedColor().isValid()
				? std::optional{ dialog.selectedColor() }
				: std::nullopt;
		};
	}

	colour_role_list::colour_role_list(QWidget* const aParent)
		: QWidget{ aParent }
		, Picker{ platform_colour_picker() }
	{
		setObjectName(QStringLiteral("appearanceColourRoleList"));
		setAccessibleName(QStringLiteral("Colours"));

		Filter = new QLineEdit{ this };
		Filter->setObjectName(QStringLiteral("appearanceColourFilter"));
		Filter->setPlaceholderText(QStringLiteral("Filter roles"));
		Filter->setAccessibleName(QStringLiteral("Filter roles"));
		Filter->setClearButtonEnabled(true);

		Well = new QScrollArea{ this };
		Well->setObjectName(QStringLiteral("appearanceColourWell"));
		// [[BPR-0779]]. RFC-BPR-0034 C6 wants every control reachable by
		// keyboard to carry an accessible name. A QScrollArea defaults to
		// Qt::WheelFocus, which includes Tab, so this well is a real stop on the
		// way through the card and a screen reader would reach it unannounced.
		// It keeps that stop, because the well is how a keyboard reviewer
		// scrolls 45 roles, and is named instead. The theme strip's well takes
		// the other route (`theme_preview_strip.cpp`, Qt::NoFocus) because Left
		// and Right on the strip itself already scroll the selection into view.
		Well->setAccessibleName(QStringLiteral("Colour roles"));
		Well->setFrameShape(QFrame::NoFrame);
		Well->setWidgetResizable(true);
		Well->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		Well->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		Rows = new QWidget{ Well };
		Rows->setObjectName(QStringLiteral("appearanceColourRows"));
		auto* const column = new QVBoxLayout{ Rows };
		column->setContentsMargins(ListInset, 0, ListInset, ListInset / 2);
		column->setSpacing(0);
		Well->setWidget(Rows);

		// The pinned header lives on the viewport rather than in the column, so
		// showing it neither moves a row nor changes the list's height.
		Sticky.Frame = new QWidget{ Well->viewport() };
		Sticky.Frame->setObjectName(QStringLiteral("appearanceColourStickyHeader"));
		Sticky.Frame->setAutoFillBackground(true);
		Sticky.Frame->setBackgroundRole(QPalette::Base);
		auto* const stickyRow = new QHBoxLayout{ Sticky.Frame };
		stickyRow->setContentsMargins(ListInset, RowSpacing / 2, ListInset, 2);
		stickyRow->setSpacing(RowSpacing / 2);
		Sticky.Heading  = new QLabel{ Sticky.Frame };
		auto stickyFont = Sticky.Heading->font();
		stickyFont.setWeight(QFont::DemiBold);
		Sticky.Heading->setFont(stickyFont);
		Sticky.Count = new QLabel{ Sticky.Frame };
		Sticky.Count->setForegroundRole(QPalette::PlaceholderText);
		stickyRow->addWidget(Sticky.Heading);
		stickyRow->addWidget(Sticky.Count);
		stickyRow->addStretch(1);
		Sticky.Frame->hide();

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(RowSpacing / 2);
		layout->addWidget(Filter);
		layout->addWidget(Well, 1);

		Well->setMinimumHeight(MinimumWellHeight);
		Well->setMaximumHeight(PreferredWellHeight);

		QObject::connect(
			Filter, &QLineEdit::textChanged, this, [this](const QString& aText) {
				apply_filter(aText);
			});
		QObject::connect(
			Well->verticalScrollBar(),
			&QScrollBar::valueChanged,
			this,
			[this](int) { refresh_sticky_header(); });

		// The rows are deliberately not built here. See the class documentation:
		// fifty-eight rows of four widgets each is the largest fixed structure in
		// the application's chrome, and a reviewer who never opens Appearance should
		// not be carrying it.
	}

	void colour_role_list::ensure_rows()
	{
		if (Built) {
			return;
		}
		// Set first: everything below refreshes rows, and a refresh that asked
		// whether the rows existed would come back here.
		Built                = true;
		Statistics.RowsBuilt = true;

		auto* const column = qobject_cast<QVBoxLayout*>(Rows->layout());
		if (column == nullptr) {
			return;
		}

		// Built from the core's published count, in the core's own order, so a role
		// the core declares cannot be one this list omits by accident. The group
		// each row belongs to, and whether it is offered at all, are the
		// vocabulary's, which derives both from the same enumeration. A role that
		// is not offered has no row ([[BPR-0986]]).
		for (const auto index : display_order()) {
			const auto role = role_at(index);
			if (!appearance_role_offered(role)) {
				continue;
			}
			const auto group = appearance_role_group_of(role);
			if (GroupHeaders[static_cast<std::size_t>(group)].Frame == nullptr) {
				auto& header = GroupHeaders[static_cast<std::size_t>(group)];
				header.Frame = new QWidget{ Rows };
				header.Frame->setObjectName(
					QStringLiteral("appearanceColourGroup%1")
						.arg(static_cast<int>(group)));
				// Opaque on purpose: the pinned copy is drawn over the rows, and a
				// transparent header would let them show through it.
				header.Frame->setAutoFillBackground(true);
				header.Frame->setBackgroundRole(QPalette::Base);
				auto* const headerRow = new QHBoxLayout{ header.Frame };
				headerRow->setContentsMargins(0, RowSpacing / 2, 0, 2);
				headerRow->setSpacing(RowSpacing / 2);
				header.Heading
					= new QLabel{ appearance_role_group_text(group), header.Frame };
				auto heading = header.Heading->font();
				heading.setWeight(QFont::DemiBold);
				header.Heading->setFont(heading);
				header.Count = new QLabel{ header.Frame };
				header.Count->setForegroundRole(QPalette::PlaceholderText);
				headerRow->addWidget(header.Heading);
				headerRow->addWidget(header.Count);
				headerRow->addStretch(1);
				header.Frame->setAccessibleName(appearance_role_group_text(group));
				column->addWidget(header.Frame);
			}

			auto& entry = RoleRows[index];
			entry.Frame = new QWidget{ Rows };
			entry.Frame->setObjectName(
				QStringLiteral("appearanceColourRow%1").arg(static_cast<int>(index)));
			entry.Frame->setMinimumHeight(RowHeight);
			auto* const rowLayout = new QHBoxLayout{ entry.Frame };
			rowLayout->setContentsMargins(0, 2, 0, 2);
			rowLayout->setSpacing(RowSpacing);

			auto* const swatch = new QToolButton{ entry.Frame };
			swatch->setObjectName(
				QStringLiteral("appearanceColourSwatch%1")
					.arg(static_cast<int>(index)));
			swatch->setAutoRaise(true);
			swatch->setFocusPolicy(Qt::StrongFocus);
			swatch->setIconSize(QSize{ SwatchSide, SwatchSide });
			swatch->setFixedWidth(SwatchColumn);
			swatch->setAccessibleName(appearance_role_text(role));
			QObject::connect(swatch, &QToolButton::clicked, this, [this, role] {
				open_picker(role);
			});
			entry.Swatch = swatch;

			entry.Name = new QLabel{ appearance_role_text(role), entry.Frame };
			entry.Name->setTextInteractionFlags(Qt::NoTextInteraction);
			entry.Value    = new QLabel{ entry.Frame };
			entry.Previous = new QLabel{ entry.Frame };
			entry.Previous->setForegroundRole(QPalette::PlaceholderText);
			entry.Previous->hide();

			rowLayout->addWidget(entry.Swatch);
			rowLayout->addWidget(entry.Name, 1);
			rowLayout->addWidget(entry.Value);
			rowLayout->addWidget(entry.Previous);
			column->addWidget(entry.Frame);
		}
		column->addStretch(1);

		// Everything supplied before the rows existed now reaches them.
		apply_value_font();
		refresh_rows();
		apply_filter(Filter->text());
	}

	void colour_role_list::set_theme_colours(const core::appearance_palette& aColours)
	{
		if (ThemeColours == aColours) {
			return;
		}
		ThemeColours = aColours;
		if (Built) {
			refresh_rows();
		}
	}

	void colour_role_list::set_previewed_colours(
		std::optional<core::appearance_palette> aColours)
	{
		if (PreviewedColours == aColours) {
			return;
		}
		PreviewedColours = std::move(aColours);
		if (Built) {
			refresh_rows();
		}
	}

	void colour_role_list::set_value_font(QFont aFont)
	{
		if (ValueFontSupplied && ValueFont == aFont) {
			return;
		}
		ValueFont         = std::move(aFont);
		ValueFontSupplied = true;
		if (Built) {
			apply_value_font();
		}
	}

	void colour_role_list::set_picker(colour_picker aPicker)
	{
		Picker = std::move(aPicker);
	}

	void colour_role_list::set_dispatch(colour_edit_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void colour_role_list::apply_filter(const QString& aText)
	{
		if (Filter->text() != aText) {
			// Setting the field re-enters through its own change signal, which keeps
			// the field and the listed rows one fact rather than two.
			Filter->setText(aText);
			return;
		}
		if (!Built) {
			return;
		}
		auto listed = std::size_t{ 0U };
		for (auto index = std::size_t{ 0U }; index < RoleRows.size(); ++index) {
			if (RoleRows[index].Frame == nullptr) {
				continue;
			}
			const auto shown = aText.isEmpty()
				|| appearance_role_text(role_at(index))
					   .contains(aText, Qt::CaseInsensitive);
			RoleRows[index].Frame->setVisible(shown);
			listed += shown ? 1U : 0U;
		}
		Statistics.RowsListed = listed;
		refresh_counts();
		// A group that lists nothing has no heading to show, so the list never says
		// "Diff 0" over a gap.
		for (auto index = std::size_t{ 0U }; index < GroupHeaders.size(); ++index) {
			GroupHeaders[index].Frame->setVisible(
				GroupHeaders[index].Count->text() != QStringLiteral("0"));
		}
		refresh_sticky_header();
	}

	auto colour_role_list::filter_field() const noexcept -> QLineEdit*
	{
		return Filter;
	}

	auto colour_role_list::well() const noexcept -> QScrollArea*
	{
		return Well;
	}

	auto colour_role_list::sticky_header() const noexcept -> QWidget*
	{
		return Sticky.Frame;
	}

	auto colour_role_list::sticky_heading_text() const -> QString
	{
		return Sticky.Heading->text();
	}

	auto colour_role_list::row_of(const core::appearance_role aRole) -> QWidget*
	{
		ensure_rows();
		return RoleRows[core::index_of(aRole)].Frame;
	}

	auto colour_role_list::swatch_of(const core::appearance_role aRole)
		-> QAbstractButton*
	{
		ensure_rows();
		return RoleRows[core::index_of(aRole)].Swatch;
	}

	auto colour_role_list::name_label_of(const core::appearance_role aRole) -> QLabel*
	{
		ensure_rows();
		return RoleRows[core::index_of(aRole)].Name;
	}

	auto colour_role_list::value_label_of(const core::appearance_role aRole) -> QLabel*
	{
		ensure_rows();
		return RoleRows[core::index_of(aRole)].Value;
	}

	auto colour_role_list::previous_value_label_of(const core::appearance_role aRole)
		-> QLabel*
	{
		ensure_rows();
		return RoleRows[core::index_of(aRole)].Previous;
	}

	auto colour_role_list::group_header_of(const appearance_role_group aGroup)
		-> QWidget*
	{
		ensure_rows();
		return GroupHeaders[static_cast<std::size_t>(aGroup)].Frame;
	}

	auto colour_role_list::group_heading_text(const appearance_role_group aGroup)
		-> QString
	{
		ensure_rows();
		return GroupHeaders[static_cast<std::size_t>(aGroup)].Heading->text();
	}

	auto colour_role_list::group_count_text(const appearance_role_group aGroup)
		-> QString
	{
		ensure_rows();
		return GroupHeaders[static_cast<std::size_t>(aGroup)].Count->text();
	}

	auto colour_role_list::listed_roles() -> std::vector<core::appearance_role>
	{
		ensure_rows();
		auto roles = std::vector<core::appearance_role>{ };
		roles.reserve(RoleRows.size());
		for (const auto index : display_order()) {
			if (RoleRows[index].Frame != nullptr && !RoleRows[index].Frame->isHidden()) {
				roles.push_back(role_at(index));
			}
		}
		return roles;
	}

	auto colour_role_list::edited(const core::appearance_role aRole) const noexcept
		-> bool
	{
		return PreviewedColours.has_value()
			&& (*PreviewedColours)[core::index_of(aRole)]
			!= ThemeColours[core::index_of(aRole)];
	}

	auto colour_role_list::edited_roles() const noexcept -> std::size_t
	{
		auto edits = std::size_t{ 0U };
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			edits += edited(role_at(index)) ? 1U : 0U;
		}
		return edits;
	}

	auto colour_role_list::value_text(const core::appearance_role aRole) -> QString
	{
		ensure_rows();
		const auto* const value = RoleRows[core::index_of(aRole)].Value;
		return value != nullptr ? value->text() : QString{ };
	}

	auto colour_role_list::statistics() const noexcept
		-> colour_role_list_statistics
	{
		return Statistics;
	}

	void colour_role_list::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		refresh_sticky_header();
	}

	void colour_role_list::showEvent(QShowEvent* const aEvent)
	{
		// Opening Appearance is what makes the rows worth building.
		ensure_rows();
		QWidget::showEvent(aEvent);
	}

	auto colour_role_list::colour_of(const core::appearance_role aRole) const
		-> core::appearance_color
	{
		return PreviewedColours.has_value()
			? (*PreviewedColours)[core::index_of(aRole)]
			: ThemeColours[core::index_of(aRole)];
	}

	void colour_role_list::apply_value_font()
	{
		if (!ValueFontSupplied) {
			return;
		}
		for (auto& entry : RoleRows) {
			if (entry.Frame != nullptr) {
				entry.Value->setFont(ValueFont);
				entry.Previous->setFont(ValueFont);
			}
		}
	}

	void colour_role_list::refresh_row(const core::appearance_role aRole)
	{
		auto& entry = RoleRows[core::index_of(aRole)];
		if (entry.Frame == nullptr) {
			return;
		}
		const auto shown   = colour_of(aRole);
		const auto changed = edited(aRole);
		entry.Swatch->setIcon(swatch_icon(shown, palette(), devicePixelRatioF()));
		entry.Value->setText(colour_text(shown));
		// An edited row states both values, and states them as text rather than
		// only as a swatch: the new one in ink and the old one beside it.
		entry.Value->setForegroundRole(
			changed ? QPalette::WindowText : QPalette::PlaceholderText);
		auto name = entry.Name->font();
		name.setWeight(changed ? QFont::DemiBold : QFont::Normal);
		entry.Name->setFont(name);
		entry.Previous->setVisible(changed);
		entry.Previous->setText(
			changed ? QStringLiteral("was %1")
						  .arg(colour_text(ThemeColours[core::index_of(aRole)]))
					: QString{ });
		entry.Swatch->setAccessibleDescription(
			changed ? QStringLiteral(
						  "Opens the colour picker. Now %1, was %2; the change "
						  "previews in the review window as you pick.")
						  .arg(
							  colour_text(shown),
							  colour_text(ThemeColours[core::index_of(aRole)]))
					: QStringLiteral(
						  "Opens the colour picker. %1 now; the change previews in "
						  "the review window as you pick.")
						  .arg(colour_text(shown)));
	}

	void colour_role_list::refresh_rows()
	{
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			refresh_row(role_at(index));
		}
	}

	void colour_role_list::refresh_counts()
	{
		auto counted = std::array<std::size_t, appearance_role_group_count>{ };
		for (auto index = std::size_t{ 0U }; index < RoleRows.size(); ++index) {
			if (RoleRows[index].Frame != nullptr && !RoleRows[index].Frame->isHidden()) {
				++counted[static_cast<std::size_t>(
					appearance_role_group_of(role_at(index)))];
			}
		}
		// Counted rather than restated. The card prints exactly the offered
		// memberships the vocabulary reports, so a role added to one group cannot
		// leave a stale number beside it.
		for (auto index = std::size_t{ 0U }; index < GroupHeaders.size(); ++index) {
			GroupHeaders[index].Count->setText(
				QString::number(static_cast<qulonglong>(counted[index])));
			GroupHeaders[index].Frame->setAccessibleDescription(
				QStringLiteral("%1 colours")
					.arg(static_cast<qulonglong>(counted[index])));
		}
	}

	void colour_role_list::refresh_sticky_header()
	{
		if (!Built || Sticky.Frame == nullptr || Well == nullptr) {
			return;
		}
		// The group whose own header has scrolled above the viewport, which is the
		// one a reviewer has lost sight of and the only one worth pinning. Strictly
		// above: a header resting exactly at the top is the one in view, and pinning
		// a copy over it would draw the same heading twice.
		auto pinned = std::optional<appearance_role_group>{ };
		for (auto index = std::size_t{ 0U }; index < GroupHeaders.size(); ++index) {
			auto* const frame = GroupHeaders[index].Frame;
			if (frame == nullptr || frame->isHidden()) {
				continue;
			}
			const auto top = frame->mapTo(Well->viewport(), QPoint{ 0, 0 }).y();
			if (top < 0) {
				pinned = group_at(index);
			}
		}
		if (!pinned.has_value()) {
			Sticky.Frame->hide();
			return;
		}
		Sticky.Heading->setText(appearance_role_group_text(*pinned));
		Sticky.Count->setText(
			GroupHeaders[static_cast<std::size_t>(*pinned)].Count->text());
		Sticky.Frame->setAccessibleName(appearance_role_group_text(*pinned));
		Sticky.Frame->setGeometry(
			0, 0, Well->viewport()->width(), Sticky.Frame->sizeHint().height());
		Sticky.Frame->show();
		Sticky.Frame->raise();
	}

	void colour_role_list::open_picker(const core::appearance_role aRole)
	{
		const auto before = colour_of(aRole);
		++Statistics.PickersOpened;
		if (!Picker) {
			return;
		}
		const auto picked = Picker(colour_pick_request{
			.Title   = appearance_role_text(aRole),
			.Initial = to_presentation_color(before),
			.Changed = [this, aRole](const QColor& aColour) {
				++Statistics.LiveChanges;
				report(aRole, aColour);
			},
			.Parent = this,
		});
		if (picked.has_value()) {
			++Statistics.AcceptedPicks;
			report(aRole, *picked);
			return;
		}
		// Cancelled: the role goes back to the colour it had before the picker
		// opened, because every colour the picker moved through was previewed and
		// leaving the last one would keep a change nobody accepted.
		++Statistics.CancelledPicks;
		report(aRole, to_presentation_color(before));
	}

	void colour_role_list::report(
		const core::appearance_role aRole,
		const QColor&               aColour)
	{
		if (!aColour.isValid() || !Dispatch) {
			return;
		}
		Dispatch(aRole, to_appearance_color(aColour));
	}
} // namespace bpr::qt
