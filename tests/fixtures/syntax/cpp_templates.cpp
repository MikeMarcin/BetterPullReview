// clang-format off: the contract test finds its lines by their exact text,
// and the comparison controls depend on their spacing.
// BPR-1559: template function calls, template arguments and template types.
// Each line under test carries a marker in its trailing comment; the words a
// test pins come before the comment.
#include <map>
#include <memory>
#include <vector>

namespace engine {
	template<typename Component>
	Component* getComponent(); // T00 namespace-scope declaration
}

struct Entity
{
	template<typename T> T getComponent(); // T01 member declaration
	template<typename T, int N> T* get(); // T02 non-type parameter
};

template<typename T>
T Entity::getComponent() // T03 out-of-line definition
{
	return T{ };
}

template<>
Transform Entity::getComponent<Transform>() // T04 explicit specialization
{
	return { };
}

void calls(Entity& entity, Entity* pointer)
{
	auto a = getComponent<Type>(); // T10 free call
	auto b = entity.getComponent<Type>(); // T11 member call through a dot
	auto c = pointer->getComponent<Type>(); // T12 member call through an arrow
	auto d = engine::getComponent<Type>(); // T13 qualified call
	auto e = pointer->template get<Node>(); // T14 template disambiguator
	auto f = make<int>(); // T15 primitive argument
	auto g = make<ns::Kind>(); // T16 qualified argument
	auto h = make<Outer<Inner<A, B>>>(); // T17 nested arguments
	auto i = Factory<Widget>::create(); // T18 static member call
	auto j = make<Widget*, const Gadget&>(); // T19 pointer and reference arguments
	auto k = make<Shape, 3, MAX_SIZE>(); // T20 non-type arguments
	auto l = std::make_unique<Widget>(); // T21 standard library call
	auto m = static_cast<Base*>(pointer); // T22 cast
	auto n = std::function<Result(Argument)>{ }; // T23 function type
}

Foo<Bar> x; // T30 instantiation type
std::vector<std::pair<Key, Value>> pairs; // T31 nested standard types
std::map<Key, std::vector<Value>> table; // T32 mixed nesting
object_pool<Element> pool; // T33 lower-case template type
Registry<Plugin>& registry(); // T34 reference to an instance

auto comparisons(int a, int b, int c, int d, int n) -> bool
{
	auto first = a < b > c; // N01 spaced comparison
	if (a < b && c > d) { } // N02 spaced logical comparison
	for (int i=0; i<n; ++i) { } // N03 tight loop condition
	auto second = a<b && c>d; // N04 tight logical comparison
	auto third = (a<b) + (c>d); // N05 parenthesised comparisons
	auto fourth = a<b || c>d; // N06 tight disjunction
	auto shifted = a<<b; // N07 shift
	auto lesser = a<=b; // N08 less or equal
	return first; // N09
}
// clang-format on
