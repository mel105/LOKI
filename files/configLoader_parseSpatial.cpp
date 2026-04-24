// =============================================================================
//  Append to configLoader.cpp (inside namespace loki { ... } block)
//  Also add:
//    static SpatialConfig _parseSpatial(const nlohmann::json& j);
//  to ConfigLoader class declaration in configLoader.hpp.
//  Also add in load():
//    cfg.spatial = _parseSpatial(j.value("spatial", json::object()));
// =============================================================================

SpatialConfig ConfigLoader::_parseSpatial(const nlohmann::json& j)
{
    SpatialConfig cfg;

    cfg.method          = getOrDefault<std::string>(j, "method",           "kriging", false);
    cfg.crossValidate   = getOrDefault<bool>       (j, "cross_validate",   true,      false);
    cfg.confidenceLevel = getOrDefault<double>     (j, "confidence_level", 0.95,      false);

    const std::vector<std::string> validMethods = {
        "kriging", "idw", "rbf", "natural_neighbor", "bspline_surface", "nurbs"
    };
    bool methodOk = false;
    for (const auto& m : validMethods) if (m == cfg.method) { methodOk = true; break; }
    if (!methodOk) {
        throw ConfigException(
            "ConfigLoader: spatial.method '" + cfg.method
            + "' not recognised. Valid: kriging, idw, rbf, "
              "natural_neighbor, bspline_surface, nurbs (placeholder).");
    }
    if (cfg.confidenceLevel <= 0.0 || cfg.confidenceLevel >= 1.0) {
        throw ConfigException(
            "ConfigLoader: spatial.confidence_level must be in (0, 1), got "
            + std::to_string(cfg.confidenceLevel) + ".");
    }

    // ---- grid sub-section ---------------------------------------------------
    if (j.contains("grid")) {
        const auto& g = j.at("grid");
        cfg.grid.resolutionX = getOrDefault<double>(g, "resolution_x", 0.0, false);
        cfg.grid.resolutionY = getOrDefault<double>(g, "resolution_y", 0.0, false);
        cfg.grid.xMin        = getOrDefault<double>(g, "x_min",        0.0, false);
        cfg.grid.xMax        = getOrDefault<double>(g, "x_max",        0.0, false);
        cfg.grid.yMin        = getOrDefault<double>(g, "y_min",        0.0, false);
        cfg.grid.yMax        = getOrDefault<double>(g, "y_max",        0.0, false);

        if (cfg.grid.resolutionX < 0.0 || cfg.grid.resolutionY < 0.0) {
            throw ConfigException(
                "ConfigLoader: spatial.grid resolution must be >= 0 (0 = auto).");
        }
    }

    // ---- kriging sub-section ------------------------------------------------
    if (j.contains("kriging")) {
        const auto& k = j.at("kriging");
        cfg.kriging.method      = getOrDefault<std::string>(k, "method",       "ordinary", false);
        cfg.kriging.knownMean   = getOrDefault<double>     (k, "known_mean",   0.0,        false);
        cfg.kriging.trendDegree = getOrDefault<int>        (k, "trend_degree", 1,          false);

        if (cfg.kriging.method != "simple" &&
            cfg.kriging.method != "ordinary" &&
            cfg.kriging.method != "universal") {
            throw ConfigException(
                "ConfigLoader: spatial.kriging.method '" + cfg.kriging.method
                + "' not recognised. Valid: simple, ordinary, universal.");
        }
        if (cfg.kriging.trendDegree < 1 || cfg.kriging.trendDegree > 2) {
            throw ConfigException(
                "ConfigLoader: spatial.kriging.trend_degree must be 1 or 2, got "
                + std::to_string(cfg.kriging.trendDegree) + ".");
        }

        // variogram sub-section
        if (k.contains("variogram")) {
            const auto& v = k.at("variogram");
            cfg.kriging.variogram.model    = getOrDefault<std::string>(v, "model",     "spherical", false);
            cfg.kriging.variogram.nLagBins = getOrDefault<int>        (v, "n_lag_bins", 20,         false);
            cfg.kriging.variogram.maxLag   = getOrDefault<double>     (v, "max_lag",    0.0,        false);
            cfg.kriging.variogram.nugget   = getOrDefault<double>     (v, "nugget",     0.0,        false);
            cfg.kriging.variogram.sill     = getOrDefault<double>     (v, "sill",       0.0,        false);
            cfg.kriging.variogram.range    = getOrDefault<double>     (v, "range",      0.0,        false);

            if (cfg.kriging.variogram.nLagBins < 3) {
                throw ConfigException(
                    "ConfigLoader: spatial.kriging.variogram.n_lag_bins must be >= 3.");
            }
        }
    }

    // ---- idw sub-section ----------------------------------------------------
    if (j.contains("idw")) {
        const auto& idw = j.at("idw");
        cfg.idw.power = getOrDefault<double>(idw, "power", 2.0, false);
        if (cfg.idw.power <= 0.0) {
            throw ConfigException(
                "ConfigLoader: spatial.idw.power must be > 0, got "
                + std::to_string(cfg.idw.power) + ".");
        }
    }

    // ---- rbf sub-section ----------------------------------------------------
    if (j.contains("rbf")) {
        const auto& r = j.at("rbf");
        cfg.rbf.kernel  = getOrDefault<std::string>(r, "kernel",  "thin_plate_spline", false);
        cfg.rbf.epsilon = getOrDefault<double>     (r, "epsilon", 0.0,                 false);

        const std::vector<std::string> validKernels = {
            "multiquadric", "inverse_multiquadric", "gaussian",
            "thin_plate_spline", "cubic"
        };
        bool kernelOk = false;
        for (const auto& kn : validKernels) {
            if (kn == cfg.rbf.kernel) { kernelOk = true; break; }
        }
        if (!kernelOk) {
            throw ConfigException(
                "ConfigLoader: spatial.rbf.kernel '" + cfg.rbf.kernel
                + "' not recognised. Valid: multiquadric, inverse_multiquadric, "
                  "gaussian, thin_plate_spline, cubic.");
        }
        if (cfg.rbf.epsilon < 0.0) {
            throw ConfigException(
                "ConfigLoader: spatial.rbf.epsilon must be >= 0 (0 = auto).");
        }
    }

    // ---- bspline_surface sub-section ----------------------------------------
    if (j.contains("bspline_surface")) {
        const auto& b = j.at("bspline_surface");
        cfg.bsplineSurface.degreeU       = getOrDefault<int>        (b, "degree_u",       3,        false);
        cfg.bsplineSurface.degreeV       = getOrDefault<int>        (b, "degree_v",       3,        false);
        cfg.bsplineSurface.nCtrlU        = getOrDefault<int>        (b, "n_ctrl_u",       6,        false);
        cfg.bsplineSurface.nCtrlV        = getOrDefault<int>        (b, "n_ctrl_v",       6,        false);
        cfg.bsplineSurface.knotPlacement = getOrDefault<std::string>(b, "knot_placement", "uniform",false);

        for (int deg : {cfg.bsplineSurface.degreeU, cfg.bsplineSurface.degreeV}) {
            if (deg < 1 || deg > 5) {
                throw ConfigException(
                    "ConfigLoader: spatial.bspline_surface.degree must be in [1, 5].");
            }
        }
        if (cfg.bsplineSurface.nCtrlU < cfg.bsplineSurface.degreeU + 1 ||
            cfg.bsplineSurface.nCtrlV < cfg.bsplineSurface.degreeV + 1) {
            throw ConfigException(
                "ConfigLoader: spatial.bspline_surface.n_ctrl must be >= degree + 1.");
        }
        if (cfg.bsplineSurface.knotPlacement != "uniform" &&
            cfg.bsplineSurface.knotPlacement != "chord_length") {
            throw ConfigException(
                "ConfigLoader: spatial.bspline_surface.knot_placement '"
                + cfg.bsplineSurface.knotPlacement
                + "' not recognised. Valid: uniform, chord_length.");
        }
    }

    return cfg;
}
