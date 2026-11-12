#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_EVIDENCE_AREA_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_LOCAL_DRAFT_EVIDENCE_AREA_HPP

#include "bpr/application/local_draft_workbench.hpp"

#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QDialog;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace bpr::qt {
	/// \brief The gestures the Evidence area sends to its host.
	struct local_draft_evidence_dispatch
	{
		std::function<void(core::claim_kind, const std::string&)>                                                                       AddClaim;
		std::function<void(std::uint64_t, const std::string&)>                                                                          ReviseClaim;
		std::function<void(core::check_method, const std::string&, const std::string&, std::vector<std::uint64_t>, core::formal_scope)> AddCheck;
		std::function<void(core::manual_result)>                                                                                        RecordResult;
		std::function<void(const std::string&, std::uint64_t)>                                                                          AddQuestion;
		std::function<void(std::uint64_t, bool)>                                                                                        ResolveQuestion;
		std::function<void(std::uint64_t, core::evidence_disposition)>                                                                  Decide;
		std::function<void(std::uint64_t, bool)>                                                                                        Select;
		/// The host asks for the document and imports it.
		std::function<void()>                           Import;
		std::function<void(std::uint64_t, std::size_t)> OpenArtifact;
		/// A link artifact: the composition opens the address it recorded.
		std::function<void(std::uint64_t, std::size_t)> OpenLink;
		std::function<void()>                           CloseArtifact;
	};

	/// \brief What the author enters in each form. The default forms are
	/// dialogs; a course that cannot answer one replaces them.
	struct local_draft_evidence_forms
	{
		struct claim
		{
			core::claim_kind Kind = core::claim_kind::other;
			std::string      Text;
		};
		struct check
		{
			core::check_method         Method = core::check_method::test;
			std::string                Title;
			std::string                Command;
			std::vector<std::uint64_t> Claims;
			core::formal_scope         Formal;
		};
		struct question
		{
			std::string   Text;
			std::uint64_t Claim = 0;
		};
		std::function<std::optional<claim>(const application::local_draft_workbench_projection&)>               Claim;
		std::function<std::optional<std::string>(const core::evidence_claim&)>                                  Revision;
		std::function<std::optional<check>(const application::local_draft_workbench_projection&)>               Check;
		std::function<std::optional<core::manual_result>(const application::local_draft_workbench_projection&)> Result;
		std::function<std::optional<question>(const application::local_draft_workbench_projection&)>            Question;
	};

	/// \brief A local draft's Evidence (RFC-BPR-0109): claims, their checks
	/// and reported results, open questions, and what is selected for later
	/// description and tour use.
	/// \details A result is never shown as verified or observed: each says
	/// how it was reported, which candidate's material it speaks for and
	/// what its method can speak for. Imported claims, checks and questions
	/// are proposed until the author accepts or rejects them. It decides
	/// nothing and performs no effect: gestures leave through
	/// `local_draft_evidence_dispatch`.
	class local_draft_evidence_area final : public QWidget
	{
	public:
		explicit local_draft_evidence_area(QWidget* aParent = nullptr);

		void set_dispatch(local_draft_evidence_dispatch aDispatch);
		/// \brief Replaces the forms, for a course that cannot answer a dialog.
		void set_forms(local_draft_evidence_forms aForms);
		void present(const application::local_draft_workbench_projection& aProjection);

		[[nodiscard]] auto tree() const noexcept -> QTreeWidget*;
		[[nodiscard]] auto detail() const noexcept -> QPlainTextEdit*;
		[[nodiscard]] auto artifacts() const noexcept -> QListWidget*;
		[[nodiscard]] auto status() const noexcept -> QLabel*;
		[[nodiscard]] auto add_claim_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto add_check_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto record_result_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto add_question_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto import_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto accept_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto reject_button() const noexcept -> QPushButton*;
		/// \brief The tree row of one record, by identity.
		[[nodiscard]] auto row_of(std::uint64_t aRecord) const -> QTreeWidgetItem*;
		/// \brief The artifact reader, while one is shown.
		[[nodiscard]] auto artifact_reader() const noexcept -> QDialog*;

	private:
		void               rebuild();
		void               present_detail();
		void               present_status();
		void               present_artifact();
		void               update_buttons();
		[[nodiscard]] auto selected_record() const -> std::uint64_t;

		local_draft_evidence_dispatch                 Dispatch;
		local_draft_evidence_forms                    Forms;
		application::local_draft_workbench_projection Projection;
		bool                                          Presenting = false;

		QTreeWidget*    Tree         = nullptr;
		QPlainTextEdit* Detail       = nullptr;
		QListWidget*    Artifacts    = nullptr;
		QLabel*         Status       = nullptr;
		QPushButton*    AddClaim     = nullptr;
		QPushButton*    AddCheck     = nullptr;
		QPushButton*    RecordResult = nullptr;
		QPushButton*    AddQuestion  = nullptr;
		QPushButton*    Import       = nullptr;
		QPushButton*    Accept       = nullptr;
		QPushButton*    Reject       = nullptr;
		QDialog*        Reader       = nullptr;
		QPlainTextEdit* ReaderText   = nullptr;
	};
}

#endif
