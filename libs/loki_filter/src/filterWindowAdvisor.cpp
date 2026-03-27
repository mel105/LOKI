#include <loki/filter/filterWindowAdvisor.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/stats/descriptive.hpp>
#include <loki/timeseries/timeSeries.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace loki;

static constexpr double MAD_TO_SIGMA = 1.0 / 0.6745;
static constexpr double SILVERMAN_C  = 0.9;

int FilterWindowAdvisor::toOdd(int v, int minVal)
{
    int result = std::max(v, minVal);
    if (result % 2 == 0) result += 1;
    return result;
}

int FilterWindowAdvisor::silvermanMad(const std::vector<double>& values,
                                       int n,
                                       std::string& rationale)
{
    const double madVal = loki::stats::mad(values, loki::NanPolicy::SKIP);
    const double sigmaH = madVal * MAD_TO_SIGMA;
    const double bwRaw  = sigmaH * std::pow(4.0 / (3.0 * static_cast<double>(n)), 0.2);
    const int    window = toOdd(static_cast<int>(std::round(bwRaw)), 3);

    rationale = "SILVERMAN_MAD: MAD=" + std::to_string(madVal)
              + ", sigma_H=" + std::to_string(sigmaH)
              + ", bw_raw=" + std::to_string(bwRaw)
              + " -> window=" + std::to_string(window) + " samples.";
    return window;
}

int FilterWindowAdvisor::silverman(const std::vector<double>& values,
                                    int n,
                                    std::string& rationale)
{
    const double stdVal = loki::stats::stddev(values, false, loki::NanPolicy::SKIP);
    const double iqrVal = loki::stats::iqr(values, loki::NanPolicy::SKIP);
    const double sigma  = std::min(stdVal, iqrVal / 1.34);
    const double bwRaw  = SILVERMAN_C * sigma * std::pow(static_cast<double>(n), -0.2);
    const int    window = toOdd(static_cast<int>(std::round(bwRaw)), 3);

    rationale = "SILVERMAN: std=" + std::to_string(stdVal)
              + ", IQR/1.34=" + std::to_string(iqrVal / 1.34)
              + ", sigma=" + std::to_string(sigma)
              + ", bw_raw=" + std::to_string(bwRaw)
              + " -> window=" + std::to_string(window) + " samples.";
    return window;
}

int FilterWindowAdvisor::acfPeak(const std::vector<double>& values,
                                  int n,
                                  int maxLag,
                                  std::string& rationale)
{
    const int effectiveMaxLag = (maxLag <= 0)
        ? std::min(n / 10, 200)
        : std::min(maxLag, n - 1);

    if (effectiveMaxLag < 2) {
        throw ConfigException(
            "FilterWindowAdvisor: series too short for ACF_PEAK estimation "
            "(need at least 20 observations).");
    }

    const std::vector<double> acfVals =
        loki::stats::acf(values, effectiveMaxLag, loki::NanPolicy::SKIP);

    int peakLag = -1;
    for (int k = 2; k < effectiveMaxLag; ++k) {
        if (acfVals[static_cast<std::size_t>(k)]     > acfVals[static_cast<std::size_t>(k - 1)] &&
            acfVals[static_cast<std::size_t>(k)]     > acfVals[static_cast<std::size_t>(k + 1)] &&
            acfVals[static_cast<std::size_t>(k)]     > 0.0) {
            peakLag = k;
            break;
        }
    }

    if (peakLag < 0) {
        throw ConfigException(
            "FilterWindowAdvisor: ACF_PEAK found no significant local maximum "
            "within maxLag=" + std::to_string(effectiveMaxLag) +
            ". Consider increasing acfMaxLag or switching to SILVERMAN_MAD.");
    }

    const int window = toOdd(peakLag, 3);

    rationale = "ACF_PEAK: first ACF local maximum at lag=" + std::to_string(peakLag)
              + " (acf=" + std::to_string(acfVals[static_cast<std::size_t>(peakLag)]) + ")"
              + " -> window=" + std::to_string(window) + " samples.";
    return window;
}

FilterWindowAdvisor::Advice FilterWindowAdvisor::advise(const TimeSeries& series,
                                                         const Config&     cfg)
{
    const int n = static_cast<int>(series.size());
    if (n == 0) {
        throw DataException("FilterWindowAdvisor::advise: input series is empty.");
    }

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(n));
    for (std::size_t i = 0; i < series.size(); ++i)
        values.push_back(series[i].value);

    std::string rationale;
    int window = 3;

    switch (cfg.method) {
        case Method::SILVERMAN_MAD:
            window = silvermanMad(values, n, rationale);
            break;
        case Method::SILVERMAN:
            window = silverman(values, n, rationale);
            break;
        case Method::ACF_PEAK:
            window = acfPeak(values, n, cfg.acfMaxLag, rationale);
            break;
    }

    const int minW = (cfg.minWindow % 2 == 0) ? cfg.minWindow + 1 : cfg.minWindow;
    window = toOdd(std::max(window, minW), minW);

    const double bandwidth = static_cast<double>(window) / static_cast<double>(n);

    return Advice{window, bandwidth, std::move(rationale)};
}
