#include "bpr/application/credential_vault_diagnostics.hpp"
#include "bpr/application/operation_diagnostics.hpp"

#include <type_traits>
#include <utility>
#include <variant>

namespace bpr::application {
	namespace {
		using action  = core::operation_action;
		using failure = core::operation_failure;

		/// A typed error that names no failure is still a failure.
		[[nodiscard]] auto failure_of(const github_credential_vault_failure aFailure) noexcept -> failure
		{
			const auto named = operation_failure_of(aFailure);
			return named == failure::none ? failure::operation_failed : named;
		}

		/// What one terminal adds to its line.
		struct ending
		{
			failure       Failure = failure::none;
			std::uint32_t Count   = 0U;
		};

		[[nodiscard]] auto ending_of(const github_credential_store_completion& aCompletion) noexcept -> ending
		{
			return std::visit(
				[](const auto& aTerminal) noexcept -> ending {
					using type = std::decay_t<decltype(aTerminal)>;
					if constexpr (std::is_same_v<type, github_credential_store_error>) {
						return { .Failure = failure_of(aTerminal.Failure) };
					} else if constexpr (std::is_same_v<type, github_credential_store_cancelled>) {
						return { .Failure = failure::cancelled };
					} else {
						return { };
					}
				},
				aCompletion);
		}

		[[nodiscard]] auto ending_of(const github_credential_retrieve_completion& aCompletion) noexcept -> ending
		{
			return std::visit(
				[](const auto& aTerminal) noexcept -> ending {
					using type = std::decay_t<decltype(aTerminal)>;
					if constexpr (std::is_same_v<type, github_credential_retrieve_error>) {
						return { .Failure = failure_of(aTerminal.Failure) };
					} else if constexpr (std::is_same_v<type, github_credential_retrieve_cancelled>) {
						return { .Failure = failure::cancelled };
					} else if constexpr (std::is_same_v<type, github_credential_retrieve_success>) {
						return { .Count = 1U };
					} else {
						return { };
					}
				},
				aCompletion);
		}

		[[nodiscard]] auto ending_of(const github_credential_delete_completion& aCompletion) noexcept -> ending
		{
			return std::visit(
				[](const auto& aTerminal) noexcept -> ending {
					using type = std::decay_t<decltype(aTerminal)>;
					if constexpr (std::is_same_v<type, github_credential_delete_error>) {
						return { .Failure = failure_of(aTerminal.Failure) };
					} else if constexpr (std::is_same_v<type, github_credential_delete_cancelled>) {
						return { .Failure = failure::cancelled };
					} else {
						return { .Count = aTerminal.Disposition == github_credential_delete_disposition::deleted ? 1U : 0U };
					}
				},
				aCompletion);
		}

		class diagnosed_credential_vault final : public github_credential_vault_port
		{
		public:
			diagnosed_credential_vault(std::unique_ptr<github_credential_vault_port> aVault, std::shared_ptr<diagnostic_log_port> aLog)
				: Vault{ std::move(aVault) }
				, Log{ std::move(aLog) }
			{
			}

			void store(github_credential_enrollment aEnrollment, github_credential_store_completion_handler aCompletion) override
			{
				submit(action::vault_store, std::move(aCompletion), [&](auto aWrapped) { Vault->store(std::move(aEnrollment), std::move(aWrapped)); });
			}
			void retrieve(github_credential_operation_context aContext, github_credential_retrieve_completion_handler aCompletion) override
			{
				submit(action::vault_load, std::move(aCompletion), [&](auto aWrapped) { Vault->retrieve(aContext, std::move(aWrapped)); });
			}
			void delete_credential(github_credential_operation_context aContext, github_credential_delete_completion_handler aCompletion) override
			{
				submit(action::vault_delete, std::move(aCompletion), [&](auto aWrapped) { Vault->delete_credential(aContext, std::move(aWrapped)); });
			}
			void store_refresh(github_credential_enrollment aEnrollment, github_credential_store_completion_handler aCompletion) override
			{
				submit(action::refresh_store, std::move(aCompletion), [&](auto aWrapped) { Vault->store_refresh(std::move(aEnrollment), std::move(aWrapped)); });
			}
			void retrieve_refresh(github_credential_operation_context aContext, github_credential_retrieve_completion_handler aCompletion) override
			{
				submit(action::refresh_load, std::move(aCompletion), [&](auto aWrapped) { Vault->retrieve_refresh(aContext, std::move(aWrapped)); });
			}
			void delete_refresh(github_credential_operation_context aContext, github_credential_delete_completion_handler aCompletion) override
			{
				submit(action::refresh_delete, std::move(aCompletion), [&](auto aWrapped) { Vault->delete_refresh(aContext, std::move(aWrapped)); });
			}
			void cancel(github_credential_operation_context aContext) override { Vault->cancel(aContext); }

		private:
			/// Records the start, forwards the submission with a completion
			/// that records the terminal before handing it on, and records
			/// `unavailable` if the submission throws (it then retained nothing).
			template <typename TCompletion, typename TForward>
			void submit(const action aAction, TCompletion aCompletion, TForward&& aForward)
			{
				record_operation(Log.get(), { .Action = aAction }, true);
				const auto watch   = operation_stopwatch{ };
				auto       wrapped = TCompletion{ [log = Log, aAction, watch, completion = std::move(aCompletion)](auto aTerminal) {
					const auto end = ending_of(aTerminal);
					record_operation(log.get(), { .Action = aAction, .Failure = end.Failure, .DurationMilliseconds = watch.milliseconds(), .Count = end.Count });
					if (completion) {
						completion(std::move(aTerminal));
					}
				} };
				try {
					std::forward<TForward>(aForward)(std::move(wrapped));
				} catch (...) {
					record_operation(Log.get(), { .Action = aAction, .Failure = failure::unavailable, .DurationMilliseconds = watch.milliseconds() });
					throw;
				}
			}

			std::unique_ptr<github_credential_vault_port> Vault;
			std::shared_ptr<diagnostic_log_port>          Log;
		};
	} // namespace

	auto with_credential_vault_diagnostics(std::unique_ptr<github_credential_vault_port> aVault, std::shared_ptr<diagnostic_log_port> aLog)
		-> std::unique_ptr<github_credential_vault_port>
	{
		if (!aVault || !aLog) {
			return aVault;
		}
		return std::make_unique<diagnosed_credential_vault>(std::move(aVault), std::move(aLog));
	}
} // namespace bpr::application
