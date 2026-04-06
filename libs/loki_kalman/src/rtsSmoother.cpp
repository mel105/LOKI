#include "loki/kalman/rtsSmoother.hpp"

#include "loki/core/exceptions.hpp"

using namespace loki;

namespace loki::kalman {

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------

RtsSmoother::RtsSmoother(KalmanModel model)
    : m_model(std::move(model))
{}

// -----------------------------------------------------------------------------
//  smooth -- backward pass
// -----------------------------------------------------------------------------

std::vector<SmootherStep> RtsSmoother::smooth(const std::vector<FilterStep>& filterSteps) const
{
    if (filterSteps.empty()) {
        throw DataException("RtsSmoother::smooth: filterSteps is empty.");
    }

    const std::size_t T = filterSteps.size();
    const int n = static_cast<int>(m_model.F.rows());
    const Eigen::MatrixXd Ft = m_model.F.transpose();

    std::vector<SmootherStep> steps(T);

    // Initialise terminal condition: smoothed == filtered at last epoch
    {
        SmootherStep& last   = steps[T - 1];
        const FilterStep& fs = filterSteps[T - 1];
        last.xSmooth = fs.xFilt;
        last.PSmooth = fs.PFilt;
        last.G       = Eigen::MatrixXd::Zero(n, n);
    }

    // Backward recursion: t = T-2 down to 0
    for (int t = static_cast<int>(T) - 2; t >= 0; --t) {
        const FilterStep& fsCur  = filterSteps[static_cast<std::size_t>(t)];
        const FilterStep& fsNext = filterSteps[static_cast<std::size_t>(t + 1)];
        SmootherStep& sCur       = steps[static_cast<std::size_t>(t)];
        const SmootherStep& sNext = steps[static_cast<std::size_t>(t + 1)];

        // Smoother gain: G[t] = P[t|t] * F' * inv(P[t+1|t])
        // Use LLT (Cholesky) for stable inversion of the positive-definite PPred.
        const Eigen::LLT<Eigen::MatrixXd> llt(fsNext.PPred);
        if (llt.info() != Eigen::Success) {
            // Fallback to pseudoinverse if Cholesky fails (degenerate PPred)
            const Eigen::MatrixXd Pinv = fsNext.PPred.completeOrthogonalDecomposition()
                                                      .pseudoInverse();
            sCur.G = fsCur.PFilt * Ft * Pinv;
        } else {
            // G = PFilt * F' * PPred^{-1}  via solve: (PPred * G^T = F * PFilt^T)
            sCur.G = llt.solve((m_model.F * fsCur.PFilt.transpose()).transpose()).transpose();
        }

        // Smoothed state
        sCur.xSmooth = fsCur.xFilt
                       + sCur.G * (sNext.xSmooth - m_model.F * fsCur.xFilt);

        // Smoothed covariance
        const Eigen::MatrixXd dP = sNext.PSmooth - fsNext.PPred;
        sCur.PSmooth = fsCur.PFilt + sCur.G * dP * sCur.G.transpose();
    }

    return steps;
}

} // namespace loki::kalman
