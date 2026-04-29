#include <loki/geodesy/plotGeodesy.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>


#include <Eigen/Dense>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using namespace loki::geodesy;

namespace {

// Forward slash helper for gnuplot on Windows
std::string fwdSlash(const std::string& p)
{
    std::string r = p;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

// Linspace helper
std::vector<double> linspace(double a, double b, int n)
{
    std::vector<double> v(static_cast<std::size_t>(n));
    double step = (b - a) / (n - 1);
    for (int i = 0; i < n; ++i) v[static_cast<std::size_t>(i)] = a + i * step;
    return v;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

PlotGeodesy::PlotGeodesy(const GeodesyConfig& cfg, const std::string& datasetId)
    : m_cfg(cfg)
    , m_datasetId(datasetId)
    , m_outDir(cfg.outputDir + "IMG/")
{
    std::filesystem::create_directories(m_outDir);
}

// ---------------------------------------------------------------------------
// chi2 quantile for 2 dof
// ---------------------------------------------------------------------------

double PlotGeodesy::chi2Quantile2dof(double prob) noexcept
{
    // Exact closed form: chi2(2) = -2 ln(1 - prob)
    return -2.0 * std::log(1.0 - prob);
}

// ---------------------------------------------------------------------------
// outPath
// ---------------------------------------------------------------------------

std::string PlotGeodesy::outPath(const std::string& tag,
                                  const std::string& suffix) const
{
    return m_outDir + "geodesy_" + m_datasetId + "_" + tag + "_" + suffix + ".png";
}

// ---------------------------------------------------------------------------
// computeErrorEllipse
// ---------------------------------------------------------------------------

void PlotGeodesy::computeErrorEllipse(const Eigen::Matrix2d& cov2x2,
                                       double                  prob,
                                       std::vector<double>&    xEll,
                                       std::vector<double>&    yEll,
                                       std::vector<double>&    xHelm,
                                       std::vector<double>&    yHelm)
{
    // Eigendecomposition
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eig(cov2x2);
    Eigen::Vector2d eigVals = eig.eigenvalues();
    Eigen::Matrix2d eigVecs = eig.eigenvectors();

    // Larger eigenvalue index
    int   iMax   = (eigVals(0) >= eigVals(1)) ? 0 : 1;
    int   iMin   = 1 - iMax;
    double lMax  = eigVals(iMax);
    double lMin  = eigVals(iMin);

    // Scale by sqrt of chi2 quantile
    double scale = std::sqrt(chi2Quantile2dof(prob));
    double a = scale * std::sqrt(lMax);   // semi-major
    double b = scale * std::sqrt(lMin);   // semi-minor

    // Rotation angle of major axis
    Eigen::Vector2d vMax = eigVecs.col(iMax);
    double theta = std::atan2(vMax(1), vMax(0));

    constexpr int   NPTS   = 360;
    auto            t      = linspace(0.0, 2.0 * std::numbers::pi, NPTS + 1);

    double cosT = std::cos(theta);
    double sinT = std::sin(theta);

    xEll.resize(static_cast<std::size_t>(NPTS + 1));
    yEll.resize(static_cast<std::size_t>(NPTS + 1));
    xHelm.resize(static_cast<std::size_t>(NPTS + 1));
    yHelm.resize(static_cast<std::size_t>(NPTS + 1));

    double e2 = a * a - b * b;

    for (int i = 0; i <= NPTS; ++i) {
        std::size_t si = static_cast<std::size_t>(i);
        double ct = std::cos(t[si]);
        double st = std::sin(t[si]);

        // Ellipse in principal axes, then rotate
        double ex = a * ct;
        double ey = b * st;
        xEll[si] = cosT * ex - sinT * ey;
        yEll[si] = sinT * ex + cosT * ey;

        // Helmert curve: rho = sqrt(e^2 cos^2(t) + b^2)
        double rho = std::sqrt(e2 * ct * ct + b * b);
        double hx  = rho * ct;
        double hy  = rho * st;
        xHelm[si] = cosT * hx - sinT * hy;
        yHelm[si] = sinT * hx + cosT * hy;
    }
}

// ---------------------------------------------------------------------------
// plotErrorEllipse
// ---------------------------------------------------------------------------

void PlotGeodesy::plotErrorEllipse(const Eigen::Matrix2d& cov2x2,
                                    double                  prob,
                                    const std::string&      labelX,
                                    const std::string&      labelY,
                                    const std::string&      tag) const
{
    std::vector<double> xEll, yEll, xHelm, yHelm;
    computeErrorEllipse(cov2x2, prob, xEll, yEll, xHelm, yHelm);

    std::string pngPath = fwdSlash(outPath(tag, "ellipse"));

    loki::Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 800,600");
    gp("set output '" + pngPath + "'");
    gp("set title 'Error ellipse & Helmert curve (" + tag + ")'");
    gp("set xlabel '" + labelX + "'");
    gp("set ylabel '" + labelY + "'");
    gp("set size ratio -1");
    gp("set key top right");
    gp("set grid");

    // Build inline data blocks
    std::ostringstream dsEll, dsHelm;
    dsEll  << "$ell << EOD\n";
    dsHelm << "$helm << EOD\n";
    for (std::size_t i = 0; i < xEll.size(); ++i) {
        dsEll  << xEll[i]  << " " << yEll[i]  << "\n";
        dsHelm << xHelm[i] << " " << yHelm[i] << "\n";
    }
    dsEll  << "EOD";
    dsHelm << "EOD";

    gp(dsEll.str());
    gp(dsHelm.str());

    gp("plot $ell  with lines lw 2 lc rgb '#0055aa' title 'Error ellipse', "
       "     $helm with lines lw 2 lc rgb '#cc4400' dt 2 title 'Helmert curve'");

    LOKI_INFO("PlotGeodesy: wrote " + pngPath);
}

// ---------------------------------------------------------------------------
// plotCovariancePanel
// ---------------------------------------------------------------------------

void PlotGeodesy::plotCovariancePanel(const MonteCarloResult&         mcResult,
                                       const Eigen::MatrixXd&          samples,
                                       const std::vector<std::string>& labels,
                                       double                           prob) const
{
    int N = static_cast<int>(mcResult.analyticalCov.rows());
    if (N < 2) return;

    // For each off-diagonal pair: scatter + error ellipse
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            Eigen::Matrix2d sub;
            sub << mcResult.analyticalCov(i, i), mcResult.analyticalCov(i, j),
                   mcResult.analyticalCov(j, i), mcResult.analyticalCov(j, j);

            std::string tag = labels[static_cast<std::size_t>(i)].substr(0, 1)
                            + labels[static_cast<std::size_t>(j)].substr(0, 1);

            std::string pngPath = fwdSlash(outPath(tag, "scatter"));

            std::vector<double> xEll, yEll, xHelm, yHelm;
            computeErrorEllipse(sub, prob, xEll, yEll, xHelm, yHelm);

            // Mean offsets for centering the ellipse on the empirical mean
            double mx = mcResult.empiricalMean(i);
            double my = mcResult.empiricalMean(j);

            loki::Gnuplot gp;
            gp("set terminal pngcairo noenhanced font 'Sans,12' size 700,700");
            gp("set output '" + pngPath + "'");
            gp("set title '" + labels[static_cast<std::size_t>(i)]
               + " vs " + labels[static_cast<std::size_t>(j)] + "'");
            gp("set xlabel '" + labels[static_cast<std::size_t>(i)] + "'");
            gp("set ylabel '" + labels[static_cast<std::size_t>(j)] + "'");
            gp("set size ratio -1");
            gp("set key top right");
            gp("set grid");

            // Sample scatter data
            std::ostringstream dsScatter, dsEll, dsHelm;
            dsScatter << "$scatter << EOD\n";
            for (int s = 0; s < samples.cols(); ++s)
                dsScatter << samples(i, s) << " " << samples(j, s) << "\n";
            dsScatter << "EOD";

            dsEll << "$ell << EOD\n";
            dsHelm << "$helm << EOD\n";
            for (std::size_t k = 0; k < xEll.size(); ++k) {
                dsEll  << (mx + xEll[k])  << " " << (my + yEll[k])  << "\n";
                dsHelm << (mx + xHelm[k]) << " " << (my + yHelm[k]) << "\n";
            }
            dsEll  << "EOD";
            dsHelm << "EOD";

            gp(dsScatter.str());
            gp(dsEll.str());
            gp(dsHelm.str());

            gp("plot $scatter with points pt 7 ps 0.4 lc rgb '#888888' title 'MC samples', "
               "     $ell     with lines lw 2 lc rgb '#0055aa' title 'Error ellipse', "
               "     $helm    with lines lw 2 lc rgb '#cc4400' dt 2 title 'Helmert curve'");

            LOKI_INFO("PlotGeodesy: wrote " + pngPath);
        }
    }
}

// ---------------------------------------------------------------------------
// plotDistances
// ---------------------------------------------------------------------------

void PlotGeodesy::plotDistances(const std::vector<GeodLineResult>& lines) const
{
    if (lines.empty()) return;

    std::string pngPath = fwdSlash(outPath("pairs", "distances"));

    loki::Gnuplot gp;
    gp("set terminal pngcairo noenhanced font 'Sans,12' size 900,500");
    gp("set output '" + pngPath + "'");
    gp("set title 'Geodesic distances between consecutive points'");
    gp("set xlabel 'Point pair index'");
    gp("set ylabel 'Distance [m]'");
    gp("set style data histograms");
    gp("set style fill solid 0.7 border -1");
    gp("set boxwidth 0.6");
    gp("set grid ytics");
    gp("set key off");

    std::ostringstream ds;
    ds << "$dist << EOD\n";
    for (std::size_t i = 0; i < lines.size(); ++i)
        ds << i << " " << lines[i].geodesic.distance << "\n";
    ds << "EOD";

    gp(ds.str());
    gp("plot $dist using 1:2 with boxes lc rgb '#0055aa'");

    LOKI_INFO("PlotGeodesy: wrote " + pngPath);
}
