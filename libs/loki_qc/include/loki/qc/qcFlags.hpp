#pragma once

#include <cstdint>

namespace loki::qc {

/// @brief Bitfield flag values for QC flagging CSV output.
///
/// Flags are OR-combined per epoch. For example, an epoch that is both an
/// IQR outlier and a MAD outlier receives flag = FLAG_OUTLIER_IQR | FLAG_OUTLIER_MAD = 6.
///
/// Priority for coverage plot colour: FLAG_GAP > FLAG_OUTLIER_* > FLAG_VALID.
constexpr uint8_t FLAG_VALID       = 0;  ///< Epoch is valid -- no issues detected.
constexpr uint8_t FLAG_GAP         = 1;  ///< Epoch is missing (gap in time axis).
constexpr uint8_t FLAG_OUTLIER_IQR = 2;  ///< Flagged by IQR detector.
constexpr uint8_t FLAG_OUTLIER_MAD = 4;  ///< Flagged by MAD detector.
constexpr uint8_t FLAG_OUTLIER_ZSC = 8;  ///< Flagged by Z-score detector.

} // namespace loki::qc
