// clang-format off: RFC-BPR-0081 gap suite; layout is part of the input.
#include <meta>
namespace Reflect {
enum class Colour { Red, Green };
constexpr auto info = ^^Colour;
consteval auto count() { return std::meta::enumerators_of(^^Colour).size(); }
void walk() {
	template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Colour))) {
		using T = [: std::meta::type_of(e) :];
	}
}
}
auto red = Reflect::Colour::Red;
