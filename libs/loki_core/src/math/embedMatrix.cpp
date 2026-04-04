#include <loki/math/embedMatrix.hpp>
#include <loki/core/exceptions.hpp>

using namespace loki;

namespace loki::math {

Eigen::MatrixXd buildEmbedMatrix(const std::vector<double>& y, int L)
{
    const std::size_t n = y.size();

    if (n < 3) {
        throw SeriesTooShortException(
            "buildEmbedMatrix: series length " + std::to_string(n)
            + " is too short (minimum 3).");
    }
    if (L < 2) {
        throw AlgorithmException(
            "buildEmbedMatrix: window length L=" + std::to_string(L)
            + " must be >= 2.");
    }
    if (L >= static_cast<int>(n)) {
        throw AlgorithmException(
            "buildEmbedMatrix: window length L=" + std::to_string(L)
            + " must be < series length n=" + std::to_string(n) + ".");
    }

    const int K = static_cast<int>(n) - L + 1;

    // X[i, j] = y[i + j],  i = 0..K-1,  j = 0..L-1
    Eigen::MatrixXd X(K, L);
    for (int i = 0; i < K; ++i) {
        for (int j = 0; j < L; ++j) {
            X(i, j) = y[static_cast<std::size_t>(i + j)];
        }
    }

    return X;
}

} // namespace loki::math
