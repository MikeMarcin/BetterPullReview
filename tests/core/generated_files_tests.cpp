// RFC-BPR-0089 M2: generated decisions and their precedence, under core-only
// (AC04's decisions, AC08). The precedence tests use exact-path rules; the
// head's `.gitattributes`, through the first-party reader, have their own
// (BPR-1692).
#include "bpr/core/generated_files.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(1);
		}
	}

	/// \brief Attribute answers for exact paths; every other path is unspecified.
	class exact_path_rules final : public core::generated_attribute_rules
	{
	public:
		explicit exact_path_rules(std::map<std::string, core::git_attribute_state, std::less<>> aStates)
			: States{ std::move(aStates) }
		{
		}

		[[nodiscard]] auto linguist_generated(const std::string_view aPath) const -> core::git_attribute_state override
		{
			++Asked;
			const auto found = States.find(aPath);
			return found != States.end() ? found->second : core::git_attribute_state{ };
		}

		mutable std::size_t Asked = 0U;

	private:
		std::map<std::string, core::git_attribute_state, std::less<>> States;
	};

	auto set() -> core::git_attribute_state
	{
		return { .Kind = core::git_attribute_state::kind::set, .Value = { } };
	}

	auto unset() -> core::git_attribute_state
	{
		return { .Kind = core::git_attribute_state::kind::unset, .Value = { } };
	}

	auto value(std::string aValue) -> core::git_attribute_state
	{
		return { .Kind = core::git_attribute_state::kind::value, .Value = std::move(aValue) };
	}

	auto marks_after(core::generated_file_marks aMarks, const std::string_view aPath, const core::generated_gesture aGesture)
		-> core::generated_file_marks
	{
		auto next = core::apply_generated_gesture(aMarks, core::file_identity_of(aPath), aGesture);
		expect(next.has_value(), "a gesture on a valid file is accepted");
		expect(core::valid(*next), "a gesture leaves valid marks");
		return *next;
	}

	void test_attribute_states_decide_without_marks()
	{
		const auto none = core::decide_generated(nullptr, { });
		expect(!none.Generated && !none.SetAside && none.Reason == core::generated_reason::unspecified, "no rule: an ordinary file");
		const auto marked = core::decide_generated(nullptr, set());
		expect(marked.Generated && marked.SetAside && marked.Reason == core::generated_reason::attribute_set, "linguist-generated sets a file aside");
		const auto truthy = core::decide_generated(nullptr, value("true"));
		expect(truthy.Generated && truthy.Reason == core::generated_reason::attribute_set, "linguist-generated=true is set");
		const auto other = core::decide_generated(nullptr, value("yes"));
		expect(other.Generated, "any value other than false is generated, as Linguist reads it");
		const auto unsetState = core::decide_generated(nullptr, unset());
		expect(!unsetState.Generated && unsetState.Reason == core::generated_reason::attribute_unset, "-linguist-generated is normal");
		const auto falseState = core::decide_generated(nullptr, value("false"));
		expect(!falseState.Generated && falseState.Reason == core::generated_reason::attribute_false, "linguist-generated=false is normal");
	}

	void test_marks_outrank_attributes()
	{
		const auto file   = core::file_identity_of("gen/out.c");
		auto       normal = core::generated_file_mark{ .File = file, .Mark = core::generated_mark::show_as_normal, .ReviewAnyway = false };
		auto       forced = core::generated_file_mark{ .File = file, .Mark = core::generated_mark::treat_as_generated, .ReviewAnyway = false };
		const auto a      = core::decide_generated(&normal, set());
		expect(!a.Generated && a.Reason == core::generated_reason::marked_normal, "Show as normal outranks linguist-generated");
		const auto b = core::decide_generated(&forced, unset());
		expect(b.Generated && b.SetAside && b.Reason == core::generated_reason::marked_generated, "Treat as generated outranks -linguist-generated");
		const auto c = core::decide_generated(&forced, { });
		expect(c.Generated && c.Reason == core::generated_reason::marked_generated, "Treat as generated marks a file no attribute names");

		auto       anyway = core::generated_file_mark{ .File = file, .Mark = core::generated_mark::none, .ReviewAnyway = true };
		const auto d      = core::decide_generated(&anyway, set());
		expect(d.Generated && d.ReviewAnyway && !d.SetAside && d.Reason == core::generated_reason::attribute_set,
			   "Review anyway returns a generated file to the review and it stays generated");
		const auto e = core::decide_generated(&anyway, { });
		expect(!e.Generated && !e.ReviewAnyway && !e.SetAside, "Review anyway on a file that is not generated changes nothing");
	}

	void test_gestures_normalize_and_round_trip()
	{
		auto marks = core::generated_file_marks{ };
		marks      = marks_after(marks, "b.lock", core::generated_gesture::treat_as_generated);
		marks      = marks_after(marks, "a.min.js", core::generated_gesture::treat_as_generated);
		expect(marks.Marks.size() == 2U, "two marks are kept");
		expect(core::find_generated_mark(marks, core::file_identity_of("a.min.js")) != nullptr, "a mark is found by file");

		marks            = marks_after(marks, "a.min.js", core::generated_gesture::review_anyway);
		const auto* kept = core::find_generated_mark(marks, core::file_identity_of("a.min.js"));
		expect(kept != nullptr && kept->Mark == core::generated_mark::treat_as_generated && kept->ReviewAnyway, "Review anyway keeps the mark");
		marks = marks_after(marks, "a.min.js", core::generated_gesture::set_aside_again);
		kept  = core::find_generated_mark(marks, core::file_identity_of("a.min.js"));
		expect(kept != nullptr && !kept->ReviewAnyway, "Set aside again ends Review anyway");

		marks = marks_after(marks, "a.min.js", core::generated_gesture::follow_attributes);
		expect(core::find_generated_mark(marks, core::file_identity_of("a.min.js")) == nullptr, "Follow .gitattributes forgets the mark");
		const auto same = marks_after(marks, "a.min.js", core::generated_gesture::set_aside_again);
		expect(same == marks, "a gesture that changes nothing returns the marks unchanged");

		marks = marks_after(marks, "b.lock", core::generated_gesture::show_as_normal);
		kept  = core::find_generated_mark(marks, core::file_identity_of("b.lock"));
		expect(kept != nullptr && kept->Mark == core::generated_mark::show_as_normal, "Show as normal replaces Treat as generated");

		auto anyway = marks_after({ }, "x/gen.c", core::generated_gesture::review_anyway);
		anyway      = marks_after(anyway, "x/gen.c", core::generated_gesture::show_as_normal);
		kept        = core::find_generated_mark(anyway, core::file_identity_of("x/gen.c"));
		expect(kept != nullptr && kept->Mark == core::generated_mark::show_as_normal && !kept->ReviewAnyway,
			   "Show as normal ends Review anyway: the reviewer said what the file is");

		const auto invalid = core::apply_generated_gesture({ }, core::file_identity{ }, core::generated_gesture::treat_as_generated);
		expect(!invalid && invalid.error() == core::generated_mark_refusal::invalid_file, "an invalid file is refused");
		auto unsorted = core::generated_file_marks{ .Marks = { *core::find_generated_mark(marks, core::file_identity_of("b.lock")),
															   *core::find_generated_mark(marks, core::file_identity_of("b.lock")) } };
		expect(!core::valid(unsorted), "repeated marks are not valid");
		const auto refused = core::apply_generated_gesture(unsorted, core::file_identity_of("c"), core::generated_gesture::treat_as_generated);
		expect(!refused && refused.error() == core::generated_mark_refusal::invalid_state, "malformed marks are refused");
		auto silent = core::generated_file_marks{ .Marks = { { .File = core::file_identity_of("d"), .Mark = core::generated_mark::none, .ReviewAnyway = false } } };
		expect(!core::valid(silent), "a mark that says nothing is not kept");
	}

	void test_gesture_availability()
	{
		const auto ordinary = core::decide_generated(nullptr, { });
		expect(core::generated_gesture_available(ordinary, nullptr, core::generated_gesture::treat_as_generated), "an ordinary file offers Treat as generated");
		expect(!core::generated_gesture_available(ordinary, nullptr, core::generated_gesture::show_as_normal), "an ordinary file offers no Show as normal");
		expect(!core::generated_gesture_available(ordinary, nullptr, core::generated_gesture::review_anyway), "an ordinary file offers no Review anyway");
		expect(!core::generated_gesture_available(ordinary, nullptr, core::generated_gesture::follow_attributes), "an unmarked file offers no Follow .gitattributes");

		const auto aside = core::decide_generated(nullptr, set());
		expect(core::generated_gesture_available(aside, nullptr, core::generated_gesture::review_anyway), "a set-aside file offers Review anyway");
		expect(core::generated_gesture_available(aside, nullptr, core::generated_gesture::show_as_normal), "a set-aside file offers Show as normal");
		expect(!core::generated_gesture_available(aside, nullptr, core::generated_gesture::set_aside_again), "a set-aside file offers no Set aside again");

		const auto anywayMark = core::generated_file_mark{ .File = core::file_identity_of("g"), .Mark = core::generated_mark::none, .ReviewAnyway = true };
		const auto anyway     = core::decide_generated(&anywayMark, set());
		expect(core::generated_gesture_available(anyway, &anywayMark, core::generated_gesture::set_aside_again), "a file reviewed anyway offers Set aside again");
		expect(!core::generated_gesture_available(anyway, &anywayMark, core::generated_gesture::review_anyway), "a file reviewed anyway offers no Review anyway");

		const auto normalMark = core::generated_file_mark{ .File = core::file_identity_of("g"), .Mark = core::generated_mark::show_as_normal, .ReviewAnyway = false };
		const auto normal     = core::decide_generated(&normalMark, set());
		expect(core::generated_gesture_available(normal, &normalMark, core::generated_gesture::follow_attributes), "a marked file offers Follow .gitattributes");
		expect(core::generated_gesture_available(normal, &normalMark, core::generated_gesture::treat_as_generated), "a file shown as normal offers Treat as generated");
	}

	void test_review_decisions_follow_paths_marks_and_rules()
	{
		// The owner's shape: C2 regenerates gen/**; C3 also edits one.
		const auto paths = std::vector<std::string>{ "src/generator.py", "gen/a.c", "gen/b.c", "gen/keep.c", "package-lock.json", "web/app.min.js" };
		const auto rules = exact_path_rules{ {
			{ "gen/a.c", set() },
			{ "gen/b.c", set() },
			{ "gen/keep.c", unset() },
		} };

		const auto none = core::decide_generated_files(paths, { }, nullptr);
		expect(none.size() == paths.size(), "one decision per path");
		expect(std::ranges::none_of(none, [](const core::generated_decision& aDecision) { return aDecision.Generated; }),
			   "nothing is hidden before a rule or gesture says so");

		const auto byRules = core::decide_generated_files(paths, { }, &rules);
		expect(rules.Asked == paths.size(), "the rules are asked once per path");
		expect(!byRules[0].Generated && byRules[1].SetAside && byRules[2].SetAside, "linguist-generated paths are set aside");
		expect(!byRules[3].Generated && byRules[3].Reason == core::generated_reason::attribute_unset, "-linguist-generated returns a path to normal");
		expect(!byRules[4].Generated && !byRules[5].Generated, "without attributes a lockfile and a minified file stay normal (owner, Q4)");

		auto marks        = core::generated_file_marks{ };
		marks             = marks_after(marks, "package-lock.json", core::generated_gesture::treat_as_generated);
		marks             = marks_after(marks, "gen/a.c", core::generated_gesture::show_as_normal);
		marks             = marks_after(marks, "gen/b.c", core::generated_gesture::review_anyway);
		const auto marked = core::decide_generated_files(paths, marks, &rules);
		expect(marked[4].SetAside && marked[4].Reason == core::generated_reason::marked_generated, "Treat as generated sets a lockfile aside");
		expect(!marked[1].Generated && marked[1].Reason == core::generated_reason::marked_normal, "Show as normal returns a marked path");
		expect(marked[2].Generated && marked[2].ReviewAnyway && !marked[2].SetAside, "Review anyway returns a generated path, still generated");
		const auto again = core::decide_generated_files(paths, marks, &rules);
		expect(again == marked, "equal inputs decide equally");

		auto malformed = marks;
		std::ranges::reverse(malformed.Marks);
		const auto ignored = core::decide_generated_files(paths, malformed, nullptr);
		expect(std::ranges::none_of(ignored, [](const core::generated_decision& aDecision) { return aDecision.Generated; }),
			   "malformed marks decide nothing");
	}

	void test_attribute_directories()
	{
		const auto paths       = std::vector<std::string>{ "a/b/c.txt", "a/d.txt", "root.txt", "a-b/e.txt", "x/y/z/w.txt" };
		const auto directories = core::generated_attribute_directories(paths);
		const auto expected    = std::vector<std::string>{ "", "a", "a-b", "a/b", "x", "x/y", "x/y/z" };
		expect(directories == expected, "the root and every ancestor, each once, parents first");
		expect(core::generated_attribute_directories({ }) == std::vector<std::string>{ std::string{ } }, "the root is always read");
	}

	/// \brief BPR-1692. The head's `.gitattributes` through the first-party
	/// reader: patterns, nesting, macros and the three ways to say no. The
	/// same corpus is checked against `git check-attr` by the RFC-BPR-0083
	/// harness (`git_metadata_harness`, case `linguist-generated`).
	auto head_files() -> std::vector<core::generated_attribute_file>
	{
		return {
			{ .Directory = "",
			  .Contents  = "[attr]generated linguist-generated\n"
						   "gen/** linguist-generated\n"
						   "*.min.js linguist-generated=true\n"
						   "/anchored.gen linguist-generated\n"
						   "build/ linguist-generated\n"
						   "vendor/*.js generated\n"
						   "docs/** -linguist-generated\n"
						   "keep.json linguist-generated=false\n"
						   "\"quoted name.txt\" linguist-generated\n" },
			{ .Directory = "gen",
			  .Contents  = "item_007.txt -linguist-generated\n"
						   "special.txt !linguist-generated\n"
						   "[attr]local linguist-generated\n"
						   "*.loc local\n" },
			{ .Directory = "nested", .Contents = "f.txt linguist-generated\n" },
		};
	}

	void test_head_attribute_rules()
	{
		using kind       = core::git_attribute_state::kind;
		const auto files = head_files();
		const auto rules = core::read_generated_attribute_rules(files);
		expect(rules.has_value() && *rules != nullptr, "the head's files are read");
		const auto state = [&](const std::string_view aPath) { return (*rules)->linguist_generated(aPath); };
		expect(state("gen/item_000.txt").Kind == kind::set, "gen/** marks a file below it");
		expect(state("gen/sub/deep.txt").Kind == kind::set, "** crosses directories");
		expect(state("gen/item_007.txt").Kind == kind::unset, "a deeper file's -linguist-generated outranks its parent");
		expect(state("gen/special.txt").Kind == kind::unspecified, "!linguist-generated stops the lookup");
		expect(state("gen/x.loc").Kind == kind::set, "a nested file defines no macro; the parent's rule still applies");
		expect(state("a/b.min.js") == value("true"), "a basename pattern matches at any depth");
		expect(state("anchored.gen").Kind == kind::set && state("sub/anchored.gen").Kind == kind::unspecified, "a leading slash anchors");
		expect(state("build/out.txt").Kind == kind::unspecified, "a directory pattern never marks a file below it");
		expect(state("vendor/lib.js").Kind == kind::set, "a root macro expands");
		expect(state("vendor/deep/lib.js").Kind == kind::unspecified, "a star does not cross a slash");
		expect(state("docs/a/b.md").Kind == kind::unset, "-linguist-generated");
		expect(state("keep.json") == value("false"), "linguist-generated=false");
		expect(state("quoted name.txt").Kind == kind::set, "a quoted pattern");
		expect(state("nested/f.txt").Kind == kind::set && state("f.txt").Kind == kind::unspecified, "a nested file applies below itself only");
		expect(state("GEN/item_001.txt").Kind == kind::unspecified, "matching is case-sensitive");
		expect(state("src/app.cpp").Kind == kind::unspecified, "an unmatched path is unspecified");

		const auto paths     = std::vector<std::string>{ "gen/item_000.txt", "gen/item_007.txt", "keep.json", "src/app.cpp" };
		const auto decisions = core::decide_generated_files(paths, { }, rules->get());
		expect(decisions[0].SetAside && decisions[0].Reason == core::generated_reason::attribute_set, "a marked path is set aside");
		expect(!decisions[1].Generated && decisions[1].Reason == core::generated_reason::attribute_unset, "-linguist-generated returns a path to normal");
		expect(!decisions[2].Generated && decisions[2].Reason == core::generated_reason::attribute_false, "=false is normal");
		expect(!decisions[3].Generated && decisions[3].Reason == core::generated_reason::unspecified, "an ordinary file");
		const auto again = core::read_generated_attribute_rules(files);
		expect(core::decide_generated_files(paths, { }, again->get()) == decisions, "equal files decide equally");
	}

	void test_head_attribute_bounds()
	{
		using refusal = core::generated_attributes_refusal;
		expect(core::read_generated_attribute_rules({ }).has_value(), "no file is no rule");
		auto many = std::vector<core::generated_attribute_file>{ { .Directory = "", .Contents = "*.gen linguist-generated\n" } };
		for (std::size_t index = 1U; index < core::maximum_generated_attribute_files; ++index) {
			many.push_back({ .Directory = "d" + std::to_string(1000U + index), .Contents = "*.gen linguist-generated\n" });
		}
		expect(core::read_generated_attribute_rules(many).has_value(), "64 files are read");
		many.push_back({ .Directory = "z", .Contents = { } });
		expect(core::read_generated_attribute_rules(many) == std::unexpected{ refusal::too_many_files }, "a 65th file refuses the rules");

		auto large = std::vector<core::generated_attribute_file>{ { .Directory = "", .Contents = std::string(core::maximum_generated_attribute_file_bytes, '#') } };
		expect(core::read_generated_attribute_rules(large).has_value(), "512 KiB is read");
		large.front().Contents.push_back('\n');
		expect(core::read_generated_attribute_rules(large) == std::unexpected{ refusal::file_too_large }, "one byte more refuses the rules");

		const auto refused = [](std::vector<core::generated_attribute_file> aFiles) {
			return core::read_generated_attribute_rules(aFiles) == std::unexpected{ refusal::invalid_files };
		};
		expect(refused({ { .Directory = "b", .Contents = { } }, { .Directory = "a", .Contents = { } } }), "parents and siblings come in order");
		expect(refused({ { .Directory = "a", .Contents = { } }, { .Directory = "a", .Contents = { } } }), "one file per directory");
		for (const auto* directory : { "/a", "a/", "a//b", "a/../b", "./a", ".." }) {
			expect(refused({ { .Directory = directory, .Contents = { } } }), "a directory is a normalized repository path");
		}
	}
} // namespace

auto main() -> int
{
	test_attribute_states_decide_without_marks();
	test_marks_outrank_attributes();
	test_gestures_normalize_and_round_trip();
	test_gesture_availability();
	test_review_decisions_follow_paths_marks_and_rules();
	test_attribute_directories();
	test_head_attribute_rules();
	test_head_attribute_bounds();
	std::cout << "generated files: all passed\n";
	return 0;
}
