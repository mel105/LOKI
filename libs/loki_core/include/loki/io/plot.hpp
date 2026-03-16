#pragma once

#include "loki/core/config.hpp"
#include "loki/timeseries/timeSeries.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace loki {

/**
 * @brief High-level plotting interface using gnuplot as the rendering backend.
 *
 * Plot wraps the low-level Gnuplot pipe and provides ready-made plot types
 * suited for time series analysis. Each method generates one PNG (or the
 * format configured in PlotConfig::outputFormat) and saves it to the image
 * output directory defined in AppConfig::imgDir.
 *
 * Two overload families are provided for every plot type:
 *   - TimeSeries overloads: use the full time/metadata information stored in
 *     a TimeSeries object. The x-axis format is derived from PlotConfig (or,
 *     if not set, from AppConfig::input.timeFormat).
 *   - Vector overloads: accept raw std::vector<double> values and use a
 *     sequential integer index as the x-axis. Useful when working with
 *     intermediate algorithmic results that are not yet wrapped in TimeSeries.
 *
 * Gnuplot is invoked per-call. Data are written to a temporary file inside
 * imgDir, consumed by gnuplot, then deleted immediately after rendering.
 *
 * Plotting can be gated by the PlotConfig::enabled flags. Callers are
 * responsible for checking the relevant flag before calling a method, or
 * they can rely on the guard overload plotIfEnabled().
 *
 * Usage example:
 * @code
 *   loki::Plot plot(appConfig);
 *   plot.timeSeries(mySeries, "Station GRAZ - dN");
 *   plot.histogram(mySeries, 40);
 * @endcode
 *
 * @throws loki::IoException  if gnuplot cannot be opened or a file cannot
 *                             be written.
 * @throws loki::DataException if a TimeSeries passed to a plot method is empty.
 */
class Plot {
public:

    // ── Construction ──────────────────────────────────────────────────────────

    /**
     * @brief Constructs a Plot bound to the given application configuration.
     *
     * The constructor does not open a gnuplot process. Each plot method opens
     * its own short-lived pipe so that a failure in one plot does not affect
     * the others.
     *
     * @param config Application configuration. AppConfig::imgDir must be set
     *               and the directory must exist (or be creatable).
     */
    explicit Plot(const AppConfig& config);

    // ── TimeSeries overloads ──────────────────────────────────────────────────

    /**
     * @brief Plots a single time series as a line plot.
     *
     * The x-axis displays time in the format resolved by effectiveTimeFormat().
     * The y-axis label is taken from TimeSeries metadata (componentName + unit).
     *
     * Output file: imgDir/timeSeries_<stationId>_<componentName>.png
     *
     * @param ts    Time series to plot.
     * @param title Optional plot title. If empty, stationId + componentName is used.
     * @throws DataException if ts is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void timeSeries(const TimeSeries& ts, const std::string& title = "");

    /**
     * @brief Two-panel comparison plot.
     *
     * Top panel:    both series overlaid (analysed in black, reference in red).
     * Bottom panel: difference (analysed - reference).
     *
     * Output file: imgDir/comparison_<stationId>.png
     *
     * @param analysed  The series under investigation.
     * @param reference The reference series (must cover the same time span).
     * @param title     Optional plot title.
     * @throws DataException if either series is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void comparison(const TimeSeries& analysed,
                    const TimeSeries& reference,
                    const std::string& title = "");

    /**
     * @brief Plots the autocorrelation function (ACF) as an impulse plot.
     *
     * Lags are computed up to maxLag. A dashed 95% confidence band is drawn
     * at +/- 1.96 / sqrt(n).
     *
     * Output file: imgDir/acf_<stationId>_<componentName>.png
     *
     * @param ts     Time series whose ACF is plotted.
     * @param maxLag Maximum number of lags to compute.
     * @throws DataException if ts has fewer than maxLag + 2 valid observations.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void acf(const TimeSeries& ts, int maxLag = 40);

    /**
     * @brief Plots a histogram of the series values.
     *
     * A normal distribution curve fitted to the sample mean and standard
     * deviation is overlaid on the histogram bars.
     *
     * Output file: imgDir/histogram_<stationId>_<componentName>.png
     *
     * @param ts   Time series whose values are binned.
     * @param bins Number of histogram bins.
     * @throws DataException if ts is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void histogram(const TimeSeries& ts, int bins = 30);

    /**
     * @brief Quantile-quantile plot against the standard normal distribution.
     *
     * Points lying on the diagonal indicate normally distributed residuals.
     * Useful for verifying assumptions before applying parametric tests.
     *
     * Output file: imgDir/qqplot_<stationId>_<componentName>.png
     *
     * @param ts Time series whose values are tested.
     * @throws DataException if ts has fewer than 3 valid observations.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void qqPlot(const TimeSeries& ts);

    /**
     * @brief Box-and-whisker plot with descriptive statistics annotation.
     *
     * The annotation box in the corner of the plot lists: n, mean, median,
     * standard deviation, min, max, Q1, Q3.
     *
     * Output file: imgDir/boxplot_<stationId>_<componentName>.png
     *
     * @param ts Time series to summarise.
     * @throws DataException if ts is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void boxplot(const TimeSeries& ts);

    // ── Raw-vector overloads ──────────────────────────────────────────────────

    /**
     * @brief Plots a raw value vector as a line plot (sequential index x-axis).
     *
     * @param values Vector of y-values.
     * @param title  Plot title and output filename stem.
     * @throws DataException if values is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void timeSeries(const std::vector<double>& values, const std::string& title = "series");

    /**
     * @brief Two-panel comparison plot from raw vectors.
     *
     * @param analysed  Analysed series values.
     * @param reference Reference series values (must be the same length).
     * @param title     Plot title and output filename stem.
     * @throws DataException if vectors are empty or have different sizes.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void comparison(const std::vector<double>& analysed,
                    const std::vector<double>& reference,
                    const std::string& title = "comparison");

    /**
     * @brief ACF plot from a raw value vector.
     *
     * @param values Vector of values.
     * @param maxLag Maximum lag.
     * @param title  Output filename stem.
     * @throws DataException if values is too short.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void acf(const std::vector<double>& values,
             int maxLag = 40,
             const std::string& title = "acf");

    /**
     * @brief Histogram from a raw value vector.
     *
     * @param values Vector of values.
     * @param bins   Number of bins.
     * @param title  Output filename stem.
     * @throws DataException if values is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void histogram(const std::vector<double>& values,
                   int bins = 30,
                   const std::string& title = "histogram");

    /**
     * @brief QQ-plot from a raw value vector.
     *
     * @param values Vector of values.
     * @param title  Output filename stem.
     * @throws DataException if values has fewer than 3 elements.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void qqPlot(const std::vector<double>& values, const std::string& title = "qqplot");

    /**
     * @brief Box plot from a raw value vector.
     *
     * @param values Vector of values.
     * @param title  Output filename stem.
     * @throws DataException if values is empty.
     * @throws IoException   on gnuplot or file I/O failure.
     */
    void boxplot(const std::vector<double>& values, const std::string& title = "boxplot");

private:

    AppConfig m_config;  ///< Full application configuration (copied).

    // ── Helpers ───────────────────────────────────────────────────────────────

    /**
     * @brief Returns the time format to use for x-axis labelling.
     *
     * If PlotConfig::timeFormat is non-empty it takes precedence; otherwise
     * InputConfig::timeFormat is used. Guaranteed to never return an empty string.
     */
    [[nodiscard]] TimeFormat effectiveTimeFormat() const noexcept;

    /**
     * @brief Builds the absolute output path for a plot file.
     *
     * @param stem Base filename without extension (e.g. "timeSeries_GRAZ_dN").
     * @return Absolute path: imgDir / (stem + "." + outputFormat).
     */
    [[nodiscard]] std::filesystem::path outputPath(const std::string& stem) const;

    /**
     * @brief Writes a two-column (x, y) data table to a temporary file.
     *
     * The temporary file is placed in imgDir with a leading dot so it is
     * visually separated from output images. The caller is responsible for
     * deleting it after use (see removeTempFile()).
     *
     * @param stem Temporary filename stem (e.g. ".tmp_timeseries").
     * @param data Column pairs: first = x, second = y.
     * @return Absolute path to the written file.
     * @throws IoException if the file cannot be opened for writing.
     */
    [[nodiscard]] std::filesystem::path writeTempData(
        const std::string& stem,
        const std::vector<std::pair<double, double>>& data) const;

    /**
     * @brief Writes a multi-column data table to a temporary file.
     *
     * Each inner vector is one column. All columns must have the same size.
     *
     * @param stem    Temporary filename stem.
     * @param columns Data columns (first column is typically x / index).
     * @return Absolute path to the written file.
     * @throws IoException if the file cannot be opened or column sizes differ.
     */
    [[nodiscard]] std::filesystem::path writeTempDataMulti(
        const std::string& stem,
        const std::vector<std::vector<double>>& columns) const;

    /**
     * @brief Deletes a temporary data file, ignoring any errors.
     *
     * @param path Path to the file to remove.
     */
    static void removeTempFile(const std::filesystem::path& path) noexcept;

    /**
     * @brief Extracts valid (non-NaN) values from a TimeSeries as a vector.
     *
     * @param ts Source time series.
     * @return Vector of (x, y) pairs where x is chosen by effectiveTimeFormat().
     * @throws DataException if ts is empty.
     */
    [[nodiscard]] std::vector<std::pair<double, double>>
    toXY(const TimeSeries& ts) const;

    /**
     * @brief Formats a gnuplot terminal line for the configured output format.
     *
     * @param widthPx  Image width in pixels.
     * @param heightPx Image height in pixels.
     * @return Full gnuplot "set terminal ..." command string.
     */
    [[nodiscard]] std::string terminalCmd(int widthPx = 1200, int heightPx = 600) const;

    /**
     * @brief Returns a gnuplot x-axis time format string if UTC display is active.
     *
     * Returns an empty string for non-time x-axes (INDEX, MJD, etc.).
     */
    [[nodiscard]] std::string gnuplotTimeFmt() const;

    /**
     * @brief Computes ACF values up to maxLag from a vector of values.
     *
     * Missing (NaN) values are skipped.
     *
     * @param values Input data.
     * @param maxLag Maximum lag.
     * @return Vector of (lag, acf) pairs.
     * @throws DataException if values is too short.
     */
    [[nodiscard]] static std::vector<std::pair<double, double>>
    computeAcf(const std::vector<double>& values, int maxLag);

    /**
     * @brief Computes theoretical normal quantiles for a QQ-plot.
     *
     * @param values Input data (NaN values are ignored).
     * @return Vector of (theoretical_quantile, sample_quantile) pairs, sorted.
     * @throws DataException if fewer than 3 valid values are present.
     */
    [[nodiscard]] static std::vector<std::pair<double, double>>
    computeQQ(const std::vector<double>& values);

    /**
     * @brief Extracts valid (non-NaN) values from a vector.
     */
    [[nodiscard]] static std::vector<double>
    validValues(const std::vector<double>& values);
};

} // namespace loki
