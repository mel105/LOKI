#include <loki/arima/plotArima.hpp>
#include <loki/io/gnuplot.hpp>
#include <loki/io/plot.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace loki;
using namespace loki::arima;

// ----------------------------------------------------------------------------
//  Construction
// ----------------------------------------------------------------------------

PlotArima::PlotArima(const AppConfig& cfg)
    : m_cfg{cfg}
{
    std::filesystem::create_directories(m_cfg.imgDir);
}

// ----------------------------------------------------------------------------
//  plotAll
// ----------------------------------------------------------------------------

void PlotArima::plotAll(const TimeSeries&          filled,
                         const std::vector<double>& residuals,
                         const ArimaResult&         result,
                         const ForecastResult&      forecast,
                         int                        forecastTail) const
{
    const auto& p = m_cfg.plots;

    if (p.arimaOverlay) {
        try { plotOverlay(filled, residuals, result); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotArima: overlay failed: " + std::string(ex.what()));
        }
    }

    if (p.arimaForecast && !forecast.forecast.empty()) {
        try { plotForecast(filled, residuals, result, forecast, forecastTail); }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotArima: forecast failed: " + std::string(ex.what()));
        }
    }

    if (p.arimaDiagnostics && !result.residuals.empty()) {
        try {
            // Delegate 4-panel diagnostics to loki::Plot::residualDiagnostics.
            // Build stem for output filename.
            const std::string base = _baseName(filled);
            const std::string stem = (m_cfg.imgDir / (base + "_diagnostics")).string();

            loki::Plot corePlot(m_cfg);
            corePlot.residualDiagnostics(result.residuals, result.fitted, stem);
        }
        catch (const LOKIException& ex) {
            LOKI_WARNING("PlotArima: diagnostics failed: " + std::string(ex.what()));
        }
    }
}

// ----------------------------------------------------------------------------
//  plotOverlay
// ----------------------------------------------------------------------------

void PlotArima::plotOverlay(const TimeSeries&          filled,
                             const std::vector<double>& residuals,
                             const ArimaResult&         result) const
{
    if (residuals.empty()) {
        throw DataException("PlotArima::plotOverlay: residuals vector is empty.");
    }
    if (result.fitted.empty()) {
        throw DataException("PlotArima::plotOverlay: result.fitted is empty.");
    }

    const std::string base    = _baseName(filled);
    const auto        datRes  = m_cfg.imgDir / (".tmp_" + base + "_ov_res.dat");
    const auto        datFit  = m_cfg.imgDir / (".tmp_" + base + "_ov_fit.dat");
    const auto        outPath = _outPath(base, "overlay");

    // Write deseasonalized residuals (full length, aligned to filled timestamps)
    {
        std::ofstream dat(datRes);
        dat << std::fixed << std::setprecision(10);
        const std::size_t n = std::min(residuals.size(), filled.size());
        for (std::size_t i = 0; i < n; ++i) {
            dat << filled[i].time.mjd() << "\t" << residuals[i] << "\n";
        }
    }

    // Write fitted values. They are aligned to the TAIL of residuals:
    // the first (residuals.size() - result.fitted.size()) observations
    // were consumed by differencing.
    {
        std::ofstream dat(datFit);
        dat << std::fixed << std::setprecision(10);
        const std::size_t nFit    = result.fitted.size();
        const std::size_t nTotal  = std::min(residuals.size(), filled.size());
        const std::size_t offset  = (nTotal > nFit) ? (nTotal - nFit) : 0;
        for (std::size_t i = 0; i < nFit; ++i) {
            dat << filled[offset + i].time.mjd() << "\t" << result.fitted[i] << "\n";
        }
    }

    // Build ARIMA order label for legend
    std::ostringstream modelLabel;
    modelLabel << "ARIMA(" << result.order.p << ","
               << result.order.d << "," << result.order.q << ")";
    if (result.seasonal.s > 0) {
        modelLabel << "(" << result.seasonal.P << ","
                   << result.seasonal.D << "," << result.seasonal.Q
                   << ")[" << result.seasonal.s << "]";
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1400, 500));
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'ARIMA overlay: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top right");
        gp("set arrow 1 from graph 0, first 0.0 to graph 1, first 0.0"
           " nohead lc rgb '#aaaaaa' lw 1 dt 2");
        gp("plot '" + _fwdSlash(datRes) + "' u 1:2 w l lc rgb '#aaaaaa' lw 1.0 t 'Residuals',"
           " '" + _fwdSlash(datFit) + "' u 1:2 w l lc rgb '#d7191c' lw 1.5 t '"
           + modelLabel.str() + "'");
    } catch (const LOKIException& ex) {
        std::filesystem::remove(datRes);
        std::filesystem::remove(datFit);
        throw;
    }

    std::filesystem::remove(datRes);
    std::filesystem::remove(datFit);
}

// ----------------------------------------------------------------------------
//  plotForecast
// ----------------------------------------------------------------------------

void PlotArima::plotForecast(const TimeSeries&          filled,
                              const std::vector<double>& residuals,
                              const ArimaResult&         result,
                              const ForecastResult&      forecast,
                              int                        nTail) const
{
    if (forecast.forecast.empty()) {
        throw DataException("PlotArima::plotForecast: forecast vector is empty.");
    }

    const std::string base    = _baseName(filled);
    const auto        datObs  = m_cfg.imgDir / (".tmp_" + base + "_fc_obs.dat");
    const auto        datFc   = m_cfg.imgDir / (".tmp_" + base + "_fc_fc.dat");
    const auto        outPath = _outPath(base, "forecast");

    // Last nTail observed residuals with MJD on x-axis
    {
        std::ofstream dat(datObs);
        dat << std::fixed << std::setprecision(10);

        const std::size_t n      = std::min(residuals.size(), filled.size());
        const std::size_t nShow  = static_cast<std::size_t>(
            std::min(nTail, static_cast<int>(n)));
        const std::size_t start  = n - nShow;

        for (std::size_t i = start; i < n; ++i) {
            dat << filled[i].time.mjd() << "\t" << residuals[i] << "\n";
        }
    }

    // Determine last MJD and step size for x-axis continuation
    double lastMjd  = 0.0;
    double stepMjd  = 0.25;  // default: 6h data

    const std::size_t nFilled = filled.size();
    if (nFilled >= 2) {
        lastMjd = filled[nFilled - 1].time.mjd();
        stepMjd = filled[nFilled - 1].time.mjd() - filled[nFilled - 2].time.mjd();
        if (stepMjd <= 0.0) { stepMjd = 0.25; }
    } else if (nFilled == 1) {
        lastMjd = filled[0].time.mjd();
    }

    // Forecast: col1=mjd, col2=point, col3=lower95, col4=upper95
    {
        std::ofstream dat(datFc);
        dat << std::fixed << std::setprecision(10);
        const std::size_t h = forecast.forecast.size();
        for (std::size_t i = 0; i < h; ++i) {
            const double mjd = lastMjd + static_cast<double>(i + 1) * stepMjd;
            dat << mjd << "\t"
                << forecast.forecast[i] << "\t"
                << forecast.lower95[i]  << "\t"
                << forecast.upper95[i]  << "\n";
        }
    }

    // Build model label
    std::ostringstream modelLabel;
    modelLabel << "ARIMA(" << result.order.p << ","
               << result.order.d << "," << result.order.q << ")";
    if (result.seasonal.s > 0) {
        modelLabel << "(" << result.seasonal.P << ","
                   << result.seasonal.D << "," << result.seasonal.Q
                   << ")[" << result.seasonal.s << "]";
    }

    try {
        Gnuplot gp;
        gp("set terminal " + _terminal(1400, 500));
        gp("set output '" + _fwdSlash(outPath) + "'");
        gp("set title 'ARIMA forecast: " + base + "' noenhanced");
        gp("set xlabel 'MJD'");
        gp("set ylabel 'Value'");
        gp("set grid");
        gp("set key top left");

        // Vertical dashed line at forecast boundary
        gp("set arrow 1 from " + std::to_string(lastMjd) + ", graph 0"
           " to " + std::to_string(lastMjd) + ", graph 1"
           " nohead lc rgb '#888888' lw 1.5 dt 2");

        // Horizontal zero line
        gp("set arrow 2 from graph 0, first 0.0 to graph 1, first 0.0"
           " nohead lc rgb '#aaaaaa' lw 1 dt 3");

        // col 1=mjd, 2=forecast, 3=lower95, 4=upper95
        gp("plot '" + _fwdSlash(datObs) + "' u 1:2"
           " w l lc rgb '#2c7bb6' lw 1.5 t 'Observed',"
           " '" + _fwdSlash(datFc) + "' u 1:3:4"
           " w filledcurves lc rgb '#fdae61' fs transparent solid 0.35 t '95% interval',"
           " '" + _fwdSlash(datFc) + "' u 1:2"
           " w l lc rgb '#d7191c' lw 2.0 t '" + modelLabel.str() + " forecast'");
    } catch (const LOKIException& ex) {
        std::filesystem::remove(datObs);
        std::filesystem::remove(datFc);
        throw;
    }

    std::filesystem::remove(datObs);
    std::filesystem::remove(datFc);
}

// ----------------------------------------------------------------------------
//  Private helpers
// ----------------------------------------------------------------------------

std::string PlotArima::_baseName(const TimeSeries& ts) const
{
    const auto& meta    = ts.metadata();
    std::string dataset = m_cfg.input.file.stem().string();
    if (dataset.empty()) dataset = "data";
    std::string param   = meta.componentName.empty() ? "series" : meta.componentName;
    return "arima_" + dataset + "_" + param;
}

std::filesystem::path PlotArima::_outPath(const std::string& base,
                                           const std::string& plotType) const
{
    return m_cfg.imgDir / (base + "_" + plotType + "." + m_cfg.plots.outputFormat);
}

std::string PlotArima::_terminal(int widthPx, int heightPx) const
{
    const std::string& fmt = m_cfg.plots.outputFormat;
    if (fmt == "eps")
        return "postscript eps color noenhanced solid font 'Sans,12'";
    if (fmt == "svg")
        return "svg noenhanced font 'Sans,12' size "
               + std::to_string(widthPx) + "," + std::to_string(heightPx);
    return "pngcairo noenhanced font 'Sans,12' size "
           + std::to_string(widthPx) + "," + std::to_string(heightPx);
}

std::string PlotArima::_fwdSlash(const std::filesystem::path& p)
{
    std::string s = p.string();
    for (auto& c : s) { if (c == '\\') c = '/'; }
    return s;
}
