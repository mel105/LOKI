#include <loki/math/designMatrix.hpp>
#include <loki/core/exceptions.hpp>

#include <cmath>

using namespace loki;

static constexpr double TWO_PI = 2.0 * 3.141592653589793238462643383279502884;

Eigen::MatrixXd DesignMatrix::polynomial(const Eigen::VectorXd& x, int degree)
{
    if (x.size() == 0) {
        throw AlgorithmException("DesignMatrix::polynomial: x vector is empty.");
    }
    if (degree < 0) {
        throw AlgorithmException("DesignMatrix::polynomial: degree must be >= 0.");
    }

    const int m = static_cast<int>(x.size());
    const int n = degree + 1;
    Eigen::MatrixXd A(m, n);

    for (int i = 0; i < m; ++i) {
        double xi = 1.0;
        for (int j = 0; j < n; ++j) {
            A(i, j) = xi;
            xi *= x(i);
        }
    }
    return A;
}

Eigen::MatrixXd DesignMatrix::harmonic(const Eigen::VectorXd& t,
                                        const std::vector<double>& periods)
{
    if (t.size() == 0) {
        throw AlgorithmException("DesignMatrix::harmonic: t vector is empty.");
    }
    if (periods.empty()) {
        throw AlgorithmException("DesignMatrix::harmonic: periods list is empty.");
    }

    const int m = static_cast<int>(t.size());
    const int nPeriods = static_cast<int>(periods.size());
    const int n = 1 + 2 * nPeriods; // constant + sin+cos per period

    Eigen::MatrixXd A(m, n);

    for (int i = 0; i < m; ++i) {
        A(i, 0) = 1.0; // constant term
        for (int p = 0; p < nPeriods; ++p) {
            const double phase = TWO_PI * t(i) / periods[static_cast<std::size_t>(p)];
            A(i, 1 + 2 * p)     = std::sin(phase);
            A(i, 1 + 2 * p + 1) = std::cos(phase);
        }
    }
    return A;
}

Eigen::MatrixXd DesignMatrix::identity(int n)
{
    if (n <= 0) {
        throw AlgorithmException("DesignMatrix::identity: n must be > 0.");
    }
    return Eigen::MatrixXd::Identity(n, n);
}
