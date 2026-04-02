#include <loki/math/lagMatrix.hpp>
#include <loki/core/exceptions.hpp>

using namespace loki;

namespace loki::math {

Eigen::MatrixXd buildLagMatrix(const std::vector<double>& y, int p)
{
    if (p < 1) {
        throw AlgorithmException(
            "buildLagMatrix: AR order p must be >= 1, got "
            + std::to_string(p) + ".");
    }

    const std::size_t n = y.size();
    if (n <= static_cast<std::size_t>(p)) {
        throw SeriesTooShortException(
            "buildLagMatrix: series length " + std::to_string(n)
            + " must be greater than AR order p=" + std::to_string(p) + ".");
    }

    const std::size_t rows = n - static_cast<std::size_t>(p);
    Eigen::MatrixXd G(static_cast<Eigen::Index>(rows),
                      static_cast<Eigen::Index>(p));

    // Row i = [ y[i+p-1], y[i+p-2], ..., y[i] ]
    // This matches the notation z_t = (Y_{t-1}, Y_{t-2}, ..., Y_{t-p})^T
    // from Hau & Tong (1989), with t = i+p.
    for (std::size_t i = 0; i < rows; ++i) {
        for (int lag = 0; lag < p; ++lag) {
            G(static_cast<Eigen::Index>(i),
              static_cast<Eigen::Index>(lag)) = y[i + static_cast<std::size_t>(p) - 1
                                                    - static_cast<std::size_t>(lag)];
        }
    }

    return G;
}

} // namespace loki::math
