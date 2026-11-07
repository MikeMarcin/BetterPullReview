#include "bpr/application/tour_writing_style.hpp"

namespace bpr::application {
	auto tour_writing_style() noexcept -> std::string_view
	{
		return "Style: every text you write is GitHub-flavoured Markdown, shown rendered in the app and on GitHub. Write plainly and precisely, so a reviewer can scan it:\n"
			   "- Short paragraphs of one to three sentences, separated by a blank line. Never one long block.\n"
			   "- A bullet list (\"- \") for two or more parallel points, cases or steps; a numbered list only when order matters.\n"
			   "- Inline code (`like_this`) for identifiers, functions, types, files, paths, options and literal values.\n"
			   "- A fenced code block only for an excerpt of at most six lines that the prose needs.\n"
			   "- No headings, tables, HTML or images. Bold at most one key term.\n"
			   "- Keep the lengths asked for; formatting is not extra words.\n"
			   "Inside a JSON string, write each line break as \\n and a blank line as \\n\\n.\n";
	}
}
