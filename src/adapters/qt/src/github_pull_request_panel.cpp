#include "bpr/qt/github_pull_request_panel.hpp"
#include "bpr/qt/disclosure_button.hpp"

#include "github_presentation_text.hpp"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QToolButton>

namespace bpr::qt {
	using detail::pull_request_state_text;
	using detail::set_plain_text_label;
	using detail::text;

	github_pull_request_panel::github_pull_request_panel(QWidget* const aParent)
		: QFrame{ aParent }
	{
		setFrameShape(QFrame::NoFrame);
		setObjectName(QStringLiteral("pullRequestMetadataGroup"));
		setAccessibleName(QStringLiteral("Pull request metadata"));

		auto* const metadataLayout = new QGridLayout{ this };
		metadataLayout->setContentsMargins(0, 0, 0, 0);
		metadataLayout->setColumnStretch(0, 1);

		IdentityLabel = new QLabel{ this };
		IdentityLabel->setObjectName(QStringLiteral("pullRequestIdentity"));
		IdentityLabel->setAccessibleName(QStringLiteral("Pull request identity"));
		set_plain_text_label(IdentityLabel);
		TitleLabel = new QLabel{ this };
		TitleLabel->setObjectName(QStringLiteral("pullRequestTitle"));
		TitleLabel->setAccessibleName(QStringLiteral("Pull request title"));
		TitleLabel->setWordWrap(true);
		set_plain_text_label(TitleLabel);
		StateLabel = new QLabel{ this };
		StateLabel->setObjectName(QStringLiteral("pullRequestState"));
		StateLabel->setAccessibleName(QStringLiteral("Pull request state"));
		set_plain_text_label(StateLabel);
		AuthorLabel = new QLabel{ this };
		AuthorLabel->setObjectName(QStringLiteral("pullRequestAuthor"));
		AuthorLabel->setAccessibleName(QStringLiteral("Pull request author"));
		set_plain_text_label(AuthorLabel);
		BranchLabel = new QLabel{ this };
		BranchLabel->setObjectName(QStringLiteral("pullRequestBranches"));
		BranchLabel->setAccessibleName(QStringLiteral("Pull request branches"));
		BranchLabel->setWordWrap(true);
		set_plain_text_label(BranchLabel);
		OidLabel = new QLabel{ this };
		OidLabel->setObjectName(QStringLiteral("pullRequestCommitOids"));
		OidLabel->setAccessibleName(
			QStringLiteral("Pull request commit identities"));
		OidLabel->setWordWrap(true);
		set_plain_text_label(OidLabel);
		CountLabel = new QLabel{ this };
		CountLabel->setObjectName(QStringLiteral("pullRequestCounts"));
		CountLabel->setAccessibleName(
			QStringLiteral("Pull request record counts"));
		CountLabel->setWordWrap(true);
		set_plain_text_label(CountLabel);
		BodyEditor = new QPlainTextEdit{ this };
		BodyEditor->setObjectName(QStringLiteral("pullRequestBody"));
		BodyEditor->setAccessibleName(QStringLiteral("Pull request description"));
		BodyEditor->setAccessibleDescription(
			QStringLiteral("The pull request description as its author wrote it"));
		BodyEditor->setReadOnly(true);
		BodyEditor->setTabChangesFocus(true);
		BodyEditor->setMinimumHeight(120);

		// [[BPR-1579]]. A disclosure bar drawn from the theme, not the style.
		DetailsButton = new disclosure_button{ this };
		DetailsButton->setObjectName(
			QStringLiteral("githubTechnicalDetailsButton"));
		DetailsButton->setText(QStringLiteral("Technical details"));
		DetailsButton->setAccessibleName(
			QStringLiteral("Show pull request technical details"));
		DetailsButton->setAccessibleDescription(
			QStringLiteral("Reveal the base and head commit identities"));

		TechnicalDetails = new QWidget{ this };
		TechnicalDetails->setObjectName(QStringLiteral("githubTechnicalDetails"));
		auto* const technicalLayout = new QHBoxLayout{ TechnicalDetails };
		technicalLayout->setContentsMargins(0, 0, 0, 0);
		auto* const commitsLabel = new QLabel{
			QStringLiteral("Commit identities:"), TechnicalDetails
		};
		technicalLayout->addWidget(commitsLabel);
		technicalLayout->addWidget(OidLabel, 1);
		TechnicalDetails->hide();

		connect(
			DetailsButton,
			&QToolButton::toggled,
			this,
			[this](const bool aVisible) {
				TechnicalDetails->setVisible(aVisible);
				DetailsButton->setAccessibleName(
					aVisible
						? QStringLiteral("Hide pull request technical details")
						: QStringLiteral("Show pull request technical details"));
			});

		metadataLayout->addWidget(IdentityLabel, 0, 0);
		metadataLayout->addWidget(
			StateLabel,
			0,
			1,
			Qt::AlignRight | Qt::AlignVCenter);
		metadataLayout->addWidget(TitleLabel, 1, 0, 1, 2);
		metadataLayout->addWidget(AuthorLabel, 2, 0);
		metadataLayout->addWidget(BranchLabel, 3, 0, 1, 2);
		metadataLayout->addWidget(CountLabel, 4, 0);
		metadataLayout->addWidget(
			DetailsButton,
			4,
			1,
			Qt::AlignRight | Qt::AlignVCenter);
		metadataLayout->addWidget(TechnicalDetails, 5, 0, 1, 2);
	}

	void github_pull_request_panel::adopt(
		const application::github_pull_request_projection& aPullRequest)
	{
		IdentityLabel->setText(
			QStringLiteral("%1/%2 #%3")
				.arg(
					text(aPullRequest.Owner),
					text(aPullRequest.RepositoryName),
					QString::number(aPullRequest.Number.value())));
		TitleLabel->setText(text(aPullRequest.Title));
		StateLabel->setText(
			pull_request_state_text(aPullRequest.State, aPullRequest.Draft));
		AuthorLabel->setText(
			aPullRequest.AuthorLogin.empty()
				? QStringLiteral("Unknown author")
				: QStringLiteral("@%1").arg(text(aPullRequest.AuthorLogin)));
		BranchLabel->setText(
			QStringLiteral("%1 → %2")
				.arg(text(aPullRequest.BaseRef), text(aPullRequest.HeadRef)));
		OidLabel->setText(
			QStringLiteral("Base %1\nHead %2")
				.arg(text(aPullRequest.BaseOid), text(aPullRequest.HeadOid)));
		auto commitCount = QString::number(aPullRequest.CommitCount);
		if (aPullRequest.ReturnedCommitCount < aPullRequest.CommitCount) {
			commitCount += QStringLiteral(" (%1 returned by GitHub)")
							   .arg(QString::number(aPullRequest.ReturnedCommitCount));
		}
		CountLabel->setText(
			QStringLiteral(
				"%1 files · %2 commits · %3 reviews · %4 review threads · "
				"%5 pull request comments")
				.arg(
					QString::number(aPullRequest.FileCount),
					commitCount,
					QString::number(aPullRequest.ReviewCount),
					QString::number(aPullRequest.ReviewThreadCount),
					QString::number(aPullRequest.IssueCommentCount)));
		BodyEditor->setPlainText(text(aPullRequest.Body));
	}

	void github_pull_request_panel::clear()
	{
		IdentityLabel->setText(QStringLiteral("No pull request is open"));
		TitleLabel->setText(QStringLiteral("Not available"));
		StateLabel->setText(QStringLiteral("Not available"));
		AuthorLabel->setText(QStringLiteral("Not available"));
		BranchLabel->setText(QStringLiteral("Not available"));
		OidLabel->setText(QStringLiteral("Not available"));
		CountLabel->setText(QStringLiteral("No records"));
		BodyEditor->clear();
		DetailsButton->setChecked(false);
	}

	void github_pull_request_panel::refresh_native_appearance()
	{
		// Derived from the inherited font rather than a stored one, so a font
		// change on the composing window reaches these labels without the panel
		// having to be told about it.
		auto titleFont = font();
		titleFont.setWeight(QFont::DemiBold);
		if (titleFont.pointSizeF() > 0.0) {
			titleFont.setPointSizeF(titleFont.pointSizeF() * 1.35);
		}
		TitleLabel->setFont(titleFont);

		auto emphasisFont = font();
		emphasisFont.setWeight(QFont::DemiBold);
		IdentityLabel->setFont(emphasisFont);
		StateLabel->setFont(emphasisFont);
	}

	auto github_pull_request_panel::body_editor() const noexcept
		-> QPlainTextEdit*
	{
		return BodyEditor;
	}

	auto github_pull_request_panel::technical_details_button() const noexcept
		-> QToolButton*
	{
		return DetailsButton;
	}

	auto github_pull_request_panel::technical_details() const noexcept -> QWidget*
	{
		return TechnicalDetails;
	}
} // namespace bpr::qt
