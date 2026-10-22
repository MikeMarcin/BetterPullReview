// clang-format off: the contract test finds its lines by their exact text.
#include <vector>
#define STD_VECTOR std::vector<int>
namespace foo::bar { void baz(); } // N01
namespace fs = std::filesystem; // N02
namespace net = vendor::network; // N03
struct Widget { static int count(); enum Mode { Fast }; };
using namespace std; // N04
using namespace std::chrono_literals; // N05
using namespace outer::inner; // N06
std::vector<int> values; // N07
auto found = std::ranges::find(values, 1); // N08
::std::size_t size = 0; // N09
auto seconds = std::chrono::seconds{ 1 }; // N10
auto npos = std::string::npos; // N11
void call() { foo::bar::baz(); } // N12
auto align = Qt::AlignLeft; // N13
auto counted = Widget::count(); // N14
auto mode = Widget::Mode::Fast; // N15
auto target = vendor::network::connect(); // N16
auto nested = outer::inner::make(); // N17
auto path = fs::path{ }; // N18
auto other = mystd::thing(); // N19
auto limits = std::numeric_limits<int>::max(); // N20
namespace lib { struct std { static int x; }; auto local = std::x; } // N21
auto load(const fs::path& aPath) -> std::vector<std::string>; // N24
auto lambda = [](int aValue) -> std::string { return { }; }; // N25
auto bare = [] -> std::size_t { return 0U; }; // N26
template <typename T> concept Sized = requires(T aValue) { { aValue.size() } -> std::convertible_to<std::size_t>; }; // N27
auto what = pointer->std::exception::what(); // N28
auto message = error.std::exception::what(); // N29
auto read = pointer->field; // N30
auto member = pointer->Widget::count(); // N31
auto scoped = object.Widget::Mode::Fast; // N32
template <> struct std::hash<Widget> { auto operator()(const Widget& aValue) const -> std::size_t; }; // N33
struct foo::bar::Gadget { }; // N34
auto contexts = f(std::move(a), g<std::size_t>(b), static_cast<std::size_t>(c), decltype(std::declval<int>()){ }); // N35
template <typename T> requires std::integral<T> std::vector<T> keywords() noexcept(std::is_nothrow_swappable_v<T>) { if (x) { throw std::runtime_error{ "x" }; } co_return std::move(y); return std::move(y); } // N36
// std::vector in a comment N22
const char* text = "std::vector N23";
// clang-format on
