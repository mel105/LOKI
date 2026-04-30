#include <loki/geodesy/plotGeodesy.hpp>
#include <loki/core/exceptions.hpp>
#include <loki/core/logger.hpp>
#include <loki/io/gnuplot.hpp>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <numbers>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace loki::geodesy;

// ---------------------------------------------------------------------------
// File-scope helpers
// ---------------------------------------------------------------------------

namespace {

// Forward-slash path for gnuplot on Windows
std::string fwdSlash(const std::string& p)
{
    std::string r = p;
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

// Evenly spaced vector [a, b] with n points
std::vector<double> linspace(double a, double b, int n)
{
    std::vector<double> v(static_cast<std::size_t>(n));
    if (n == 1) { v[0] = a; return v; }
    double step = (b - a) / static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t>(i)] = a + i * step;
    return v;
}

// Axis labels for known coordinate systems
std::vector<std::string> axisLabels(const std::string& sys)
{
    if (sys == "GEOD" || sys == "geod" || sys == "geodetic")
        return { "Lat [deg]", "Lon [deg]", "Elh [m]" };
    if (sys == "ENU" || sys == "enu")
        return { "E [m]", "N [m]", "U [m]" };
    if (sys == "SPHERE" || sys == "sphere")
        return { "Lat [deg]", "Lon [deg]", "R [m]" };
    return { "X [m]", "Y [m]", "Z [m]" };
}

// Format a double with enough precision for gnuplot data blocks.
// Uses scientific notation to avoid truncation of small values.
std::string fmt(double v)
{
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(10) << v;
    return oss.str();
}

// Pearson correlation coefficient and approximate p-value
struct RegResult { double r, p, slope, intercept; };

RegResult computeRegression(const std::vector<double>& x,
                             const std::vector<double>& y)
{
    int n = static_cast<int>(x.size());
    if (n < 3) return { 0.0, 1.0, 0.0, 0.0 };

    double mx = 0, my = 0;
    for (int i = 0; i < n; ++i) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;

    double sxx = 0, sxy = 0, syy = 0;
    for (int i = 0; i < n; ++i) {
        double dx = x[i] - mx, dy = y[i] - my;
        sxx += dx*dx; sxy += dx*dy; syy += dy*dy;
    }

    RegResult res{};
    res.r = (sxx > 1e-30 && syy > 1e-30) ? sxy / std::sqrt(sxx * syy) : 0.0;
    res.slope     = (sxx > 1e-30) ? sxy / sxx : 0.0;
    res.intercept = my - res.slope * mx;

    // t-approximation -> normal tail (valid for large n like MC 1000+)
    double t = res.r * std::sqrt((n - 2.0) / (1.0 - res.r*res.r + 1e-30));
    res.p = 2.0 * (1.0 - std::erf(std::fabs(t) / std::numbers::sqrt2));
    return res;
}

// Build a gnuplot datablock string from paired x,y vectors
std::string makeDatablock(const std::string& name,
                           const std::vector<double>& x,
                           const std::vector<double>& y)
{
    std::ostringstream oss;
    oss << name << " << EOD\n";
    for (std::size_t i = 0; i < x.size(); ++i)
        oss << fmt(x[i]) << " " << fmt(y[i]) << "\n";
    oss << "EOD";
    return oss.str();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

PlotGeodesy::PlotGeodesy(const GeodesyConfig& cfg, const std::string& datasetId)
    : m_cfg(cfg)
    , m_datasetId(datasetId)
    , m_outDir(cfg.imgDir)
{
    std::filesystem::create_directories(m_outDir);
}

// ---------------------------------------------------------------------------
// chi2Quantile2dof
// ---------------------------------------------------------------------------

double PlotGeodesy::chi2Quantile2dof(double prob) noexcept
{
    // Exact formula for chi2 with 2 dof: F^{-1}(p) = -2 ln(1-p)
    return -2.0 * std::log(1.0 - prob);
}

// ---------------------------------------------------------------------------
// outPath
// ---------------------------------------------------------------------------

std::string PlotGeodesy::outPath(const std::string& tag,
                                  const std::string& suffix) const
{
    return m_outDir + "/geodesy_" + m_datasetId + "_" + tag + "_" + suffix + ".png";
}

// ---------------------------------------------------------------------------
// computeEllipseAndHelmert
//
// Error ellipse:
//   Eigendecompose 2x2 cov -> eigenvalues lMax, lMin, eigenvector angle theta.
//   Scale semi-axes by sqrt(chi2) to get a, b.
//   Parametric ellipse in principal frame: (a cos t, b sin t).
//   Rotate by theta.
//
// Helmert curve (pedal curve of the error ellipse):
//   In principal frame:
//     x'(t) = a^2 cos(t) / rho(t)
//     y'(t) = b^2 sin(t) / rho(t)
//     rho(t) = sqrt(a^2 cos^2(t) + b^2 sin^2(t))
//   Then rotate by theta.
//
// Both curves centred at (0,0). Caller adds the mean offset.
// NOTE: cov2x2 is indexed [row, col] i.e. cov2x2(0,0)=var_row, cov2x2(1,1)=var_col.
// The scatter plot has col on x-axis and row on y-axis, so we swap when building
// the submatrix: sub(0,0)=var_col, sub(1,1)=var_row.
// ---------------------------------------------------------------------------

void PlotGeodesy::computeEllipseAndHelmert(const Eigen::Matrix2d& cov2x2,
                                            double                  prob,
                                            std::vector<double>&    xEll,
                                            std::vector<double>&    yEll,
                                            std::vector<double>&    xHelm,
                                            std::vector<double>&    yHelm)
{
    xEll.clear(); yEll.clear(); xHelm.clear(); yHelm.clear();

    if (cov2x2(0,0) <= 0.0 || cov2x2(1,1) <= 0.0) return;

    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eig(cov2x2);
    if (eig.info() != Eigen::Success) return;

    Eigen::Vector2d eigVals = eig.eigenvalues();
    Eigen::Matrix2d eigVecs = eig.eigenvectors();

    if (eigVals(0) <= 0.0 || eigVals(1) <= 0.0) return;

    double lMax  = eigVals(1);
    double lMin  = eigVals(0);
    double scale = std::sqrt(chi2Quantile2dof(prob));
    double a     = scale * std::sqrt(lMax);
    double b     = scale * std::sqrt(lMin);

    Eigen::Vector2d vMax = eigVecs.col(1);
    double theta = std::atan2(vMax(1), vMax(0));
    double cosT  = std::cos(theta);
    double sinT  = std::sin(theta);

    constexpr int NPTS = 720;
    auto tVec = linspace(0.0, 2.0 * std::numbers::pi, NPTS + 1);

    xEll.resize(static_cast<std::size_t>(NPTS + 1));
    yEll.resize(static_cast<std::size_t>(NPTS + 1));
    xHelm.resize(static_cast<std::size_t>(NPTS + 1));
    yHelm.resize(static_cast<std::size_t>(NPTS + 1));

    for (int i = 0; i <= NPTS; ++i) {
        std::size_t si = static_cast<std::size_t>(i);
        double ct = std::cos(tVec[si]);
        double st = std::sin(tVec[si]);
        double ex = a * ct;
        double ey = b * st;
        xEll[si]  = cosT * ex - sinT * ey;
        yEll[si]  = sinT * ex + cosT * ey;
        // Helmert not used -- set to zero
        xHelm[si] = 0.0;
        yHelm[si] = 0.0;
    }
}

// ---------------------------------------------------------------------------
// plotCovariancePanel
// ---------------------------------------------------------------------------

auto fmt6 = [](double v) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(6) << v;
    return o.str();
};

void PlotGeodesy::plotCovariancePanel(const Eigen::MatrixXd& S,
                                       const Eigen::MatrixXd& analyticalCov,
                                       const Eigen::MatrixXd& empiricalCov,
                                       const Eigen::VectorXd& mean,
                                       double                  prob,
                                       const std::string&      tag,
                                       const std::string&      panelTitle) const
{
    int N  = static_cast<int>(S.rows());
    int nS = static_cast<int>(S.cols());

    const int baseFont = 18;
    int fontSmall = std::max(9, baseFont - 2*N);
    int fontAxis  = std::max(10, baseFont - N);
    int fontTitle = std::max(12, baseFont - N);

    if (N < 2 || nS < 2) return;

    auto labels = axisLabels(m_cfg.outputSystem);
    while (static_cast<int>(labels.size()) < N)
        labels.push_back("c" + std::to_string(labels.size()));

    std::string pngPath = fwdSlash(outPath(tag, "covariance"));

    const int cellW_px = 600;
    const int cellH_px = 430;
    loki::Gnuplot gp;

    gp("set terminal pngcairo noenhanced font 'Sans," + std::to_string(fontSmall) +
       "' size " + std::to_string(N * cellW_px) + "," + std::to_string(N * cellH_px)); 
    gp("set output '" + pngPath + "'");
    gp("set multiplot title '" + panelTitle + "' font 'Sans,20' offset 0,-0.8");

    const double pw = 1.0 / N;
    const double ph = 1.0 / N;
    const double cellW        = pw * 0.97;
    const double cellH        = ph * 0.92;
    const double marginL      = 0.06;   // pre ylabel v lavom stlpci
    const double marginLinner = 0.02;   // ostatne stlpce
    const double marginR      = 0.01;
    const double marginT      = 0.02;
    const double marginTtop   = 0.075;   // horny riadok - miesto pre panel title
    const double marginB      = 0.02;
    const double marginBbot   = 0.09;   // spodny riadok - miesto pre xlabel + xtics
    const double gapX         = 0.010; // Medzera medzi stĺpcami (1% šírky)
    const double gapY         = 0.065; // Medzera medzi riadkami (6.5% výšky)

    for (int iRow = 0; iRow < N; ++iRow) {
        for (int iCol = 0; iCol < N; ++iCol) {

            // Origin: (col * pw, bottom-up row index * ph)
            double ox = iCol * pw;
            double oy = (N - 1 - iRow) * ph;

            std::ostringstream origin, sz;
            origin << std::fixed << std::setprecision(6) << ox << "," << oy;
            sz     << std::fixed << std::setprecision(6) << cellW << "," << cellH;

            gp("set origin " + origin.str());
            gp("set size "   + sz.str());

            
            double ml = (iCol == 0)     ? marginL    : marginLinner;
            double mt = (iRow == 0)     ? marginTtop : marginT;
            double mb = (iRow == N - 1) ? marginBbot : marginB;
            double mr = marginR;
            /*
            double screenL = ox + ml;
            double screenR = ox + pw - mr;
            double screenB = oy + mb;
            double screenT = oy + ph - mt;
            */
            
            double screenL = ox + ml + (iCol > 0 ? gapX / 2.0 : 0);
            double screenR = ox + pw - mr - (iCol < N - 1 ? gapX / 2.0 : 0);
            double screenB = oy + mb + (iRow < N - 1 ? gapY / 2.0 : 0);
            double screenT = oy + ph - mt - (iRow > 0 ? gapY / 2.0 : 0);

            gp("set lmargin at screen " + fmt6(screenL));
            gp("set rmargin at screen " + fmt6(screenR));
            gp("set bmargin at screen " + fmt6(screenB));
            gp("set tmargin at screen " + fmt6(screenT));

            // gp("set title offset 0,-.8");
                        
            
            gp("unset key");
            gp("set grid");
            // Rotate xtics 45 deg, shift down so they don't overlap the axis line
            gp("set xtics rotate by 45 right offset -1,-1 font 'Sans," + std::to_string(fontSmall) + "'");
            gp("set ytics font 'Sans,12'");

            // X label only on bottom row
            if (iRow == N - 1)
                gp("set xlabel '" + labels[static_cast<std::size_t>(iCol)]
                   + "' font 'Sans,14'");
            else
                gp("set xlabel ''");

            // Y label: left column only (NOT on diagonal -- histogram y-axis
            // shows counts, not the coordinate of another row)
            if (iCol == 0)
                gp("set ylabel '" + labels[static_cast<std::size_t>(iRow)]
                   + "' font 'Sans,14'");
            else
                gp("set ylabel ''");

            // Unique datablock prefix per cell
            std::string pfx = "$d" + std::to_string(iRow)
                              + std::to_string(iCol) + "_";

            // ================================================================
            // DIAGONAL: vertical histogram (like Matlab) + two normal curves
            //   x-axis = coordinate value
            //   y-axis = count (density)
            //   red curve  = analytical normal (from Jacobian cov)
            //   green curve = empirical normal (from MC samples)
            // ================================================================
            if (iRow == iCol) {

                std::vector<double> vals(static_cast<std::size_t>(nS));
                for (int s = 0; s < nS; ++s)
                    vals[static_cast<std::size_t>(s)] = S(iRow, s);

                double vmin  = *std::min_element(vals.begin(), vals.end());
                double vmax  = *std::max_element(vals.begin(), vals.end());
                double range = vmax - vmin;
                if (range < 1e-30) range = 1.0;

                int    nbins = std::max(20, static_cast<int>(std::sqrt(nS)));
                double bw    = range / nbins;
                if (bw < 1e-30) bw = 1.0;

                std::vector<int> cnt(static_cast<std::size_t>(nbins), 0);
                for (double v : vals) {
                    int b = static_cast<int>((v - vmin) / bw);
                    b = std::clamp(b, 0, nbins - 1);
                    ++cnt[static_cast<std::size_t>(b)];
                }

                // Histogram datablock: col1=bin_centre, col2=count
                // Vertical bars: x=bin_centre, y=count ("using 1:2 with boxes")
                std::string dsHist = pfx + "hist";
                {
                    std::ostringstream oss;
                    oss << dsHist << " << EOD\n";
                    for (int b = 0; b < nbins; ++b)
                        oss << fmt(vmin + (b + 0.5) * bw) << " "
                            << fmt(static_cast<double>(cnt[static_cast<std::size_t>(b)]))
                            << "\n";
                    oss << "EOD";
                    gp(oss.str());
                }

                double sigAn  = std::sqrt(analyticalCov(iRow, iRow));
                double sigEmp = std::sqrt(empiricalCov(iRow, iRow));
                double muVal  = mean(iRow);
                double pdfScl = nS * bw;

                // PDF datablock: col1=value, col2=density (vertical curve)
                auto makePdf = [&](const std::string& dsName, double sig) {
                    std::ostringstream oss;
                    oss << dsName << " << EOD\n";
                    for (double v : linspace(vmin - 0.1*range, vmax + 0.1*range, 300)) {
                        double d  = (sig > 1e-30) ? (v - muVal) / sig : 0.0;
                        double pd = (sig > 1e-30)
                            ? pdfScl / (sig * std::sqrt(2.0 * std::numbers::pi))
                              * std::exp(-0.5 * d * d)
                            : 0.0;
                        oss << fmt(v) << " " << fmt(pd) << "\n";
                    }
                    oss << "EOD";
                    gp(oss.str());
                };

                std::string dsPdfAn  = pfx + "pdfan";
                std::string dsPdfEmp = pfx + "pdfemp";
                makePdf(dsPdfAn,  sigAn);
                makePdf(dsPdfEmp, sigEmp);

                std::ostringstream titleSS;
                titleSS << std::scientific << std::setprecision(3);
                titleSS << "s_an=" << sigAn << " s_emp=" << sigEmp;
                gp("set title '" + titleSS.str() + "' font 'Sans,15'");
                //gp("unset label"); 
                //gp("set label 1 '" + titleSS.str() + "' at graph 0.5, graph 1.05 center font 'Sans,12'");

                int cntMax = *std::max_element(cnt.begin(), cnt.end());
                gp("set xrange [" + fmt(vmin - 0.05*range) + ":"
                                  + fmt(vmax + 0.05*range) + "]");
                gp("set yrange [0:" + fmt(cntMax * 1.20) + "]");
                gp("set boxwidth " + fmt(bw * 0.85) + " absolute");
                gp("set style fill solid 0.55 border -1");

                // Vertical bars: x=bin_centre (col1), y=count (col2)
                gp("plot " + dsHist
                   + " using 1:2 with boxes lc rgb '#555555' notitle, "
                   + dsPdfAn  + " using 1:2 with lines lw 2 lc rgb '#cc2200' notitle, "
                   + dsPdfEmp + " using 1:2 with lines lw 2 lc rgb '#007700' notitle");

                gp("unset xrange");
                gp("unset yrange");
                gp("unset style");

            // ================================================================
            // LOWER TRIANGLE: scatter + analytical ellipse (blue) +
            //                 empirical ellipse (green) + Helmert (red dashed)
            //
            // IMPORTANT: do NOT use "set size ratio" inside multiplot --
            // it corrupts subsequent cell positions. Instead we rely on
            // autoscale and let gnuplot choose axis ranges naturally.
            // ================================================================
            } else if (iRow > iCol) {

                // Scatter: x = col component, y = row component
                std::string dsSc = pfx + "sc";
                {
                    std::ostringstream oss;
                    oss << dsSc << " << EOD\n";
                    for (int s = 0; s < nS; ++s)
                        oss << fmt(S(iCol, s)) << " " << fmt(S(iRow, s)) << "\n";
                    oss << "EOD";
                    gp(oss.str());
                }

                double mx = mean(iCol);
                double my = mean(iRow);

                // 2x2 submatrix with x=col, y=row ordering
                auto makeSub = [&](const Eigen::MatrixXd& cov) -> Eigen::Matrix2d {
                    Eigen::Matrix2d sub;
                    sub(0,0) = cov(iCol, iCol);
                    sub(0,1) = cov(iCol, iRow);
                    sub(1,0) = cov(iRow, iCol);
                    sub(1,1) = cov(iRow, iRow);
                    return sub;
                };

                Eigen::Matrix2d subAn  = makeSub(analyticalCov);
                Eigen::Matrix2d subEmp = makeSub(empiricalCov);

                std::vector<double> xEllA, yEllA, xHelmA, yHelmA;
                computeEllipseAndHelmert(subAn, prob, xEllA, yEllA, xHelmA, yHelmA);

                std::vector<double> xEllE, yEllE, xHelmE, yHelmE;
                computeEllipseAndHelmert(subEmp, prob, xEllE, yEllE, xHelmE, yHelmE);

                if (xEllA.empty())
                    LOKI_WARNING("PlotGeodesy: analytical ellipse degenerate at ("
                                 + std::to_string(iRow) + "," + std::to_string(iCol) + ")");
                if (xEllE.empty())
                    LOKI_WARNING("PlotGeodesy: empirical ellipse degenerate at ("
                                 + std::to_string(iRow) + "," + std::to_string(iCol) + ")");

                std::ostringstream titleSS;
                titleSS << std::scientific << std::setprecision(3);
                titleSS << "cov_an=" << analyticalCov(iRow, iCol)
                        << " cov_emp=" << empiricalCov(iRow, iCol);
                gp("set title '" + titleSS.str() + "' font 'Sans,15'");

                std::string plotCmd = dsSc + " with points pt 7 ps 0.20"
                                             " lc rgb '#333333' notitle";

                if (!xEllA.empty()) {
                    std::vector<double> xEA(xEllA.size()), yEA(yEllA.size());
                    for (std::size_t k = 0; k < xEllA.size(); ++k) {
                        xEA[k] = mx + xEllA[k]; yEA[k] = my + yEllA[k];
                    }
                    gp(makeDatablock(pfx + "ellan", xEA, yEA));
                    plotCmd += ", " + pfx + "ellan"
                               + " with lines lw 2 lc rgb '#0055cc' notitle";
                }

                if (!xEllE.empty()) {
                    std::vector<double> xEE(xEllE.size()), yEE(yEllE.size());
                    for (std::size_t k = 0; k < xEllE.size(); ++k) {
                        xEE[k] = mx + xEllE[k]; yEE[k] = my + yEllE[k];
                    }
                    gp(makeDatablock(pfx + "ellemp", xEE, yEE));
                    plotCmd += ", " + pfx + "ellemp"
                               + " with lines lw 2 lc rgb '#007700' notitle";
                }

                gp("unset key");
                gp("plot " + plotCmd);

            // ================================================================
            // UPPER TRIANGLE: scatter + regression line + R[p] in title
            // ================================================================
            } else {

                std::vector<double> xv(static_cast<std::size_t>(nS));
                std::vector<double> yv(static_cast<std::size_t>(nS));
                for (int s = 0; s < nS; ++s) {
                    xv[static_cast<std::size_t>(s)] = S(iCol, s);
                    yv[static_cast<std::size_t>(s)] = S(iRow, s);
                }

                RegResult reg = computeRegression(xv, yv);

                std::string dsSc = pfx + "sc";
                {
                    std::ostringstream oss;
                    oss << dsSc << " << EOD\n";
                    for (int s = 0; s < nS; ++s)
                        oss << fmt(xv[static_cast<std::size_t>(s)]) << " "
                            << fmt(yv[static_cast<std::size_t>(s)]) << "\n";
                    oss << "EOD";
                    gp(oss.str());
                }

                double xmin = *std::min_element(xv.begin(), xv.end());
                double xmax = *std::max_element(xv.begin(), xv.end());
                std::vector<double> xReg = linspace(xmin, xmax, 50);
                std::vector<double> yReg(50);
                for (int k = 0; k < 50; ++k)
                    yReg[static_cast<std::size_t>(k)] =
                        reg.slope * xReg[static_cast<std::size_t>(k)] + reg.intercept;

                gp(makeDatablock(pfx + "reg", xReg, yReg));

                std::ostringstream titleSS;
                titleSS << std::fixed << std::setprecision(2)
                        << "R:" << reg.r << "["
                        << std::setprecision(3) << reg.p << "]";
                gp("set title '" + titleSS.str() + "' font 'Sans,12'");

                gp("plot " + dsSc + " with points pt 7 ps 0.20"
                             " lc rgb '#333333' notitle, "
                   + pfx + "reg with lines lw 2 lc rgb '#cc2200' notitle");
            }
        }
    }

    gp("unset multiplot");
    LOKI_INFO("PlotGeodesy: wrote " + pngPath);
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
        ds << i << " " << fmt(lines[i].geodesic.distance) << "\n";
    ds << "EOD";

    gp(ds.str());
    gp("plot $dist using 1:2 with boxes lc rgb '#0055aa'");

    LOKI_INFO("PlotGeodesy: wrote " + pngPath);
}
