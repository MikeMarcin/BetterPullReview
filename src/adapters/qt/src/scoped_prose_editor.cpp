#include "bpr/qt/scoped_prose_editor.hpp"
#include <QInputMethodEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>
namespace bpr::qt {
	namespace {
		auto visible(std::string_view aRaw) -> QString
		{
			return QString::fromUtf8(aRaw.data(), static_cast<qsizetype>(aRaw.size())).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QChar{ '\r' }, QChar{ '\n' }).replace(QChar{ 0x2029 }, QChar{ '\n' });
		}
		// A native paragraph occupies one UTF-16 position; CRLF occupies two raw bytes.
		auto offset(std::string_view aRaw, int aPosition) -> std::optional<std::size_t>
		{
			if (aPosition < 0) {
				return std::nullopt;
			}
			int position = 0;
			for (std::size_t i = 0; i < aRaw.size();) {
				if (position == aPosition) {
					return i;
				}
				const auto c = static_cast<unsigned char>(aRaw[i]);
				if (c == '\r' && i + 1 < aRaw.size() && aRaw[i + 1] == '\n') {
					i += 2;
				} else {
					i += c < 0x80 ? 1 : c < 0xe0 ? 2
						: c < 0xf0               ? 3
												 : 4;
				}
				position += c >= 0xf0 ? 2 : 1;
			}
			return position == aPosition ? std::optional{ aRaw.size() } : std::nullopt;
		}
		auto position(std::string_view aRaw, std::size_t aOffset) -> int
		{
			return static_cast<int>(visible(aRaw.substr(0, aOffset)).size());
		}
		/// A caret keeps its place across a change made elsewhere: text before the changed span is
		/// unmoved, text after it shifts by the length change, and a caret inside it goes to its start.
		auto carried_position(const QString& aBefore, const QString& aAfter, int aPosition) -> int
		{
			const auto limit  = std::min(aBefore.size(), aAfter.size());
			auto       prefix = qsizetype{ 0 };
			while (prefix < limit && aBefore[prefix] == aAfter[prefix]) {
				++prefix;
			}
			auto suffix = qsizetype{ 0 };
			while (suffix < limit - prefix && aBefore[aBefore.size() - 1 - suffix] == aAfter[aAfter.size() - 1 - suffix]) {
				++suffix;
			}
			const auto position = static_cast<qsizetype>(aPosition);
			if (position <= prefix) {
				return static_cast<int>(position);
			}
			if (position >= aBefore.size() - suffix) {
				return static_cast<int>(std::clamp<qsizetype>(position + (aAfter.size() - aBefore.size()), 0, aAfter.size()));
			}
			return static_cast<int>(prefix);
		}
	}
	scoped_prose_editor::scoped_prose_editor(QWidget* aParent)
		: QPlainTextEdit{ aParent }
	{
		attach();
		connect(this, &QPlainTextEdit::textChanged, this, [this] { changed(); });
		connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this] { ++FocusRevision; });
		connect(this, &QPlainTextEdit::selectionChanged, this, [this] { ++FocusRevision; });
	}
	scoped_prose_editor::~scoped_prose_editor()
	{
		for (const auto& [doc, state] : Documents) {
			static_cast<void>(state);
			disconnect(doc, nullptr, this, nullptr);
		}
	}
	void scoped_prose_editor::attach()
	{
		auto* doc = document();
		if (Documents.contains(doc)) {
			return;
		}
		Documents[doc] = { };
		connect(doc, &QTextDocument::undoCommandAdded, this, [this, doc] { Documents.at(doc).NewCommand = true; });
		connect(doc, &QObject::destroyed, this, [this, doc] { Documents.erase(doc); });
	}
	void scoped_prose_editor::bind_document(QTextDocument* aDocument)
	{
		if (document() == aDocument) {
			return;
		}
		Presenting = true;
		const QSignalBlocker block{ this };
		QPlainTextEdit::setDocument(aDocument);
		attach();
		Presenting = false;
		++EditorGeneration;
		++TextGeneration;
		if (Invalidated) {
			Invalidated();
		}
	}
	auto scoped_prose_editor::raw() const -> const std::string& { return Documents.at(document()).Raw; }
	void scoped_prose_editor::present(std::string aRaw, bool aNewOwner)
	{
		auto& state = Documents.at(document());
		if (!aNewOwner && state.Raw == aRaw) {
			return;
		}
		Presenting = true;
		const QSignalBlocker block{ this };
		const auto           shown = document()->toRawText().replace(QChar{ 0x2029 }, QChar{ '\n' });
		if (shown != visible(aRaw)) {
			// Another presentation changed this text. Keep this author's caret, selection and scroll
			// where they were; never take focus, and never move a caret the author did not move.
			const auto prior  = textCursor();
			const auto anchor = prior.anchor(), caret = prior.position();
			const auto scroll = verticalScrollBar()->value();
			setPlainText(visible(aRaw));
			if (!aNewOwner) {
				const auto text = visible(aRaw);
				auto       live = textCursor();
				live.setPosition(std::min(carried_position(shown, text, anchor), document()->characterCount() - 1));
				live.setPosition(std::min(carried_position(shown, text, caret), document()->characterCount() - 1), QTextCursor::KeepAnchor);
				setTextCursor(live);
				verticalScrollBar()->setValue(scroll);
			}
		}
		document()->clearUndoRedoStacks();
		state            = { };
		state.Raw        = std::move(aRaw);
		state.History[0] = state.Raw;
		Presenting       = false;
		++TextGeneration;
		++EditorGeneration;
		if (Invalidated) {
			Invalidated();
		}
	}
	auto scoped_prose_editor::capture() const -> std::optional<prose_capture>
	{
		const auto cursor = textCursor();
		const auto begin = offset(raw(), cursor.selectionStart()), end = offset(raw(), cursor.selectionEnd());
		if (!begin || !end || !core::valid_writing_utf8(raw()) || document()->toRawText().replace(QChar{ 0x2029 }, QChar{ '\n' }) != visible(raw())) {
			return std::nullopt;
		}
		return prose_capture{ raw(), { *begin, *end, cursor.anchor(), cursor.position(), FocusRevision, cursor.hasSelection() ? 1U : 0U }, TextGeneration, EditorGeneration };
	}
	void scoped_prose_editor::changed()
	{
		if (Presenting) {
			return;
		}
		auto&      state    = Documents.at(document());
		const auto steps    = document()->availableUndoSteps();
		const auto now      = document()->toRawText().replace(QChar{ 0x2029 }, QChar{ '\n' });
		const auto oldSteps = state.Steps;
		const auto old      = visible(state.Raw);
		const auto history  = state.History.find(steps);
		const bool replay   = !state.NewCommand && steps != oldSteps && history != state.History.end() && visible(history->second) == now;
		if (Replacement) {
			state.Raw = *Replacement;
		} else if (replay) {
			state.Raw = history->second;
		} else {
			// Native insert/delete/IME gives normalized text. Retain untouched raw bytes.
			qsizetype first = 0, lastOld = old.size(), lastNew = now.size();
			while (first < lastOld && first < lastNew && old[first] == now[first]) {
				++first;
			}
			if (first > 0 && first < old.size() && old[first].isLowSurrogate()) {
				--first;
			}
			while (lastOld > first && lastNew > first && old[lastOld - 1] == now[lastNew - 1]) {
				--lastOld;
				--lastNew;
			}
			if (lastOld < old.size() && old[lastOld].isLowSurrogate()) {
				++lastOld;
				++lastNew;
			}
			const auto begin = offset(state.Raw, static_cast<int>(first)), end = offset(state.Raw, static_cast<int>(lastOld));
			if (begin && end) {
				state.Raw = state.Raw.substr(0, *begin) + now.mid(first, lastNew - first).toUtf8().toStdString() + state.Raw.substr(*end);
			} else {
				state.Raw = now.toUtf8().toStdString();
			}
		}
		if (!replay) {
			state.History.erase(state.History.upper_bound(oldSteps), state.History.end());
			for (auto it = state.Rewrites.begin(); it != state.Rewrites.end();) {
				if (it->second.After > oldSteps) {
					it = state.Rewrites.erase(it);
				} else {
					++it;
				}
			}
		}
		state.History[steps] = state.Raw;
		state.Steps          = steps;
		state.NewCommand     = false;
		++TextGeneration;
		if (replay && steps < oldSteps) {
			const auto rewrite = state.Rewrites.find(steps);
			if (rewrite != state.Rewrites.end() && rewrite->second.After == oldSteps) {
				const auto selection  = rewrite->second.Selection;
				const auto generation = TextGeneration, editor = EditorGeneration;
				QTimer::singleShot(0, this, [this, selection, generation, editor] {
					if (TextGeneration != generation || EditorGeneration != editor) {
						return;
					}
					auto cursor = textCursor();
					cursor.setPosition(selection.Anchor);
					cursor.setPosition(selection.Cursor, QTextCursor::KeepAnchor);
					setTextCursor(cursor);
				});
			}
		}
		if (Invalidated) {
			Invalidated();
		}
	}
	auto scoped_prose_editor::apply(const core::writing_edit& aEdit) -> bool
	{
		const auto& request = aEdit.Request;
		if (isReadOnly() || raw() != request.Raw || TextGeneration != request.Target.TextGeneration || EditorGeneration != request.Target.EditorGeneration || aEdit.Text == raw()) {
			return false;
		}
		const auto begin = position(raw(), request.Selection.Begin), end = position(raw(), request.Selection.End);
		auto&      state      = Documents.at(document());
		const auto before     = document()->availableUndoSteps();
		state.History[before] = state.Raw;
		auto       live       = textCursor();
		const auto liveAnchor = live.anchor(), livePosition = live.position();
		const bool unmoved = FocusRevision == request.Selection.FocusRevision;
		auto       cursor  = QTextCursor{ document() };
		cursor.setPosition(begin);
		cursor.setPosition(end, QTextCursor::KeepAnchor);
		Replacement = aEdit.Text;
		cursor.beginEditBlock();
		cursor.insertText(visible(aEdit.Replacement));
		const auto finish = cursor.position();
		if (finish > begin) {
			// A trailing format command keeps subsequent typing out of this native undo block.
			cursor.setPosition(begin, QTextCursor::KeepAnchor);
			cursor.setCharFormat(QTextCharFormat{ });
			cursor.setPosition(finish);
		}
		cursor.endEditBlock();
		Replacement.reset();
		state.Rewrites[before] = { document()->availableUndoSteps(), request.Selection };
		if (unmoved) {
			setTextCursor(cursor);
		} else {
			live.setPosition(std::min(liveAnchor, document()->characterCount() - 1));
			live.setPosition(std::min(livePosition, document()->characterCount() - 1), QTextCursor::KeepAnchor);
			setTextCursor(live);
		}
		return true;
	}
	void scoped_prose_editor::inputMethodEvent(QInputMethodEvent* aEvent)
	{
		++TextGeneration;
		if (Invalidated) {
			Invalidated();
		}
		QPlainTextEdit::inputMethodEvent(aEvent);
	}
}
