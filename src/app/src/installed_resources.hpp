#ifndef BPR_SRC_APP_SRC_INSTALLED_RESOURCES_HPP
#define BPR_SRC_APP_SRC_INSTALLED_RESOURCES_HPP

#include <QString>

#include <expected>
#include <memory>
#include <string_view>

namespace bpr::app {
	/// \brief The file an image keeps its Qt resources in, beside its executable.
	inline constexpr std::string_view installed_resources_file_name = "better_pull_review.rcc";

	/// \brief Why an image's Qt resource bundle was refused.
	enum class installed_resources_refusal {
		missing, ///< There is no file where the image keeps it.
		unreadable, ///< The file could not be read whole, or is implausibly large.
		digest_mismatch, ///< The bytes are not the ones this image was built with.
		not_a_resource_file, ///< The bytes are right, and Qt's resource system refuses them.
	};

	/// \brief The stable diagnostic word for a refusal.
	[[nodiscard]] auto installed_resources_refusal_name(installed_resources_refusal aRefusal) noexcept -> std::string_view;

	/// \brief An image's Qt resource bundle, registered with Qt's resource system.
	/// \details [[BPR-0974]]. A tests-disabled Windows image keeps its Qt
	/// resources -- the application icon, the Dif strips and the syntax
	/// definitions -- in one bundle beside the executable, not inside it. This
	/// owns the bundle's bytes and their registration. A resource read through
	/// it can point into those bytes, so the owner must outlive every Qt object
	/// that can read one: the application declares it before its QApplication.
	/// Destruction unregisters the bundle. A default-constructed owner, which is
	/// what a graph that embeds its resources gets, owns nothing.
	class installed_resources
	{
	public:
		/// \brief Whether this owner holds a registered bundle.
		[[nodiscard]] auto registered() const noexcept -> bool;

	private:
		struct registration;
		struct registration_release
		{
			void operator()(registration* aRegistration) const noexcept;
		};

		std::unique_ptr<registration, registration_release> Registration;

		friend auto load_installed_resources(const QString& aPath, std::string_view aExpectedSha256)
			-> std::expected<installed_resources, installed_resources_refusal>;
	};

	/// \brief Registers the bundle at \p aPath if its bytes are exactly the expected ones.
	/// \param aPath The bundle's file.
	/// \param aExpectedSha256 The lower-case hexadecimal SHA-256 the build recorded.
	/// \returns The registered bundle, or why it was refused. Nothing is
	/// registered on a refusal.
	[[nodiscard]] auto load_installed_resources(const QString& aPath, std::string_view aExpectedSha256)
		-> std::expected<installed_resources, installed_resources_refusal>;

	/// \brief This image's own bundle, from beside its executable.
	/// \details Called before QApplication exists and before anything reads a
	/// resource. The bundle must be the one this image was built with. A graph
	/// that embeds its resources answers an empty owner and never refuses.
	[[nodiscard]] auto load_image_resources() -> std::expected<installed_resources, installed_resources_refusal>;
}

#endif
