/// \file
/// \brief RFC-BPR-0108 as amended (owner, 2026-11-13). A local comparison's
/// identity for the pull request window's own diff viewer: valid as the viewer
/// requires, equal for equal subjects, paths and bytes, apart for another
/// subject, path or content, and every comparison of one subject under one
/// authority and pair of anchors, so Find in files reaches it. No Qt.

#include "bpr/core/local_presentation.hpp"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}
}

auto main() -> int
{
	const auto options = core::diff_options{ };
	const auto key     = core::local_presentation_key("draft-a#candidate-1", "src/a.cpp", bytes("int a;\n"), bytes("int a = 1;\n"), options);
	expect(key.has_value() && core::valid(*key), "a valid comparison identity for the viewer");
	expect(key == core::local_presentation_key("draft-a#candidate-1", "src/a.cpp", bytes("int a;\n"), bytes("int a = 1;\n"), options), "equal inputs, equal identity");
	const auto other = core::local_presentation_key("draft-b#candidate-1", "src/a.cpp", bytes("int a;\n"), bytes("int a = 1;\n"), options);
	expect(other && other->Authority != key->Authority && other->File != key->File, "another subject is another authority and file");
	const auto sibling = core::local_presentation_key("draft-a#candidate-1", "src/b.cpp", std::nullopt, bytes("int b;\n"), options);
	expect(sibling && sibling->Authority == key->Authority && sibling->LeftAnchor == key->LeftAnchor && sibling->RightAnchor == key->RightAnchor && sibling->File != key->File && !sibling->LeftBlob,
		   "one subject's files share its authority and anchors; an added file has no left side");
	const auto identity = core::local_presentation_identity_of("draft-a#candidate-1");
	expect(identity.Authority == key->Authority && identity.Left == key->LeftAnchor && identity.Right == key->RightAnchor, "the identity search scopes use");
	const auto changed = core::local_presentation_key("draft-a#candidate-1", "src/a.cpp", bytes("int a;\n"), bytes("int a = 2;\n"), options);
	expect(changed && changed->RightBlob != key->RightBlob && changed->File == key->File, "other bytes, another blob; the file stays");
	expect(!core::local_presentation_key("draft-a#candidate-1", "src/a.cpp", std::nullopt, std::nullopt, options), "nothing on either side is no comparison");
	expect(core::local_presentation_file("draft-a", "src/a.cpp") == core::local_presentation_file("draft-a", "src/a.cpp")
			   && core::local_presentation_file("draft-a", "src/a.cpp") != core::local_presentation_file("draft-a", "src/b.cpp"),
		   "file identities follow the path");
	std::cout << "local-presentation-tests:pass\n";
	return EXIT_SUCCESS;
}
