#include "linux_secret_service.hpp"

#include <libsecret/secret.h>

#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <utility>

// This file's identifier spellings are pinned, not merely conventional.
// cmake/VerifyArchitectureBoundaries.cmake requires the Linux Secret Service
// adapter to retain an exact, binary, noninteractive credential lifecycle, and
// it enforces that by matching source text -- including parameter names such
// as `prompt`, `actual_name` and `canonical_service_bus_name`. Renaming them to
// the project's `aPrompt` convention leaves the behaviour identical and fails
// `architecture_boundaries`, because the pinned patterns no longer match.
//
// The naming findings here are therefore accepted rather than resolved, per
// BPR-0443. Anything that is not an identifier name is still checked; if a
// pattern in that file is ever relaxed, drop the matching part of this
// suppression rather than widening it.
// NOLINTBEGIN(readability-identifier-naming)
namespace bpr::adapters::github {
	namespace {
		constexpr std::string_view production_schema_identifier        = "org.betterpullreview.github-native-credential.secret-service.v2";
		constexpr std::string_view integration_test_schema_identifier  = "org.betterpullreview.github-native-credential."
																		 "secret-service.integration-test.v2";
		constexpr std::string_view authority_attribute                 = "authority";
		constexpr std::string_view schema_attribute                    = "xdg:schema";
		constexpr std::string_view binary_content_type                 = "application/octet-stream";
		constexpr std::string_view gnome_keyring_content_type          = "text/plain";
		constexpr std::string_view production_label                    = "BetterPullReview GitHub native credential";
		constexpr std::string_view integration_test_label              = "BetterPullReview GitHub native credential integration test";
		constexpr std::string_view canonical_service_bus_name          = "org.freedesktop.secrets";
		constexpr gint             prompt_dismiss_timeout_milliseconds = 1'000;

		static_assert(linux_secret_service_authority_account_bytes == 574U);

		template <typename... Callables>
		struct overloaded : Callables...
		{
			using Callables::operator()...;
		};

		template <typename... Callables>
		overloaded(Callables...) -> overloaded<Callables...>;

		[[nodiscard]] constexpr auto hex_digit(const std::uint8_t value) noexcept
			-> char
		{
			constexpr std::array<char, 16U> digits{
				'0',
				'1',
				'2',
				'3',
				'4',
				'5',
				'6',
				'7',
				'8',
				'9',
				'a',
				'b',
				'c',
				'd',
				'e',
				'f',
			};
			return digits[value];
		}

		void append_hex_u64(linux_secret_service_authority_account& output,
							std::size_t&                            offset,
							const std::uint64_t                     value) noexcept
		{
			for (std::size_t nibble = 0U; nibble < 16U; ++nibble) {
				const auto shift = static_cast<unsigned int>((15U - nibble) * 4U);
				const auto digit = static_cast<std::uint8_t>((value >> shift) & 0xFU);
				output[offset]   = hex_digit(digit);
				++offset;
			}
		}

		void append_hex_u8(linux_secret_service_authority_account& output,
						   std::size_t&                            offset,
						   const std::uint8_t                      value) noexcept
		{
			output[offset] = hex_digit(static_cast<std::uint8_t>(value >> 4U));
			++offset;
			output[offset] = hex_digit(static_cast<std::uint8_t>(value & 0xFU));
			++offset;
		}

		[[nodiscard]] constexpr auto server_contract_discriminator(
			const application::github_server_contract contract) noexcept -> std::uint8_t
		{
			switch (contract) {
			case application::github_server_contract::github_com_2022_11_28:
				return 0x01U;
			case application::github_server_contract::ghes_3_18_2022_11_28:
				return 0x02U;
			}
			return 0x00U;
		}

		[[nodiscard]] constexpr auto normalize_failure(
			const application::github_credential_vault_failure failure) noexcept
			-> application::github_credential_vault_failure
		{
			return application::valid(failure)
				? failure
				: application::github_credential_vault_failure::operation_failed;
		}

		class error_owner final
		{
		public:
			error_owner() = default;

			~error_owner() noexcept
			{
				if (Value != nullptr) {
					g_error_free(Value);
				}
			}

			error_owner(const error_owner&)                    = delete;
			auto operator=(const error_owner&) -> error_owner& = delete;
			error_owner(error_owner&&)                         = delete;
			auto operator=(error_owner&&) -> error_owner&      = delete;

			[[nodiscard]] auto out() noexcept -> GError**
			{
				return &Value;
			}

			[[nodiscard]] auto get() const noexcept -> const GError*
			{
				return Value;
			}

		private:
			GError* Value = nullptr;
		};

		template <typename Object>
		class object_owner final
		{
		public:
			explicit object_owner(Object* object) noexcept
				: ObjectValue{ object }
			{
			}

			~object_owner() noexcept
			{
				reset();
			}

			object_owner(const object_owner&)                    = delete;
			auto operator=(const object_owner&) -> object_owner& = delete;

			object_owner(object_owner&& other) noexcept
				: ObjectValue{ std::exchange(other.ObjectValue, nullptr) }
			{
			}

			auto operator=(object_owner&& other) noexcept -> object_owner&
			{
				if (this != &other) {
					reset();
					ObjectValue = std::exchange(other.ObjectValue, nullptr);
				}
				return *this;
			}

			[[nodiscard]] auto get() const noexcept -> Object*
			{
				return ObjectValue;
			}

			void reset() noexcept
			{
				if (ObjectValue != nullptr) {
					g_object_unref(ObjectValue);
					ObjectValue = nullptr;
				}
			}

		private:
			Object* ObjectValue = nullptr;
		};

		class hash_table_owner final
		{
		public:
			explicit hash_table_owner(GHashTable* table) noexcept
				: Table{ table }
			{
			}

			~hash_table_owner() noexcept
			{
				if (Table != nullptr) {
					g_hash_table_unref(Table);
				}
			}

			hash_table_owner(const hash_table_owner&)                    = delete;
			auto operator=(const hash_table_owner&) -> hash_table_owner& = delete;
			hash_table_owner(hash_table_owner&&)                         = delete;
			auto operator=(hash_table_owner&&) -> hash_table_owner&      = delete;

			[[nodiscard]] auto get() const noexcept -> GHashTable*
			{
				return Table;
			}

		private:
			GHashTable* Table = nullptr;
		};

		void unref_list_object(gpointer object) noexcept
		{
			if (object != nullptr) {
				g_object_unref(object);
			}
		}

		class item_list_owner final
		{
		public:
			explicit item_list_owner(GList* items) noexcept
				: Items{ items }
			{
			}

			~item_list_owner() noexcept
			{
				g_list_free_full(Items, unref_list_object);
			}

			item_list_owner(const item_list_owner&)                    = delete;
			auto operator=(const item_list_owner&) -> item_list_owner& = delete;
			item_list_owner(item_list_owner&&)                         = delete;
			auto operator=(item_list_owner&&) -> item_list_owner&      = delete;

			[[nodiscard]] auto get() const noexcept -> GList*
			{
				return Items;
			}

		private:
			GList* Items = nullptr;
		};

		class secret_value_owner final
		{
		public:
			explicit secret_value_owner(SecretValue* value) noexcept
				: Value{ value }
			{
			}

			~secret_value_owner() noexcept
			{
				if (Value != nullptr) {
					secret_value_unref(Value);
				}
			}

			secret_value_owner(const secret_value_owner&)                    = delete;
			auto operator=(const secret_value_owner&) -> secret_value_owner& = delete;
			secret_value_owner(secret_value_owner&&)                         = delete;
			auto operator=(secret_value_owner&&) -> secret_value_owner&      = delete;

			[[nodiscard]] auto get() const noexcept -> SecretValue*
			{
				return Value;
			}

		private:
			SecretValue* Value = nullptr;
		};

		struct NonPromptingSecretService
		{
			SecretService ParentInstance;
		};

		struct NonPromptingSecretServiceClass
		{
			SecretServiceClass ParentClass;
		};

		void dismiss_prompt(SecretPrompt* prompt) noexcept
		{
			if (prompt == nullptr) {
				return;
			}

			GError* error  = nullptr;
			auto*   result = g_dbus_proxy_call_sync(G_DBUS_PROXY(prompt),
													"Dismiss",
													g_variant_new("()"),
													G_DBUS_CALL_FLAGS_NO_AUTO_START,
													prompt_dismiss_timeout_milliseconds,
													nullptr,
													&error);
			if (result != nullptr) {
				g_variant_unref(result);
			}
			if (error != nullptr) {
				g_error_free(error);
			}
		}

		[[nodiscard]] auto prompt_denied_error() noexcept -> GError*
		{
			return g_error_new_literal(SECRET_ERROR,
									   SECRET_ERROR_IS_LOCKED,
									   "interactive Secret Service prompt denied");
		}

		[[nodiscard]] auto deny_prompt_sync(SecretService*,
											SecretPrompt* prompt,
											GCancellable*,
											const GVariantType*,
											GError** error) noexcept -> GVariant*
		{
			dismiss_prompt(prompt);
			if (error != nullptr) {
				*error = prompt_denied_error();
			}
			return nullptr;
		}

		void deny_prompt_async(SecretService* service,
							   SecretPrompt*  prompt,
							   const GVariantType*,
							   GCancellable*       cancellable,
							   GAsyncReadyCallback callback,
							   gpointer            user_data) noexcept
		{
			dismiss_prompt(prompt);
			auto* task = g_task_new(service, cancellable, callback, user_data);
			g_task_return_error(task, prompt_denied_error());
			g_object_unref(task);
		}

		[[nodiscard]] auto deny_prompt_finish(
			SecretService* service, GAsyncResult* result, GError** error) noexcept
			-> GVariant*
		{
			if (!g_task_is_valid(result, service)) {
				if (error != nullptr) {
					*error = g_error_new_literal(SECRET_ERROR,
												 SECRET_ERROR_PROTOCOL,
												 "invalid noninteractive prompt result");
				}
				return nullptr;
			}
			return static_cast<GVariant*>(
				g_task_propagate_pointer(G_TASK(result), error));
		}

		G_DEFINE_TYPE(NonPromptingSecretService,
					  non_prompting_secret_service,
					  SECRET_TYPE_SERVICE)

		void non_prompting_secret_service_init(NonPromptingSecretService*)
		{
		}

		void non_prompting_secret_service_class_init(
			NonPromptingSecretServiceClass* service_class)
		{
			auto* parent          = SECRET_SERVICE_CLASS(service_class);
			parent->prompt_sync   = deny_prompt_sync;
			parent->prompt_async  = deny_prompt_async;
			parent->prompt_finish = deny_prompt_finish;
		}

		[[nodiscard]] auto make_schema(const char* identifier) noexcept -> SecretSchema
		{
			SecretSchema schema{ };
			schema.name               = identifier;
			schema.flags              = SECRET_SCHEMA_NONE;
			schema.attributes[0].name = authority_attribute.data();
			schema.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
			return schema;
		}

		[[nodiscard]] auto schema_for(
			const linux_secret_service_schema_scope scope) noexcept
			-> const SecretSchema*
		{
			static const SecretSchema production       = make_schema(production_schema_identifier.data());
			static const SecretSchema integration_test = make_schema(integration_test_schema_identifier.data());

			switch (scope) {
			case linux_secret_service_schema_scope::production:
				return &production;
			case linux_secret_service_schema_scope::integration_test:
				return &integration_test;
			}
			return nullptr;
		}

		[[nodiscard]] auto label_for(
			const linux_secret_service_schema_scope scope) noexcept -> std::string_view
		{
			switch (scope) {
			case linux_secret_service_schema_scope::production:
				return production_label;
			case linux_secret_service_schema_scope::integration_test:
				return integration_test_label;
			}
			return { };
		}

		using null_terminated_authority_account = std::array<char, linux_secret_service_authority_account_bytes + 1U>;

		[[nodiscard]] auto make_authority_account(
			const application::github_credential_authority_key& authority) noexcept
			-> std::expected<null_terminated_authority_account,
							 application::github_credential_vault_failure>
		{
			const auto encoded = encode_linux_secret_service_authority_account(authority);
			if (!encoded.has_value()) {
				return std::unexpected{ encoded.error() };
			}

			null_terminated_authority_account terminated{ };
			for (std::size_t index = 0U; index < encoded->size(); ++index) {
				terminated[index] = (*encoded)[index];
			}
			return terminated;
		}

		[[nodiscard]] auto make_attributes(
			const null_terminated_authority_account& authority) noexcept -> GHashTable*
		{
			auto* attributes = g_hash_table_new(g_str_hash, g_str_equal);
			if (attributes == nullptr) {
				return nullptr;
			}
			g_hash_table_insert(attributes,
								const_cast<char*>(authority_attribute.data()),
								const_cast<char*>(authority.data()));
			return attributes;
		}

		[[nodiscard]] auto status_from_error(const GError* error) noexcept
			-> linux_secret_service_status
		{
			if (error == nullptr) {
				return { };
			}

			auto domain = linux_secret_service_status_domain::other;
			if (error->domain == SECRET_ERROR) {
				domain = linux_secret_service_status_domain::secret_service;
			} else if (error->domain == G_DBUS_ERROR) {
				domain = linux_secret_service_status_domain::dbus;
			} else if (error->domain == G_IO_ERROR) {
				domain = linux_secret_service_status_domain::io;
			}

			return {
				.Domain = domain,
				.Code   = static_cast<std::int32_t>(error->code),
			};
		}

		[[nodiscard]] auto mapped_error(const GError* error) noexcept
			-> application::github_credential_vault_failure
		{
			return classify_linux_secret_service_status(status_from_error(error));
		}

		[[nodiscard]] auto exact_attributes(SecretItem*                              item,
											const SecretSchema&                      schema,
											const null_terminated_authority_account& authority) noexcept -> bool
		{
			hash_table_owner attributes{ secret_item_get_attributes(item) };
			if (attributes.get() == nullptr || g_hash_table_size(attributes.get()) != 2U) {
				return false;
			}

			const auto* actual_authority = static_cast<const char*>(
				g_hash_table_lookup(attributes.get(), authority_attribute.data()));
			const auto* actual_schema = static_cast<const char*>(
				g_hash_table_lookup(attributes.get(), schema_attribute.data()));
			return actual_authority != nullptr && actual_schema != nullptr
				&& std::strcmp(actual_authority, authority.data()) == 0
				&& std::strcmp(actual_schema, schema.name) == 0;
		}

		struct unique_item_absent
		{
		};

		using unique_item_result = std::variant<SecretItem*,
												unique_item_absent,
												application::github_credential_vault_failure>;

		[[nodiscard]] auto find_unique_item(SecretCollection*                        collection,
											const SecretSchema&                      schema,
											GHashTable*                              attributes,
											const null_terminated_authority_account& authority) noexcept
			-> unique_item_result
		{
			error_owner     error;
			item_list_owner items{ secret_collection_search_sync(collection,
																 &schema,
																 attributes,
																 SECRET_SEARCH_ALL,
																 nullptr,
																 error.out()) };
			if (error.get() != nullptr) {
				return mapped_error(error.get());
			}
			if (items.get() == nullptr) {
				return unique_item_absent{ };
			}
			if (items.get()->next != nullptr) {
				return application::github_credential_vault_failure::corrupt;
			}

			auto* item = SECRET_ITEM(items.get()->data);
			if (item == nullptr || !exact_attributes(item, schema, authority)) {
				return application::github_credential_vault_failure::corrupt;
			}

			g_object_ref(item);
			return item;
		}

		class native_linux_secret_service_backend final
			: public linux_secret_service_backend
		{
		public:
			explicit native_linux_secret_service_backend(
				const linux_secret_service_schema_scope scope) noexcept
				: Scope{ scope }
			{
			}

			~native_linux_secret_service_backend() override
			{
				if (Service != nullptr) {
					g_object_unref(Service);
				}
			}

			[[nodiscard]] auto store(
				const application::github_credential_authority_key& authority,
				const application::github_credential_material&      credential)
				-> linux_secret_service_backend_store_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(authority) || !application::valid(credential)
					|| !valid(Scope)) {
					return std::unexpected{
						application::github_credential_vault_failure::invalid_request
					};
				}

				const auto encoded = make_authority_account(authority);
				if (!encoded.has_value()) {
					return std::unexpected{ encoded.error() };
				}
				hash_table_owner attributes{ make_attributes(*encoded) };
				if (attributes.get() == nullptr) {
					return std::unexpected{ application::
												github_credential_vault_failure::allocation_failure };
				}

				const auto service = service_proxy();
				if (!service.has_value()) {
					return std::unexpected{ service.error() };
				}
				const auto collection = default_collection(*service);
				if (!collection.has_value()) {
					return std::unexpected{ collection.error() };
				}
				object_owner<SecretCollection> collection_owner{ *collection };
				if (secret_collection_get_locked(collection_owner.get())) {
					return std::unexpected{
						application::github_credential_vault_failure::locked
					};
				}

				const auto* schema = schema_for(Scope);
				if (schema == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::invalid_request
					};
				}
				auto existing = find_unique_item(
					collection_owner.get(), *schema, attributes.get(), *encoded);
				if (const auto* failure = std::get_if<application::github_credential_vault_failure>(
						&existing);
					failure != nullptr) {
					return std::unexpected{ *failure };
				}
				if (const auto* item = std::get_if<SecretItem*>(&existing);
					item != nullptr) {
					object_owner<SecretItem> existing_owner{ *item };
					if (secret_item_get_locked(existing_owner.get())) {
						return std::unexpected{
							application::github_credential_vault_failure::locked
						};
					}
				}

				const auto         bytes = credential.bytes();
				secret_value_owner secret{
					secret_value_new(reinterpret_cast<const char*>(bytes.data()),
									 static_cast<gssize>(bytes.size()),
									 binary_content_type.data())
				};
				if (secret.get() == nullptr) {
					return std::unexpected{ application::
												github_credential_vault_failure::allocation_failure };
				}

				error_owner              error;
				object_owner<SecretItem> stored{
					secret_item_create_sync(collection_owner.get(),
											schema,
											attributes.get(),
											label_for(Scope).data(),
											secret.get(),
											SECRET_ITEM_CREATE_REPLACE,
											nullptr,
											error.out())
				};
				if (stored.get() == nullptr) {
					return std::unexpected{ mapped_error(error.get()) };
				}
				if (secret_item_get_locked(stored.get())
					|| !exact_attributes(stored.get(), *schema, *encoded)) {
					return std::unexpected{
						application::github_credential_vault_failure::corrupt
					};
				}
				return application::github_credential_store_disposition::stored;
			}

			[[nodiscard]] auto retrieve(
				const application::github_credential_authority_key& authority)
				-> linux_secret_service_backend_retrieve_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(authority) || !valid(Scope)) {
					return application::github_credential_vault_failure::
						invalid_request;
				}

				const auto encoded = make_authority_account(authority);
				if (!encoded.has_value()) {
					return encoded.error();
				}
				hash_table_owner attributes{ make_attributes(*encoded) };
				if (attributes.get() == nullptr) {
					return application::github_credential_vault_failure::
						allocation_failure;
				}

				const auto service = service_proxy();
				if (!service.has_value()) {
					return service.error();
				}
				const auto collection = default_collection(*service);
				if (!collection.has_value()) {
					return collection.error();
				}
				object_owner<SecretCollection> collection_owner{ *collection };
				if (secret_collection_get_locked(collection_owner.get())) {
					return application::github_credential_vault_failure::locked;
				}

				const auto* schema = schema_for(Scope);
				if (schema == nullptr) {
					return application::github_credential_vault_failure::
						invalid_request;
				}
				auto found = find_unique_item(
					collection_owner.get(), *schema, attributes.get(), *encoded);
				if (std::holds_alternative<unique_item_absent>(found)) {
					return linux_secret_service_credential_absent{ };
				}
				if (const auto* failure = std::get_if<application::github_credential_vault_failure>(
						&found);
					failure != nullptr) {
					return *failure;
				}

				object_owner<SecretItem> item{ std::get<SecretItem*>(found) };
				if (secret_item_get_locked(item.get())) {
					return application::github_credential_vault_failure::locked;
				}

				error_owner load_error;
				if (!secret_item_load_secret_sync(item.get(), nullptr, load_error.out())) {
					return mapped_error(load_error.get());
				}
				if (secret_item_get_locked(item.get())) {
					return application::github_credential_vault_failure::locked;
				}

				auto*       value                   = secret_item_get_secret(item.get());
				const auto* content_type            = value == nullptr ? nullptr : secret_value_get_content_type(value);
				const auto  recognized_content_type = content_type != nullptr
					&& (std::strcmp(content_type, binary_content_type.data()) == 0
						|| std::strcmp(
							   content_type, gnome_keyring_content_type.data())
							== 0);
				if (value == nullptr || content_type == nullptr
					|| !recognized_content_type) {
					return application::github_credential_vault_failure::corrupt;
				}

				gsize       size = 0U;
				const auto* raw  = secret_value_get(value, &size);
				if (raw == nullptr || size == 0U
					|| size > application::maximum_github_credential_bytes) {
					return application::github_credential_vault_failure::corrupt;
				}

				auto material = application::github_credential_material::create(
					std::span<const std::byte>{ reinterpret_cast<const std::byte*>(raw),
												static_cast<std::size_t>(size) });
				if (!material.has_value()) {
					return material.error()
							== application::github_credential_material_error::
								allocation_failure
						? application::github_credential_vault_failure::
							  allocation_failure
						: application::github_credential_vault_failure::corrupt;
				}
				return std::move(*material);
			}

			[[nodiscard]] auto delete_credential(
				const application::github_credential_authority_key& authority)
				-> linux_secret_service_backend_delete_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(authority) || !valid(Scope)) {
					return std::unexpected{
						application::github_credential_vault_failure::invalid_request
					};
				}

				const auto encoded = make_authority_account(authority);
				if (!encoded.has_value()) {
					return std::unexpected{ encoded.error() };
				}
				hash_table_owner attributes{ make_attributes(*encoded) };
				if (attributes.get() == nullptr) {
					return std::unexpected{ application::
												github_credential_vault_failure::allocation_failure };
				}

				const auto service = service_proxy();
				if (!service.has_value()) {
					return std::unexpected{ service.error() };
				}
				const auto collection = default_collection(*service);
				if (!collection.has_value()) {
					return std::unexpected{ collection.error() };
				}
				object_owner<SecretCollection> collection_owner{ *collection };
				if (secret_collection_get_locked(collection_owner.get())) {
					return std::unexpected{
						application::github_credential_vault_failure::locked
					};
				}

				const auto* schema = schema_for(Scope);
				if (schema == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::invalid_request
					};
				}
				auto found = find_unique_item(
					collection_owner.get(), *schema, attributes.get(), *encoded);
				if (std::holds_alternative<unique_item_absent>(found)) {
					return application::github_credential_delete_disposition::absent;
				}
				if (const auto* failure = std::get_if<application::github_credential_vault_failure>(
						&found);
					failure != nullptr) {
					return std::unexpected{ *failure };
				}

				object_owner<SecretItem> item{ std::get<SecretItem*>(found) };
				if (secret_item_get_locked(item.get())) {
					return std::unexpected{
						application::github_credential_vault_failure::locked
					};
				}

				error_owner error;
				if (!secret_item_delete_sync(item.get(), nullptr, error.out())) {
					return std::unexpected{ mapped_error(error.get()) };
				}
				return application::github_credential_delete_disposition::deleted;
			}

		private:
			[[nodiscard]] auto service_proxy() noexcept -> std::expected<SecretService*,
																		 application::github_credential_vault_failure>
			{
				if (Service != nullptr) {
					return Service;
				}

				error_owner error;
				Service = secret_service_open_sync(non_prompting_secret_service_get_type(),
												   canonical_service_bus_name.data(),
												   SECRET_SERVICE_NONE,
												   nullptr,
												   error.out());
				if (Service == nullptr) {
					const auto failure = mapped_error(error.get());
					return std::unexpected{
						failure
								== application::github_credential_vault_failure::
									operation_failed
							? application::github_credential_vault_failure::
								  unavailable
							: failure
					};
				}
				const auto* actual_name = g_dbus_proxy_get_name(G_DBUS_PROXY(Service));
				if (actual_name == nullptr
					|| std::strcmp(actual_name, canonical_service_bus_name.data()) != 0) {
					g_object_unref(Service);
					Service = nullptr;
					return std::unexpected{
						application::github_credential_vault_failure::unavailable
					};
				}
				error_owner session_error;
				if (!secret_service_ensure_session_sync(
						Service, nullptr, session_error.out())) {
					const auto failure = mapped_error(session_error.get());
					g_object_unref(Service);
					Service = nullptr;
					return std::unexpected{
						failure
								== application::github_credential_vault_failure::
									operation_failed
							? application::github_credential_vault_failure::
								  unavailable
							: failure
					};
				}
				return Service;
			}

			[[nodiscard]] static auto default_collection(
				SecretService* service) noexcept -> std::expected<SecretCollection*,
																  application::github_credential_vault_failure>
			{
				error_owner error;
				auto*       collection = secret_collection_for_alias_sync(service,
																		  SECRET_COLLECTION_DEFAULT,
																		  SECRET_COLLECTION_NONE,
																		  nullptr,
																		  error.out());
				if (collection != nullptr) {
					return collection;
				}
				if (error.get() != nullptr) {
					return std::unexpected{ mapped_error(error.get()) };
				}
				return std::unexpected{
					application::github_credential_vault_failure::unavailable
				};
			}

			linux_secret_service_schema_scope Scope;
			std::mutex                        OperationMutex;
			SecretService*                    Service = nullptr;
		};

		[[nodiscard]] auto make_native_backend(
			const linux_secret_service_schema_scope scope) noexcept
			-> std::expected<std::shared_ptr<linux_secret_service_backend>,
							 application::github_credential_vault_failure>
		{
			if (!valid(scope)) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			try {
				return std::shared_ptr<linux_secret_service_backend>{
					std::make_shared<native_linux_secret_service_backend>(scope)
				};
			} catch (const std::bad_alloc&) {
				return std::unexpected{
					application::github_credential_vault_failure::allocation_failure
				};
			} catch (...) {
				return std::unexpected{
					application::github_credential_vault_failure::operation_failed
				};
			}
		}

		struct pending_store
		{
			application::github_credential_enrollment               Enrollment;
			application::github_credential_store_completion_handler Completion;
		};

		struct pending_retrieve
		{
			application::github_credential_operation_context           Context;
			application::github_credential_retrieve_completion_handler Completion;
		};

		struct pending_delete
		{
			application::github_credential_operation_context         Context;
			application::github_credential_delete_completion_handler Completion;
		};

		using pending_operation  = std::variant<pending_store, pending_retrieve, pending_delete>;
		using pending_operations = std::map<application::github_credential_operation_context,
											pending_operation>;

		[[nodiscard]] auto extract_pending(pending_operations&                                     operations,
										   const application::github_credential_operation_context& context)
			-> std::optional<pending_operation>
		{
			auto node = operations.extract(context);
			if (node.empty()) {
				return std::nullopt;
			}
			return std::move(node.mapped());
		}

		void publish_store(pending_store&                                  pending,
						   application::github_credential_store_completion terminal)
		{
			pending.Enrollment.Credential.reset();
			auto completion = std::move(pending.Completion);
			if (completion) {
				completion(std::move(terminal));
			}
		}

		void publish_retrieve(pending_retrieve&                                  pending,
							  application::github_credential_retrieve_completion terminal)
		{
			auto completion = std::move(pending.Completion);
			if (completion) {
				completion(std::move(terminal));
			}
		}

		void publish_delete(pending_delete&                                  pending,
							application::github_credential_delete_completion terminal)
		{
			auto completion = std::move(pending.Completion);
			if (completion) {
				completion(std::move(terminal));
			}
		}

		void publish_failure(pending_operation&                                 operation,
							 const application::github_credential_vault_failure failure)
		{
			const auto effective_failure = normalize_failure(failure);
			std::visit(
				overloaded{
					[effective_failure](pending_store& pending) {
						publish_store(pending,
									  application::github_credential_store_error{
										  .Context = pending.Enrollment.Context,
										  .Failure = effective_failure,
									  });
					},
					[effective_failure](pending_retrieve& pending) {
						publish_retrieve(pending,
										 application::github_credential_retrieve_error{
											 .Context = pending.Context,
											 .Failure = effective_failure,
										 });
					},
					[effective_failure](pending_delete& pending) {
						publish_delete(pending,
									   application::github_credential_delete_error{
										   .Context = pending.Context,
										   .Failure = effective_failure,
									   });
					},
				},
				operation);
		}

		void publish_cancelled(pending_operation& operation)
		{
			std::visit(
				overloaded{
					[](pending_store& pending) {
						publish_store(pending,
									  application::github_credential_store_cancelled{
										  .Context = pending.Enrollment.Context,
									  });
					},
					[](pending_retrieve& pending) {
						publish_retrieve(pending,
										 application::github_credential_retrieve_cancelled{
											 .Context = pending.Context,
										 });
					},
					[](pending_delete& pending) {
						publish_delete(pending,
									   application::github_credential_delete_cancelled{
										   .Context = pending.Context,
									   });
					},
				},
				operation);
		}

		class linux_secret_service_vault final
			: public application::github_credential_vault_port
		{
		private:
			struct shared_state
			{
				explicit shared_state(
					std::shared_ptr<linux_secret_service_backend> backend) noexcept
					: Backend{ std::move(backend) }
				{
				}

				std::mutex                                    Mutex;
				pending_operations                            Pending;
				std::shared_ptr<linux_secret_service_backend> Backend;
				bool                                          Closing = false;
			};

		public:
			linux_secret_service_vault(application::effect_executor&                 effect_lane,
									   std::shared_ptr<linux_secret_service_backend> backend)
				: EffectLane{ effect_lane }
				, State{ std::make_shared<shared_state>(std::move(backend)) }
			{
			}

			~linux_secret_service_vault() override
			{
				pending_operations cancelled;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					State->Closing  = true;
					cancelled.swap(State->Pending);
				}

				while (!cancelled.empty()) {
					auto node = cancelled.extract(cancelled.begin());
					try {
						publish_cancelled(node.mapped());
					} catch (...) {
					}
				}
			}

			void store(application::github_credential_enrollment               enrollment,
					   application::github_credential_store_completion_handler completion)
				override
			{
				const auto context = enrollment.Context;
				if (!application::valid(enrollment) || !completion) {
					enrollment.Credential.reset();
					if (completion) {
						completion(application::github_credential_store_error{
							.Context = context,
							.Failure = application::github_credential_vault_failure::
								invalid_request,
						});
					}
					return;
				}

				queue(context,
					  pending_store{
						  .Enrollment = std::move(enrollment),
						  .Completion = std::move(completion),
					  });
			}

			void retrieve(application::github_credential_operation_context           context,
						  application::github_credential_retrieve_completion_handler completion)
				override
			{
				if (!application::valid(context) || !completion) {
					if (completion) {
						completion(application::github_credential_retrieve_error{
							.Context = context,
							.Failure = application::github_credential_vault_failure::
								invalid_request,
						});
					}
					return;
				}

				queue(context,
					  pending_retrieve{
						  .Context    = context,
						  .Completion = std::move(completion),
					  });
			}

			void delete_credential(
				application::github_credential_operation_context         context,
				application::github_credential_delete_completion_handler completion)
				override
			{
				if (!application::valid(context) || !completion) {
					if (completion) {
						completion(application::github_credential_delete_error{
							.Context = context,
							.Failure = application::github_credential_vault_failure::
								invalid_request,
						});
					}
					return;
				}

				queue(context,
					  pending_delete{
						  .Context    = context,
						  .Completion = std::move(completion),
					  });
			}

			// The refresh slot has no Secret Service schema of its own yet, so it
			// reports itself unavailable rather than sharing the credential
			// attributes and letting one slot answer for the other. Enabling it
			// belongs to BPR-0311.
			void store_refresh(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				const auto context = aEnrollment.Context;
				auto       pending
					= std::make_shared<application::github_credential_enrollment>(
						std::move(aEnrollment));
				EffectLane.post(
					[pending = std::move(pending), context, completion = std::move(aCompletion)]() mutable {
						pending->Credential.reset();
						if (completion) {
							completion(application::github_credential_store_error{
								.Context = context,
								.Failure = application::github_credential_vault_failure::
									unavailable,
							});
						}
					});
			}

			void retrieve_refresh(
				application::github_credential_operation_context           aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
			{
				EffectLane.post(
					[aContext, completion = std::move(aCompletion)]() mutable {
						if (completion) {
							completion(application::github_credential_retrieve_error{
								.Context = aContext,
								.Failure = application::github_credential_vault_failure::
									unavailable,
							});
						}
					});
			}

			void delete_refresh(
				application::github_credential_operation_context         aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
			{
				EffectLane.post(
					[aContext, completion = std::move(aCompletion)]() mutable {
						if (completion) {
							completion(application::github_credential_delete_error{
								.Context = aContext,
								.Failure = application::github_credential_vault_failure::
									unavailable,
							});
						}
					});
			}

			void cancel(
				application::github_credential_operation_context context) override
			{
				std::optional<pending_operation> pending;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					pending         = extract_pending(State->Pending, context);
				}
				if (pending.has_value()) {
					publish_cancelled(*pending);
				}
			}

		private:
			void queue(const application::github_credential_operation_context& context,
					   pending_operation                                       operation)
			{
				auto rejected = false;
				auto closing  = false;
				try {
					const auto lock = std::scoped_lock{ State->Mutex };
					closing         = State->Closing;
					if (!closing) {
						rejected = !State->Pending.try_emplace(context, std::move(operation))
										.second;
					}
				} catch (const std::bad_alloc&) {
					publish_failure(operation,
									application::github_credential_vault_failure::
										allocation_failure);
					return;
				} catch (...) {
					publish_failure(operation,
									application::github_credential_vault_failure::operation_failed);
					return;
				}

				if (closing) {
					publish_failure(operation,
									application::github_credential_vault_failure::unavailable);
					return;
				}
				if (rejected) {
					publish_failure(operation,
									application::github_credential_vault_failure::invalid_request);
					return;
				}

				const auto weak_state = std::weak_ptr<shared_state>{ State };
				try {
					EffectLane.post(
						[weak_state, context] {
							const auto state = weak_state.lock();
							if (state != nullptr) {
								execute(state, context);
							}
						});
				} catch (const std::bad_alloc&) {
					fail_queued(context,
								application::github_credential_vault_failure::
									allocation_failure);
				} catch (...) {
					fail_queued(context,
								application::github_credential_vault_failure::operation_failed);
				}
			}

			void fail_queued(
				const application::github_credential_operation_context& context,
				const application::github_credential_vault_failure      failure)
			{
				std::optional<pending_operation> pending;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					pending         = extract_pending(State->Pending, context);
				}
				if (pending.has_value()) {
					publish_failure(*pending, failure);
				}
			}

			static void execute(const std::shared_ptr<shared_state>&                    state,
								const application::github_credential_operation_context& context)
			{
				std::optional<pending_operation>              pending;
				std::shared_ptr<linux_secret_service_backend> backend;
				{
					const auto lock = std::scoped_lock{ state->Mutex };
					pending         = extract_pending(state->Pending, context);
					if (pending.has_value()) {
						backend = state->Backend;
					}
				}
				if (!pending.has_value()) {
					return;
				}

				std::visit(
					overloaded{
						[&backend](pending_store& operation) { execute_store(backend, operation); },
						[&backend](pending_retrieve& operation) { execute_retrieve(backend, operation); },
						[&backend](pending_delete& operation) { execute_delete(backend, operation); },
					},
					*pending);
			}

			static void execute_store(
				const std::shared_ptr<linux_secret_service_backend>& backend,
				pending_store&                                       pending)
			{
				try {
					const auto result = backend->store(pending.Enrollment.Context.Authority,
													   pending.Enrollment.Credential);
					if (result.has_value() && application::valid(*result)) {
						publish_store(pending,
									  application::github_credential_store_success{
										  .Context     = pending.Enrollment.Context,
										  .Disposition = *result,
									  });
						return;
					}

					const auto failure = result.has_value()
						? application::github_credential_vault_failure::
							  operation_failed
						: result.error();
					publish_store(pending,
								  application::github_credential_store_error{
									  .Context = pending.Enrollment.Context,
									  .Failure = normalize_failure(failure),
								  });
				} catch (const std::bad_alloc&) {
					publish_store(pending,
								  application::github_credential_store_error{
									  .Context = pending.Enrollment.Context,
									  .Failure = application::github_credential_vault_failure::
										  allocation_failure,
								  });
				} catch (...) {
					publish_store(pending,
								  application::github_credential_store_error{
									  .Context = pending.Enrollment.Context,
									  .Failure = application::github_credential_vault_failure::
										  operation_failed,
								  });
				}
			}

			static void execute_retrieve(
				const std::shared_ptr<linux_secret_service_backend>& backend,
				pending_retrieve&                                    pending)
			{
				try {
					auto result = backend->retrieve(pending.Context.Authority);
					std::visit(
						overloaded{
							[&pending](
								application::github_credential_material& credential) {
								if (!application::valid(credential)) {
									credential.reset();
									publish_retrieve(pending,
													 application::github_credential_retrieve_error{
														 .Context = pending.Context,
														 .Failure = application::
															 github_credential_vault_failure::
																 corrupt,
													 });
									return;
								}
								publish_retrieve(pending,
												 application::github_credential_retrieve_success{
													 .Context    = pending.Context,
													 .Credential = std::move(credential),
												 });
							},
							[&pending](linux_secret_service_credential_absent) {
								publish_retrieve(pending,
												 application::github_credential_retrieve_absent{
													 .Context = pending.Context,
												 });
							},
							[&pending](
								const application::github_credential_vault_failure
									failure) {
								publish_retrieve(pending,
												 application::github_credential_retrieve_error{
													 .Context = pending.Context,
													 .Failure = normalize_failure(failure),
												 });
							},
						},
						result);
				} catch (const std::bad_alloc&) {
					publish_retrieve(pending,
									 application::github_credential_retrieve_error{
										 .Context = pending.Context,
										 .Failure = application::github_credential_vault_failure::
											 allocation_failure,
									 });
				} catch (...) {
					publish_retrieve(pending,
									 application::github_credential_retrieve_error{
										 .Context = pending.Context,
										 .Failure = application::github_credential_vault_failure::
											 operation_failed,
									 });
				}
			}

			static void execute_delete(
				const std::shared_ptr<linux_secret_service_backend>& backend,
				pending_delete&                                      pending)
			{
				try {
					const auto result = backend->delete_credential(pending.Context.Authority);
					if (result.has_value() && application::valid(*result)) {
						publish_delete(pending,
									   application::github_credential_delete_success{
										   .Context     = pending.Context,
										   .Disposition = *result,
									   });
						return;
					}

					const auto failure = result.has_value()
						? application::github_credential_vault_failure::
							  operation_failed
						: result.error();
					publish_delete(pending,
								   application::github_credential_delete_error{
									   .Context = pending.Context,
									   .Failure = normalize_failure(failure),
								   });
				} catch (const std::bad_alloc&) {
					publish_delete(pending,
								   application::github_credential_delete_error{
									   .Context = pending.Context,
									   .Failure = application::github_credential_vault_failure::
										   allocation_failure,
								   });
				} catch (...) {
					publish_delete(pending,
								   application::github_credential_delete_error{
									   .Context = pending.Context,
									   .Failure = application::github_credential_vault_failure::
										   operation_failed,
								   });
				}
			}

			application::effect_executor& EffectLane;
			std::shared_ptr<shared_state> State;
		};

		[[nodiscard]] auto make_vault(application::effect_executor&                 effect_lane,
									  std::shared_ptr<linux_secret_service_backend> backend) noexcept
			-> linux_secret_service_vault_factory_result
		{
			if (backend == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			try {
				return std::unique_ptr<application::github_credential_vault_port>{
					std::make_unique<linux_secret_service_vault>(
						effect_lane, std::move(backend))
				};
			} catch (const std::bad_alloc&) {
				return std::unexpected{
					application::github_credential_vault_failure::allocation_failure
				};
			} catch (...) {
				return std::unexpected{
					application::github_credential_vault_failure::operation_failed
				};
			}
		}
	}

	auto linux_secret_service_schema_identifier(
		const linux_secret_service_schema_scope scope) noexcept -> std::string_view
	{
		switch (scope) {
		case linux_secret_service_schema_scope::production:
			return production_schema_identifier;
		case linux_secret_service_schema_scope::integration_test:
			return integration_test_schema_identifier;
		}
		return { };
	}

	auto encode_linux_secret_service_authority_account(
		const application::github_credential_authority_key& authority) noexcept
		-> std::expected<linux_secret_service_authority_account,
						 application::github_credential_vault_failure>
	{
		if (!application::valid(authority)) {
			return std::unexpected{
				application::github_credential_vault_failure::invalid_request
			};
		}

		linux_secret_service_authority_account encoded{ };
		std::size_t                            offset = 0U;
		append_hex_u64(encoded, offset, authority.Profile.value());
		append_hex_u64(encoded, offset, authority.ProfileRevision.value());
		append_hex_u64(encoded, offset, authority.Host.value());
		append_hex_u64(encoded, offset, authority.Account.value());
		append_hex_u8(encoded,
					  offset,
					  server_contract_discriminator(authority.ServerContract));
		const auto hostname = authority.Hostname.value();
		append_hex_u8(
			encoded, offset, static_cast<std::uint8_t>(hostname.size()));
		for (std::size_t index = 0U;
			 index < application::maximum_github_hostname_bytes;
			 ++index) {
			const std::uint8_t value = index < hostname.size()
				? static_cast<std::uint8_t>(
					  static_cast<unsigned char>(hostname[index]))
				: std::uint8_t{ 0U };
			append_hex_u8(encoded, offset, value);
		}
		return encoded;
	}

	auto classify_linux_secret_service_status(
		const linux_secret_service_status status) noexcept
		-> application::github_credential_vault_failure
	{
		using failure = application::github_credential_vault_failure;

		switch (status.Domain) {
		case linux_secret_service_status_domain::secret_service:
			switch (status.Code) {
			case SECRET_ERROR_IS_LOCKED:
				return failure::locked;
			case SECRET_ERROR_PROTOCOL:
			case SECRET_ERROR_INVALID_FILE_FORMAT:
			case SECRET_ERROR_MISMATCHED_SCHEMA:
			case SECRET_ERROR_NO_MATCHING_ATTRIBUTE:
			case SECRET_ERROR_WRONG_TYPE:
			case SECRET_ERROR_EMPTY_TABLE:
				return failure::corrupt;
			case SECRET_ERROR_NO_SUCH_OBJECT:
				return failure::unavailable;
			case SECRET_ERROR_ALREADY_EXISTS:
				return failure::operation_failed;
			default:
				return failure::operation_failed;
			}
		case linux_secret_service_status_domain::dbus:
			switch (status.Code) {
			case G_DBUS_ERROR_NO_MEMORY:
			case G_DBUS_ERROR_SPAWN_NO_MEMORY:
				return failure::allocation_failure;
			case G_DBUS_ERROR_ACCESS_DENIED:
			case G_DBUS_ERROR_AUTH_FAILED:
			case G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID:
				return failure::access_denied;
			case G_DBUS_ERROR_SERVICE_UNKNOWN:
			case G_DBUS_ERROR_NAME_HAS_NO_OWNER:
			case G_DBUS_ERROR_NO_REPLY:
			case G_DBUS_ERROR_IO_ERROR:
			case G_DBUS_ERROR_BAD_ADDRESS:
			case G_DBUS_ERROR_NO_SERVER:
			case G_DBUS_ERROR_TIMEOUT:
			case G_DBUS_ERROR_NO_NETWORK:
			case G_DBUS_ERROR_DISCONNECTED:
			case G_DBUS_ERROR_UNKNOWN_METHOD:
			case G_DBUS_ERROR_UNKNOWN_OBJECT:
			case G_DBUS_ERROR_UNKNOWN_INTERFACE:
			case G_DBUS_ERROR_TIMED_OUT:
			case G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND:
				return failure::unavailable;
			case G_DBUS_ERROR_INVALID_FILE_CONTENT:
				return failure::corrupt;
			default:
				return failure::operation_failed;
			}
		case linux_secret_service_status_domain::io:
			switch (status.Code) {
			case G_IO_ERROR_PERMISSION_DENIED:
			case G_IO_ERROR_PROXY_AUTH_FAILED:
			case G_IO_ERROR_PROXY_NEED_AUTH:
			case G_IO_ERROR_PROXY_NOT_ALLOWED:
				return failure::access_denied;
			case G_IO_ERROR_INVALID_ARGUMENT:
				return failure::invalid_request;
			case G_IO_ERROR_PARTIAL_INPUT:
			case G_IO_ERROR_INVALID_DATA:
			case G_IO_ERROR_MESSAGE_TOO_LARGE:
				return failure::corrupt;
			case G_IO_ERROR_NOT_FOUND:
			case G_IO_ERROR_NOT_INITIALIZED:
			case G_IO_ERROR_CLOSED:
			case G_IO_ERROR_TIMED_OUT:
			case G_IO_ERROR_HOST_NOT_FOUND:
			case G_IO_ERROR_DBUS_ERROR:
			case G_IO_ERROR_HOST_UNREACHABLE:
			case G_IO_ERROR_NETWORK_UNREACHABLE:
			case G_IO_ERROR_CONNECTION_REFUSED:
			case G_IO_ERROR_PROXY_FAILED:
			case G_IO_ERROR_BROKEN_PIPE:
			case G_IO_ERROR_NOT_CONNECTED:
				return failure::unavailable;
			default:
				return failure::operation_failed;
			}
		case linux_secret_service_status_domain::none:
		case linux_secret_service_status_domain::other:
			return failure::operation_failed;
		}
		return failure::operation_failed;
	}

	auto make_linux_secret_service_vault(application::effect_executor& effect_lane)
		-> linux_secret_service_vault_factory_result
	{
		auto backend = make_native_backend(linux_secret_service_schema_scope::production);
		if (!backend.has_value()) {
			return std::unexpected{ backend.error() };
		}
		return make_vault(effect_lane, std::move(*backend));
	}

	auto make_linux_secret_service_integration_test_vault(
		application::effect_executor& effect_lane)
		-> linux_secret_service_vault_factory_result
	{
		auto backend = make_native_backend(
			linux_secret_service_schema_scope::integration_test);
		if (!backend.has_value()) {
			return std::unexpected{ backend.error() };
		}
		return make_vault(effect_lane, std::move(*backend));
	}

	auto make_linux_secret_service_vault_with_backend(
		application::effect_executor&                 effect_lane,
		std::shared_ptr<linux_secret_service_backend> backend) noexcept
		-> linux_secret_service_vault_factory_result
	{
		return make_vault(effect_lane, std::move(backend));
	}
}
// NOLINTEND(readability-identifier-naming)
