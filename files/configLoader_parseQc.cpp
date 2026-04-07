// =============================================================================
//  PATCH FOR configLoader.cpp
//  Add the following block at the end of the file, just before the closing
//  "} // namespace loki" brace.
//
//  Also add the following lines inside _parsePlots(), after the kalman block:
//
//      cfg.qcCoverage  = getOrDefault<bool>(j, "qc_coverage",  true,  false);
//      cfg.qcHistogram = getOrDefault<bool>(j, "qc_histogram", true,  false);
//      cfg.qcAcf       = getOrDefault<bool>(j, "qc_acf",       false, false);
//
//  And inside AppConfig ConfigLoader::load(), after the kalman block:
//
//      if (j.contains("qc"))      cfg.qc      = _parseQc     (j.at("qc"));
// =============================================================================

QcConfig ConfigLoader::_parseQc(const nlohmann::json& j)
{
    QcConfig cfg{};

    cfg.temporalEnabled   = getOrDefault<bool>  (j, "temporal_enabled",    true,  false);
    cfg.statsEnabled      = getOrDefault<bool>  (j, "stats_enabled",       true,  false);
    cfg.outlierEnabled    = getOrDefault<bool>  (j, "outlier_enabled",     true,  false);
    cfg.samplingEnabled   = getOrDefault<bool>  (j, "sampling_enabled",    true,  false);
    cfg.seasonalEnabled   = getOrDefault<bool>  (j, "seasonal_enabled",    true,  false);
    cfg.hurstEnabled      = getOrDefault<bool>  (j, "hurst_enabled",       true,  false);
    cfg.maWindowSize      = getOrDefault<int>   (j, "ma_window_size",      365,   false);
    cfg.significanceLevel = getOrDefault<double>(j, "significance_level",  0.05,  false);
    cfg.uniformityThreshold
                          = getOrDefault<double>(j, "uniformity_threshold",0.05,  false);
    cfg.minSpanYears      = getOrDefault<double>(j, "min_span_years",      10.0,  false);

    if (cfg.maWindowSize < 3) {
        throw ConfigException(
            "ConfigLoader: qc.ma_window_size must be >= 3, got "
            + std::to_string(cfg.maWindowSize) + ".");
    }
    if (cfg.significanceLevel <= 0.0 || cfg.significanceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: qc.significance_level must be in (0, 1), got "
            + std::to_string(cfg.significanceLevel) + ".");
    }
    if (cfg.uniformityThreshold < 0.0 || cfg.uniformityThreshold > 1.0) {
        throw ConfigException(
            "ConfigLoader: qc.uniformity_threshold must be in [0, 1], got "
            + std::to_string(cfg.uniformityThreshold) + ".");
    }
    if (cfg.minSpanYears <= 0.0) {
        throw ConfigException(
            "ConfigLoader: qc.min_span_years must be > 0, got "
            + std::to_string(cfg.minSpanYears) + ".");
    }

    // ---- outlier sub-section ------------------------------------------------
    if (j.contains("outlier")) {
        const auto& o = j.at("outlier");

        cfg.outlier.iqrEnabled      = getOrDefault<bool>  (o, "iqr_enabled",      true,  false);
        cfg.outlier.iqrMultiplier   = getOrDefault<double>(o, "iqr_multiplier",   1.5,   false);
        cfg.outlier.madEnabled      = getOrDefault<bool>  (o, "mad_enabled",      true,  false);
        cfg.outlier.madMultiplier   = getOrDefault<double>(o, "mad_multiplier",   3.0,   false);
        cfg.outlier.zscoreEnabled   = getOrDefault<bool>  (o, "zscore_enabled",   true,  false);
        cfg.outlier.zscoreThreshold = getOrDefault<double>(o, "zscore_threshold", 3.0,   false);

        if (cfg.outlier.iqrMultiplier <= 0.0) {
            throw ConfigException(
                "ConfigLoader: qc.outlier.iqr_multiplier must be > 0, got "
                + std::to_string(cfg.outlier.iqrMultiplier) + ".");
        }
        if (cfg.outlier.madMultiplier <= 0.0) {
            throw ConfigException(
                "ConfigLoader: qc.outlier.mad_multiplier must be > 0, got "
                + std::to_string(cfg.outlier.madMultiplier) + ".");
        }
        if (cfg.outlier.zscoreThreshold <= 0.0) {
            throw ConfigException(
                "ConfigLoader: qc.outlier.zscore_threshold must be > 0, got "
                + std::to_string(cfg.outlier.zscoreThreshold) + ".");
        }
    }

    // ---- seasonal sub-section -----------------------------------------------
    if (j.contains("seasonal")) {
        const auto& s = j.at("seasonal");

        cfg.seasonal.enabled          = getOrDefault<bool>  (s, "enabled",            true, false);
        cfg.seasonal.minYearsPerSlot  = getOrDefault<int>   (s, "min_years_per_slot",  5,   false);
        cfg.seasonal.minMonthCoverage = getOrDefault<double>(s, "min_month_coverage",  0.5, false);

        if (cfg.seasonal.minYearsPerSlot < 1) {
            throw ConfigException(
                "ConfigLoader: qc.seasonal.min_years_per_slot must be >= 1, got "
                + std::to_string(cfg.seasonal.minYearsPerSlot) + ".");
        }
        if (cfg.seasonal.minMonthCoverage < 0.0 || cfg.seasonal.minMonthCoverage > 1.0) {
            throw ConfigException(
                "ConfigLoader: qc.seasonal.min_month_coverage must be in [0, 1], got "
                + std::to_string(cfg.seasonal.minMonthCoverage) + ".");
        }
    }

    return cfg;
}
