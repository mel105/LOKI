#include "loki/io/plot.hpp"
#include "loki/io/gnuplot.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

using namespace loki;

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr int    DEFAULT_WIDTH_PX  = 1200;
static constexpr int    DEFAULT_HEIGHT_PX =  600;
static constexpr int    COMPARE_HEIGHT_PX =  900;
static constexpr double CONF_95_COEFF     = 1.96;

static std::string makeStem(const std::string& dataset,
                             const std::string& parameter,
                             const std::string& plotType)
{
    return "core_" + dataset + "_" + parameter + "_" + plotType;
}

// ── Construction ──────────────────────────────────────────────────────────────

Plot::Plot(const AppConfig& config)
    : m_config(config)
{
    std::filesystem::create_directories(m_config.imgDir);
}

// ── ACF lag helper ────────────────────────────────────────────────────────────

// Returns effective ACF max lag:
//   - If config specifies > 0, use that.
//   - Otherwise: min(N/10, 200), minimum 10.
static int effectiveAcfLag(int configLag, std::size_t n)
{
    if (configLag > 0) return configLag;
    const int autoLag = static_cast<int>(n) / 10;
    return std::max(10, std::min(autoLag, 200));
}

// ── Histogram bins helper ─────────────────────────────────────────────────────

// Returns effective bin count:
//   - If config specifies > 0, use that.
//   - Otherwise: Sturges rule ceil(log2(N) + 1), clamped to [10, 100].
static int effectiveHistBins(int configBins, std::size_t n)
{
    if (configBins > 0) return configBins;
    const int sturges = static_cast<int>(std::ceil(std::log2(static_cast<double>(n)) + 1.0));
    return std::max(10, std::min(sturges, 100));
}

// ── TimeSeries overloads ──────────────────────────────────────────────────────

void Plot::timeSeries(const TimeSeries& ts, const std::string& title)
{
    if (ts.empty()) throw DataException("Plot::timeSeries(): time series is empty.");

    const auto& meta = ts.metadata();
    const std::string t = title.empty()
        ? (meta.stationId + " - " + meta.componentName) : title;

    const std::string dataset = m_config.input.file.stem().string().empty()
        ? "data" : m_config.input.file.stem().string();
    const std::string stem = makeStem(dataset,
        meta.componentName.empty() ? meta.stationId : meta.componentName, "timeseries");
    const auto dataFile = writeTempData(".tmp_" + stem, toXY(ts));

    const std::string yLabel = meta.componentName.empty() ? "value"
        : (meta.unit.empty() || meta.unit == "-") ? meta.componentName
        : (meta.componentName + " [" + meta.unit + "]");

    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, DEFAULT_HEIGHT_PX));
        gp("set output '" + outputPath(stem).string() + "'");
        gp("set title '"  + t + "'");
        gp("set xlabel 'Time'");
        gp("set ylabel '" + yLabel + "'");
        gp("set key off");
        gp("set grid");
        gp("set style line 1 lc rgb '#2166ac' lw 1.5");

        const std::string timeFmt = gnuplotTimeFmt();
        if (!timeFmt.empty()) {
            gp("set xdata time");
            gp("set timefmt '" + timeFmt + "'");
            gp("set format x '%Y-%m'");
            gp("set xtics rotate by -45");
        }
        gp("plot '" + dataFile.string() + "' u 1:2 notitle w l ls 1");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::comparison(const TimeSeries& analysed, const TimeSeries& reference,
                      const std::string& title)
{
    if (analysed.empty())  throw DataException("Plot::comparison(): analysed series is empty.");
    if (reference.empty()) throw DataException("Plot::comparison(): reference series is empty.");

    const auto& meta = analysed.metadata();
    const std::string t = title.empty() ? ("Comparison - " + meta.stationId) : title;
    const std::string dataset = m_config.input.file.stem().string().empty()
        ? "data" : m_config.input.file.stem().string();
    const std::string stem = makeStem(dataset,
        meta.componentName.empty() ? meta.stationId : meta.componentName, "comparison");

    const auto xy_a = toXY(analysed);
    const auto xy_r = toXY(reference);
    const std::size_t nValid = std::min(xy_a.size(), xy_r.size());

    std::vector<std::vector<double>> cols(3);
    for (std::size_t i = 0; i < nValid; ++i) {
        cols[0].push_back(xy_a[i].first);
        cols[1].push_back(xy_a[i].second);
        cols[2].push_back(xy_r[i].second);
    }
    const auto dataFile = writeTempDataMulti(".tmp_" + stem, cols);

    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, COMPARE_HEIGHT_PX));
        gp("set output '" + outputPath(stem).string() + "'");
        gp("set multiplot layout 2,1 title '" + t + "'");
        gp("set tmargin 2"); gp("set bmargin 0");
        gp("unset xlabel"); gp("set ylabel 'value'");
        gp("set key top right"); gp("set grid");
        const std::string timeFmt = gnuplotTimeFmt();
        if (!timeFmt.empty()) {
            gp("set xdata time"); gp("set timefmt '" + timeFmt + "'");
            gp("set format x '%Y-%m'");
        }
        gp("plot '" + dataFile.string() + "' u 1:2 t 'Analysed'  w l lc rgb '#2166ac' lw 1.5, "
           "''   u 1:3 t 'Reference' w l lc rgb '#d73027' lw 1.5");
        gp("set tmargin 0"); gp("set bmargin 3");
        gp("set xlabel 'Time'"); gp("set ylabel 'Difference'"); gp("set key off");
        gp("plot '" + dataFile.string() + "' u 1:($2-$3) notitle w l lc rgb '#4dac26' lw 1.5");
        gp("unset multiplot");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::acf(const TimeSeries& ts, int /*maxLag*/)
{
    if (ts.empty()) throw DataException("Plot::acf(): time series is empty.");

    std::vector<double> vals;
    vals.reserve(ts.size());
    for (const auto& obs : ts) if (isValid(obs)) vals.push_back(obs.value);

    const auto& meta = ts.metadata();
    const std::string dataset = m_config.input.file.stem().string().empty()
        ? "data" : m_config.input.file.stem().string();
    const std::string stem = makeStem(dataset,
        meta.componentName.empty() ? meta.stationId : meta.componentName, "acf");

    // Use config-driven or auto lag, ignore caller's default if config is set.
    //const int lag = effectiveAcfLag(
    //    m_config.plots.options.acfMaxLag > 0
    //        ? m_config.plots.options.acfMaxLag : maxLag,
    //    vals.size());
    const int lag = effectiveAcfLag(m_config.plots.options.acfMaxLag, vals.size());
    acf(vals, lag, stem);
}

void Plot::histogram(const TimeSeries& ts, int bins)
{
    if (ts.empty()) throw DataException("Plot::histogram(): time series is empty.");

    std::vector<double> vals;
    vals.reserve(ts.size());
    for (const auto& obs : ts) if (isValid(obs)) vals.push_back(obs.value);

    const auto& meta = ts.metadata();
    const std::string dataset = m_config.input.file.stem().string().empty()
        ? "data" : m_config.input.file.stem().string();
    const std::string stem = makeStem(dataset,
        meta.componentName.empty() ? meta.stationId : meta.componentName, "histogram");

    const int b = effectiveHistBins(
        m_config.plots.options.histogramBins > 0
            ? m_config.plots.options.histogramBins : bins,
        vals.size());
    histogram(vals, b, stem);
}

void Plot::qqPlot(const TimeSeries& ts)
{
    if (ts.empty()) throw DataException("Plot::qqPlot(): time series is empty.");

    std::vector<double> vals;
    vals.reserve(ts.size());
    for (const auto& obs : ts) if (isValid(obs)) vals.push_back(obs.value);

    const auto& meta = ts.metadata();
    const std::string dataset = m_config.input.file.stem().string().empty()
        ? "data" : m_config.input.file.stem().string();
    const std::string stem = makeStem(dataset,
        meta.componentName.empty() ? meta.stationId : meta.componentName, "qqplot");
    qqPlot(vals, stem);
}

void Plot::boxplot(const TimeSeries& ts)
{
    if (ts.empty()) throw DataException("Plot::boxplot(): time series is empty.");

    std::vector<double> vals;
    vals.reserve(ts.size());
    for (const auto& obs : ts) if (isValid(obs)) vals.push_back(obs.value);

    const auto& meta = ts.metadata();
    const std::string dataset = m_config.input.file.stem().string().empty()
        ? "data" : m_config.input.file.stem().string();
    const std::string stem = makeStem(dataset,
        meta.componentName.empty() ? meta.stationId : meta.componentName, "boxplot");
    boxplot(vals, stem);
}

// ── Raw-vector overloads ──────────────────────────────────────────────────────

void Plot::timeSeries(const std::vector<double>& values, const std::string& title)
{
    if (values.empty()) throw DataException("Plot::timeSeries(): values vector is empty.");

    std::vector<std::pair<double, double>> xy;
    xy.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
        xy.emplace_back(static_cast<double>(i), values[i]);

    const auto dataFile = writeTempData(".tmp_" + title, xy);
    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, DEFAULT_HEIGHT_PX));
        gp("set output '" + outputPath(title).string() + "'");
        gp("set title '"  + title + "'");
        gp("set xlabel 'Index'"); gp("set ylabel 'Value'");
        gp("set key off"); gp("set grid");
        gp("plot '" + dataFile.string() + "' u 1:2 notitle w l lc rgb '#2166ac' lw 1.5");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::comparison(const std::vector<double>& analysed,
                      const std::vector<double>& reference, const std::string& title)
{
    if (analysed.empty())  throw DataException("Plot::comparison(): analysed vector is empty.");
    if (reference.empty()) throw DataException("Plot::comparison(): reference vector is empty.");
    if (analysed.size() != reference.size())
        throw DataException("Plot::comparison(): vectors must be the same size.");

    const std::size_t n = analysed.size();
    std::vector<std::vector<double>> cols(3, std::vector<double>(n));
    for (std::size_t i = 0; i < n; ++i) {
        cols[0][i] = static_cast<double>(i);
        cols[1][i] = analysed[i];
        cols[2][i] = reference[i];
    }
    const auto dataFile = writeTempDataMulti(".tmp_" + title, cols);
    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, COMPARE_HEIGHT_PX));
        gp("set output '" + outputPath(title).string() + "'");
        gp("set multiplot layout 2,1 title '" + title + "'");
        gp("set grid"); gp("set key top right");
        gp("plot '" + dataFile.string() + "' u 1:2 t 'Analysed'  w l lc rgb '#2166ac' lw 1.5, "
           "''   u 1:3 t 'Reference' w l lc rgb '#d73027' lw 1.5");
        gp("set key off");
        gp("plot '" + dataFile.string() + "' u 1:($2-$3) t 'Difference' w l lc rgb '#4dac26' lw 1.5");
        gp("unset multiplot");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::acf(const std::vector<double>& values, int maxLag, const std::string& title)
{
    const auto valid = validValues(values);
    const std::size_t n = valid.size();
    const int lag = effectiveAcfLag(maxLag, n);

    if (static_cast<int>(n) < lag + 2) {
        throw DataException(
            "Plot::acf(): series too short for maxLag=" + std::to_string(lag) +
            " (need " + std::to_string(lag + 2) + " valid observations, got " +
            std::to_string(n) + ").");
    }

    const auto acfData  = computeAcf(valid, lag);
    const auto dataFile = writeTempData(".tmp_" + title, acfData);
    const double conf   = CONF_95_COEFF / std::sqrt(static_cast<double>(n));

    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, DEFAULT_HEIGHT_PX));
        gp("set output '" + outputPath(title).string() + "'");
        gp("set title 'Autocorrelation Function'");
        gp("set xlabel 'Lag'"); gp("set ylabel 'ACF'");
        gp("set yrange [-1:1]"); gp("set ytics 0.2");
        gp("set key off"); gp("set grid");

        const std::string confStr = std::to_string(conf);
        gp("set arrow 1 from graph 0, first  " + confStr +
           " to graph 1, first  " + confStr + " nohead lc rgb '#999999' lt 2");
        gp("set arrow 2 from graph 0, first -" + confStr +
           " to graph 1, first -" + confStr + " nohead lc rgb '#999999' lt 2");

        gp("plot '" + dataFile.string() + "' u 1:2 notitle w impulses lc rgb '#2166ac' lw 3");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::histogram(const std::vector<double>& values, int bins, const std::string& title)
{
    const auto valid = validValues(values);
    if (valid.empty()) throw DataException("Plot::histogram(): no valid values.");

    const int b = effectiveHistBins(bins, valid.size());

    const double minVal = *std::min_element(valid.begin(), valid.end());
    const double maxVal = *std::max_element(valid.begin(), valid.end());
    const double mean   = std::accumulate(valid.begin(), valid.end(), 0.0) /
                          static_cast<double>(valid.size());
    double variance = 0.0;
    for (const double v : valid) variance += (v - mean) * (v - mean);
    const double sigma = std::sqrt(variance / static_cast<double>(valid.size()));

    std::vector<std::pair<double, double>> raw;
    raw.reserve(valid.size());
    for (const double v : valid) raw.emplace_back(v, 0.0);
    const auto dataFile = writeTempData(".tmp_" + title, raw);

    const std::string binsStr = std::to_string(b);
    const std::string minStr  = std::to_string(minVal);
    const std::string maxStr  = std::to_string(maxVal);
    const std::string meanStr = std::to_string(mean);
    const std::string sigStr  = std::to_string(sigma);
    const std::string nStr    = std::to_string(valid.size());

    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, DEFAULT_HEIGHT_PX));
        gp("set output '" + outputPath(title).string() + "'");
        gp("set title 'Histogram'");
        gp("set xlabel 'Value'"); gp("set ylabel 'Count'");
        gp("set key top right"); gp("set grid");
        gp("n = " + binsStr);
        gp("min = " + minStr); gp("max = " + maxStr);
        gp("width = (max - min) / n");
        gp("hist(x, w) = w * floor(x / w) + w / 2.0");
        gp("set boxwidth width * 0.9");
        gp("set style fill solid 0.6 border -1");
        gp("gauss(x) = (" + nStr + " * width) * "
           "exp(-0.5 * ((x - " + meanStr + ") / " + sigStr + ")**2) / "
           "(" + sigStr + " * sqrt(2.0 * pi))");
        gp("plot '" + dataFile.string() + "' u (hist($1, width)):(1.0) smooth freq "
           "w boxes lc rgb '#2166ac' t 'Histogram', "
           "gauss(x) w l lc rgb '#d73027' lw 2 t 'Normal fit'");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::qqPlot(const std::vector<double>& values, const std::string& title)
{
    const auto qqData   = computeQQ(values);
    const auto dataFile = writeTempData(".tmp_" + title, qqData);

    // Reference line through Q1 and Q3 of the data (standard QQ line).
    // Find sample Q1 and Q3 and their corresponding theoretical quantiles.
    // qqData is sorted by theoretical quantile (x).
    const std::size_t n = qqData.size();
    const std::size_t i1 = n / 4;
    const std::size_t i3 = 3 * n / 4;
    const double x1 = qqData[i1].first,  y1 = qqData[i1].second;
    const double x3 = qqData[i3].first,  y3 = qqData[i3].second;

    // Line: y = slope * x + intercept
    const double slope     = (y3 - y1) / (x3 - x1 + 1e-12);
    const double intercept = y1 - slope * x1;

    const double xMin = qqData.front().first;
    const double xMax = qqData.back().first;

    // Two points for the reference line
    const double yAtMin = slope * xMin + intercept;
    const double yAtMax = slope * xMax + intercept;

    std::ostringstream refLine;
    refLine << std::fixed;
    refLine << "set arrow 1 from " << xMin << "," << yAtMin
            << " to " << xMax << "," << yAtMax
            << " nohead lc rgb '#d73027' lw 2";

    try {
        Gnuplot gp;
        gp(terminalCmd(DEFAULT_WIDTH_PX, DEFAULT_HEIGHT_PX));
        gp("set output '" + outputPath(title).string() + "'");
        gp("set title 'Normal Q-Q Plot'");
        gp("set xlabel 'Theoretical Quantiles'");
        gp("set ylabel 'Sample Quantiles'");
        gp("set key top left"); gp("set grid");
        gp(refLine.str());
        gp("plot '" + dataFile.string() + "' u 1:2 t 'Data' w p pt 7 ps 0.4 lc rgb '#2166ac', "
           "1/0 t 'Normal line' w l lc rgb '#d73027' lw 2");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

void Plot::boxplot(const std::vector<double>& values, const std::string& title)
{
    const auto valid = validValues(values);
    if (valid.empty()) throw DataException("Plot::boxplot(): no valid values.");

    const std::size_t n = valid.size();
    auto sorted = valid;
    std::sort(sorted.begin(), sorted.end());

    const double minVal  = sorted.front();
    const double maxVal  = sorted.back();
    const double mean    = std::accumulate(sorted.begin(), sorted.end(), 0.0) /
                           static_cast<double>(n);
    const double median  = (n % 2 == 0)
        ? 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]) : sorted[n / 2];
    const double q1      = sorted[n / 4];
    const double q3      = sorted[3 * n / 4];
    const double iqr     = q3 - q1;
    double variance = 0.0;
    for (const double v : sorted) variance += (v - mean) * (v - mean);
    const double sigma = std::sqrt(variance / static_cast<double>(n));

    // Whisker ends (Tukey fences, clamped to data range).
    const double wLow  = std::max(minVal, q1 - 1.5 * iqr);
    const double wHigh = std::min(maxVal, q3 + 1.5 * iqr);

    // Write minimal dat -- just needed for gnuplot boxplot engine.
    // We draw the box manually via objects + arrows for clean output.
    const auto dataFile = writeTempData(".tmp_" + title,
        std::vector<std::pair<double,double>>{{0.0, median}});

    auto fmt = [](double v, int prec = 4) -> std::string {
        std::ostringstream ss;
        ss.precision(prec); ss << std::fixed << v;
        return ss.str();
    };

    // Build stats table string for gnuplot label (use \n for newline in label).
    const std::string statsLabel =
        "n = "       + std::to_string(n)   + "\\n"
        "mean = "    + fmt(mean)           + "\\n"
        "median = "  + fmt(median)         + "\\n"
        "sd = "      + fmt(sigma)          + "\\n"
        "min = "     + fmt(minVal)         + "\\n"
        "max = "     + fmt(maxVal)         + "\\n"
        "Q1 = "      + fmt(q1)             + "\\n"
        "Q3 = "      + fmt(q3)             + "\\n"
        "IQR = "     + fmt(iqr);

    // Use multiplot: left panel = box, right panel = stats table.
    try {
        Gnuplot gp;
        gp(terminalCmd(600, DEFAULT_HEIGHT_PX));
        gp("set output '" + outputPath(title).string() + "'");

        const std::string titleStr =
            "n="      + std::to_string(n)
            + "  mean=" + fmt(mean)
            + "  median=" + fmt(median)
            + "  sd=" + fmt(sigma)
            + "  min=" + fmt(minVal)
            + "  max=" + fmt(maxVal);

        gp("set title 'Box Plot: " + titleStr + "'");
        gp("unset xtics");
        gp("set ylabel 'Value'");
        gp("set key off");
        gp("set grid ytics");
        gp("set yrange [" + fmt(minVal - 0.05*(maxVal-minVal), 8) + ":"
                        + fmt(maxVal + 0.05*(maxVal-minVal), 8) + "]");
        gp("set xrange [-1:1]");

        gp("set object 1 rect from -0.4," + fmt(q1,8) +
        " to 0.4," + fmt(q3,8) +
        " fc rgb '#a6cee3' fs solid 0.7 border lc rgb '#2166ac' lw 1.5");
        gp("set arrow 1 from -0.4," + fmt(median,8) +
        " to 0.4," + fmt(median,8) +
        " nohead lc rgb '#2166ac' lw 2.5");
        gp("set arrow 2 from 0," + fmt(q1,8) +
        " to 0," + fmt(wLow,8) +
        " nohead lc rgb '#2166ac' lw 1.5 dt 2");
        gp("set arrow 3 from -0.15," + fmt(wLow,8) +
        " to 0.15," + fmt(wLow,8) +
        " nohead lc rgb '#2166ac' lw 1.5");
        gp("set arrow 4 from 0," + fmt(q3,8) +
        " to 0," + fmt(wHigh,8) +
        " nohead lc rgb '#2166ac' lw 1.5 dt 2");
        gp("set arrow 5 from -0.15," + fmt(wHigh,8) +
        " to 0.15," + fmt(wHigh,8) +
        " nohead lc rgb '#2166ac' lw 1.5");

        gp("plot 1/0 notitle");
    } catch (...) { removeTempFile(dataFile); throw; }
    removeTempFile(dataFile);
}

// ── Private helpers ───────────────────────────────────────────────────────────

TimeFormat Plot::effectiveTimeFormat() const noexcept
{
    if (!m_config.plots.timeFormat.empty()) {
        const auto& s = m_config.plots.timeFormat;
        if (s == "utc")          return TimeFormat::UTC;
        if (s == "mjd")          return TimeFormat::MJD;
        if (s == "unix")         return TimeFormat::UNIX;
        if (s == "gpst_seconds") return TimeFormat::GPS_TOTAL_SECONDS;
        if (s == "gps_week_sow") return TimeFormat::GPS_WEEK_SOW;
        if (s == "index")        return TimeFormat::INDEX;
    }
    return m_config.input.timeFormat;
}

std::filesystem::path Plot::outputPath(const std::string& stem) const
{
    std::string safe = stem;
    for (char& c : safe) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|' || c == '[' || c == ']') c = '_';
    }
    return m_config.imgDir / (safe + "." + m_config.plots.outputFormat);
}

std::filesystem::path Plot::writeTempData(
    const std::string& stem,
    const std::vector<std::pair<double, double>>& data) const
{
    const auto path = m_config.imgDir / stem;
    std::ofstream f(path);
    if (!f.is_open())
        throw IoException("Plot::writeTempData(): cannot open '" + path.string() + "'.");
    f.precision(12);
    for (const auto& [x, y] : data) f << x << "\t" << y << "\n";
    return path;
}

std::filesystem::path Plot::writeTempDataMulti(
    const std::string& stem,
    const std::vector<std::vector<double>>& columns) const
{
    if (columns.empty())
        throw DataException("Plot::writeTempDataMulti(): no columns provided.");
    const std::size_t rows = columns[0].size();
    for (const auto& col : columns)
        if (col.size() != rows)
            throw DataException("Plot::writeTempDataMulti(): column size mismatch.");

    const auto path = m_config.imgDir / stem;
    std::ofstream f(path);
    if (!f.is_open())
        throw IoException("Plot::writeTempDataMulti(): cannot open '" + path.string() + "'.");
    f.precision(12);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < columns.size(); ++c) {
            f << columns[c][r];
            if (c + 1 < columns.size()) f << "\t";
        }
        f << "\n";
    }
    return path;
}

void Plot::removeTempFile(const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

std::vector<std::pair<double, double>> Plot::toXY(const TimeSeries& ts) const
{
    if (ts.empty()) throw DataException("Plot::toXY(): time series is empty.");

    const TimeFormat fmt = effectiveTimeFormat();
    std::vector<std::pair<double, double>> xy;
    xy.reserve(ts.size());

    for (const auto& obs : ts) {
        if (!isValid(obs)) continue;
        double x = 0.0;
        switch (fmt) {
            case TimeFormat::MJD:               x = obs.time.mjd();      break;
            case TimeFormat::UNIX:              x = obs.time.unixTime(); break;
            case TimeFormat::GPS_TOTAL_SECONDS:
            case TimeFormat::GPS_WEEK_SOW:
            case TimeFormat::UTC:               x = obs.time.mjd();      break;
            case TimeFormat::INDEX: default:    x = static_cast<double>(xy.size()); break;
        }
        xy.emplace_back(x, obs.value);
    }
    return xy;
}

std::string Plot::terminalCmd(int widthPx, int heightPx) const
{
    const std::string& fmt = m_config.plots.outputFormat;
    std::ostringstream ss;
    if (fmt == "eps") {
        ss << "set terminal postscript eps color solid 'Sans,12' size "
           << widthPx / 28 << "cm," << heightPx / 28 << "cm";
    } else if (fmt == "svg") {
        ss << "set terminal svg size " << widthPx << "," << heightPx << " font 'Sans,12'";
    } else {
        ss << "set terminal pngcairo size " << widthPx << "," << heightPx
           << " noenhanced font 'Sans,12'";
    }
    return ss.str();
}

std::string Plot::gnuplotTimeFmt() const
{
    if (effectiveTimeFormat() == TimeFormat::UTC) return "%s";
    return {};
}

std::vector<std::pair<double, double>>
Plot::computeAcf(const std::vector<double>& values, int maxLag)
{
    const std::size_t n = values.size();
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                        static_cast<double>(n);
    double var = 0.0;
    for (const double v : values) var += (v - mean) * (v - mean);
    if (var == 0.0) throw AlgorithmException("Plot::computeAcf(): series has zero variance.");

    std::vector<std::pair<double, double>> result;
    result.reserve(static_cast<std::size_t>(maxLag) + 1);
    for (int lag = 0; lag <= maxLag; ++lag) {
        double cov = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < n; ++i)
            cov += (values[i] - mean) * (values[i + static_cast<std::size_t>(lag)] - mean);
        result.emplace_back(static_cast<double>(lag), cov / var);
    }
    return result;
}

std::vector<std::pair<double, double>>
Plot::computeQQ(const std::vector<double>& values)
{
    const auto valid = validValues(values);
    if (valid.size() < 3)
        throw DataException("Plot::computeQQ(): need at least 3 valid observations, got "
                            + std::to_string(valid.size()) + ".");

    auto sorted = valid;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();

    auto normalQuantile = [](double p) -> double {
        static constexpr double C0=2.515517, C1=0.802853, C2=0.010328;
        static constexpr double D1=1.432788, D2=0.189269, D3=0.001308;
        const bool upper = (p > 0.5);
        const double q = upper ? (1.0 - p) : p;
        const double t = std::sqrt(-2.0 * std::log(q));
        const double z = t - (C0 + t*(C1 + t*C2)) / (1.0 + t*(D1 + t*(D2 + t*D3)));
        return upper ? z : -z;
    };

    std::vector<std::pair<double, double>> result;
    result.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double p  = (static_cast<double>(i) + 1.0 - 0.375) /
                          (static_cast<double>(n) + 0.25);
        result.emplace_back(normalQuantile(p), sorted[i]);
    }
    return result;
}

std::vector<double> Plot::validValues(const std::vector<double>& values)
{
    std::vector<double> out;
    out.reserve(values.size());
    for (const double v : values) if (v == v) out.push_back(v);
    return out;
}