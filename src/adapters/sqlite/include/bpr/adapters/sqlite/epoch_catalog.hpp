#pragma once

#include "bpr/application/epoch_catalog_port.hpp"

#include <filesystem>
#include <memory>
#include <stop_token>

namespace bpr::adapters::sqlite
{
struct epoch_catalog_configuration
{
    // The owner-private application storage directory. The adapter owns the
    // fixed state/v1/epoch-catalog.sqlite3 path beneath it.
    std::filesystem::path StorageRoot;
};

[[nodiscard]] auto make_epoch_catalog(
    const epoch_catalog_configuration& configuration,
    std::stop_token stop = {})
    -> application::epoch_catalog_result<
        std::unique_ptr<application::epoch_catalog_port>>;
} // namespace bpr::adapters::sqlite
