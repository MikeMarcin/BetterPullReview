#include "bpr/core/json_preflight.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using bpr::core::json_preflight;
	using bpr::core::json_preflight_limits;
	using bpr::core::json_preflight_result;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(1);
		}
	}

	auto check(const std::string_view aInput, const json_preflight_limits aLimits = { }) -> json_preflight_result
	{
		return json_preflight(aInput, aLimits).Result;
	}

	void structure()
	{
		expect(check(R"( {"a":[1,true,null,"x"],"b":{"c":-0.5e-3}} )") == json_preflight_result::valid, "one well-formed value");
		expect(check(R"({"a":1,"a":2})") == json_preflight_result::duplicate_key, "duplicate keys are refused");
		expect(check(R"({"\u0061":1})") == json_preflight_result::malformed, "escaped keys are refused");
		expect(check(R"({"a":1} {})") == json_preflight_result::malformed, "trailing content is refused");
		expect(check("[1,]") == json_preflight_result::malformed, "trailing comma is refused");
		expect(check("\"a\x01\"") == json_preflight_result::malformed, "a raw control character is refused");
		expect(check("\"\\u00zz\"") == json_preflight_result::malformed, "a bad unicode escape is refused");
		expect(check("[[[1]]]", { .Depth = 2U }) == json_preflight_result::structural_limit, "depth is a structural limit");
		expect(check("[1,2,3]", { .ArrayElements = 2U }) == json_preflight_result::structural_limit, "array elements are a structural limit");
		expect(check(R"({"a":1,"b":2})", { .ObjectMembers = 1U }) == json_preflight_result::structural_limit, "object members are a structural limit");
		expect(check(R"("abcdef")", { .StringBytes = 3U }) == json_preflight_result::structural_limit, "a long string is a structural limit, not malformed");
		expect(check("[1,2,3]", { .Values = 3U }) == json_preflight_result::structural_limit, "total values are a structural limit");
		expect(json_preflight("[1,[2,3]]", { }).Values == 5U, "every value is counted");
	}

	void numbers()
	{
		for (const auto text : { "0", "-0", "12", "1.5", "1e307", "9.99e307", "1E-999", "0.0000e999", "123456789012345678901234567890" }) {
			expect(check(text) == json_preflight_result::valid, std::string{ "valid number " } + text);
		}
		for (const auto text : { "01", "1.", ".5", "-", "1e", "+1", "1e308", "1e999", "17976931348623157e292", "1e100000000000000000000" }) {
			expect(check(text) == json_preflight_result::malformed, std::string{ "refused number " } + text);
		}
	}
}

int main()
{
	structure();
	numbers();
}
