#define ENUM_VALUE 17
#define LOOKALIKE                \
	namespace FAKE {             \
		enum class Fake { ID,    \
						  DNA }; \
	}
#define CONTINUED_LOOKALIKE     \
	namespace FALSE_NAMESPACE { \
		enum FalseEnum { ID };  \
	}
namespace GCT
	/* namespace/comment boundary */
	::Network {
	enum class Reason {
		Node   = 9,
		ID     = 1,
		Player = 64,
		DNA
	};
	enum Plain {
		FIRST  = ENUM_VALUE,
		SECOND = call(ENUM_VALUE, (1 + 2)),
		THIRD  = make<std::array<int, 2>, char>(),
		FOURTH = [] { return pair{ 1, 2 }; }(),
		FIFTH [[deprecated("enum Fake { ID }")]] = (1 << 2),
		LAST
	};
	auto chosen = Reason::ID;
	auto dna    = Reason::DNA;
	auto first  = Plain::FIRST;
	GCT_ASSERT(chosen);
}
namespace Alias = GCT::Network;
using namespace GCT
	/* safe split */ ::Network;
using namespace External::NAMESPACE;
namespace simple {
	enum Unscoped { Lower,
					UPPER };
}
namespace Other {
	enum class Reason { Different };
}
auto        qualified      = GCT::Network::Reason::DNA;
auto        lower          = simple::Unscoped::Lower;
auto        unrelated      = Other::Reason::DNA;
auto        unknown        = ExternalReason::ID;
auto        qualifier      = UPPER_SCOPE::Value;
auto        qualified_call = External::CALL_MACRO();
auto        ordinary_call  = External::function();
auto        standalone     = ENUM_VALUE;
auto        predefined     = __GNUC__;
const char* literal        = "namespace Bogus { enum class Reason { DNA }; }";
const char* raw            = R"cpp(
namespace RAW { enum class Reason { ID, DNA }; }
)cpp";
/* enum class Comment { ID, DNA }; namespace COMMENT { */
auto after_comment = Unknown::DNA;
namespace GCT::Network {
	auto reopened = Reason::Player;
}
namespace Shadow {
	enum class Reason { ID };
	void block()
	{
		auto Reason = unknown;
		auto hidden = Reason::ID;
	}
	void parameter(int Reason)
	{
		auto hidden_parameter = Reason::ID;
	}
	auto visible       = Reason::ID;
	using AliasReason  = Reason;
	auto alias_unknown = AliasReason::ID;
}
namespace Conflict {
	enum class Same { ID };
	struct Same
	{
		int ID;
	};
	auto ambiguous = Same::ID;
}
namespace Duplicate {
	enum class Repeated { ID,
						  ID };
	auto duplicate_member = Repeated::ID;
}
auto alias_qualified  = Alias::Reason::ID;
auto template_unknown = External<int>::ID;
auto object_qualified = object.GCT::Network::Reason::ID;
namespace Shadow {
	enum class Nested { ID };
	enum Lambda { VALUE = ([] { int Nested; return Nested::ID; }()),
				  NEXT };
}
auto after = AFTER_ENUM;
