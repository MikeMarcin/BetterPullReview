#ifndef BPR_TESTS_SUPPORT_CAPABILITY_ERROR_CONTENT_CANARIES_HPP
#define BPR_TESTS_SUPPORT_CAPABILITY_ERROR_CONTENT_CANARIES_HPP

#include <array>
#include <string_view>

namespace bpr::tests {

	// Synthetic content exercises both arbitrary prose and the shapes retained
	// for useful unknown errors. This does not classify identifiers as public.
	struct capability_error_content_canary
	{
		std::string_view Name;
		std::string_view Body;
		std::string_view Marker;
	};

	inline constexpr std::string_view capability_error_canary_credential = "synthetic-active-credential";
	inline constexpr std::array       capability_error_content_canaries{
		capability_error_content_canary{
			"message_prose",
			R"({"errors":[{"message":"private source canary alpha synthetic-active-credential"}]})",
			"private source canary alpha" },
		capability_error_content_canary{
			"type_prose",
			R"({"errors":[{"type":"private source canary beta"}]})",
			"private source canary beta" },
		capability_error_content_canary{
			"extension_type_prose",
			R"({"errors":[{"extensions":{"type":"private source canary gamma"}}]})",
			"private source canary gamma" },
		capability_error_content_canary{
			"extension_code_prose",
			R"({"errors":[{"extensions":{"code":"private source canary delta"}}]})",
			"private source canary delta" },
		capability_error_content_canary{
			"message_schema_symbol",
			R"({"errors":[{"message":"Unknown schema field 'privateField' is unavailable."}]})",
			"privateField" },
		capability_error_content_canary{
			"type_identifier",
			R"({"errors":[{"type":"PRIVATE_TYPE_CANARY"}]})",
			"PRIVATE_TYPE_CANARY" },
		capability_error_content_canary{
			"extension_type_identifier",
			R"({"errors":[{"extensions":{"type":"PRIVATE_EXT_TYPE_CANARY"}}]})",
			"PRIVATE_EXT_TYPE_CANARY" },
		capability_error_content_canary{
			"extension_code_identifier",
			R"({"errors":[{"extensions":{"code":"PRIVATE_CODE_CANARY"}}]})",
			"PRIVATE_CODE_CANARY" }
	};

} // namespace bpr::tests

#endif
