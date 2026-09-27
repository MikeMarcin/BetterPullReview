#ifndef BPR_QT_SCOPED_PROSE_EDITOR_HPP
#define BPR_QT_SCOPED_PROSE_EDITOR_HPP
#include "bpr/core/review_preparation.hpp"
#include <QPlainTextEdit>
#include <map>
namespace bpr::qt {
	struct prose_capture
	{
		std::string             Raw;
		core::writing_selection Selection;
		std::uint64_t           TextGeneration = 0, EditorGeneration = 0;
	};
	/// Canonical Markdown and native QTextDocument history share one edit lane.
	class scoped_prose_editor final : public QPlainTextEdit
	{
	public:
		explicit scoped_prose_editor(QWidget* aParent = nullptr);
		~scoped_prose_editor() override;
		void present(std::string aRaw, bool aNewOwner = false);
		/// \brief Bind native history and invalidate captured work when the document changes.
		void                  bind_document(QTextDocument* aDocument);
		[[nodiscard]] auto    raw() const -> const std::string&;
		[[nodiscard]] auto    capture() const -> std::optional<prose_capture>;
		[[nodiscard]] auto    apply(const core::writing_edit& aEdit) -> bool;
		std::function<void()> Invalidated;

	protected:
		void inputMethodEvent(QInputMethodEvent* aEvent) override;

	private:
		using QPlainTextEdit::setDocument;
		struct rewrite_selection
		{
			int                     After = 0;
			core::writing_selection Selection;
		};
		struct document_state
		{
			std::string                      Raw;
			std::map<int, std::string>       History;
			std::map<int, rewrite_selection> Rewrites;
			int                              Steps      = 0;
			bool                             NewCommand = false;
		};
		void                                     attach();
		void                                     changed();
		std::map<QTextDocument*, document_state> Documents;
		std::optional<std::string>               Replacement;
		std::uint64_t                            TextGeneration = 1, EditorGeneration = 1, FocusRevision = 1;
		bool                                     Presenting = false;
	};
}
#endif
