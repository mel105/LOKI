#pragma once

namespace loki {

/**
 * @brief Controls how NaN (missing) values are handled in statistical functions.
 */
enum class NanPolicy {
    THROW,
    SKIP,
    PROPAGATE
};

} // namespace loki