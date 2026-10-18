#ifndef BPR_TESTS_GITHUB_CONTRACT_FIXTURE_JSON_HPP
#define BPR_TESTS_GITHUB_CONTRACT_FIXTURE_JSON_HPP

#include <glaze/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::tests {
	// For constructing valid test documents and injecting semantic faults.
	// Raw malformed syntax and precision-boundary tokens must bypass this helper.
	using fixture_value  = glz::generic_sorted_u64;
	using fixture_object = fixture_value::object_t;
	using fixture_array  = fixture_value::array_t;
	// [[BPR-1262]]. Courses copy a finished value into an array (emplace_back of an lvalue) rather than
	// move it or push a temporary. In the Release graph GCC 15 at -O3 read
	// the moved variant's alternatives as uninitialized
	// (github_paged_read_tests, github_capability_probe_tests).

	struct fixture_json_options : glz::opts
	{
		bool escape_control_characters    = true; // NOLINT(readability-identifier-naming): Glaze option names.
		bool validate_skipped             = true; // NOLINT(readability-identifier-naming): Glaze option names.
		bool validate_trailing_whitespace = true; // NOLINT(readability-identifier-naming): Glaze option names.
		constexpr fixture_json_options()
		{
			null_terminated   = false;
			skip_null_members = false;
		}
	};

	[[nodiscard]] inline auto parse_fixture_object(const std::string_view aBody) -> fixture_object
	{
		fixture_value value;
		if (glz::read<fixture_json_options{ }>(value, aBody) || !value.is_object()) {
			throw std::runtime_error{ "fixture JSON object is malformed" };
		}
		return std::move(value.get_object());
	}

	template <typename Value>
	[[nodiscard]] auto fixture_json(const Value& aValue) -> std::string
	{
		std::string body;
		if (glz::write<fixture_json_options{ }>(aValue, body)) {
			throw std::runtime_error{ "fixture JSON cannot be serialized" };
		}
		return body;
	}
}

#endif
