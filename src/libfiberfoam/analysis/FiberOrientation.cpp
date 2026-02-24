#include "analysis/FiberOrientation.h"
#include "common/Logger.h"

#include <Eigen/Dense>
#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace fiberfoam
{
namespace
{

// ---------------------------------------------------------------------------
// Separable 2D Gaussian blur on a row-major (rows x cols) matrix.
// ---------------------------------------------------------------------------
void gaussianBlur2D(std::vector<double>& img, int rows, int cols, double sigma)
{
    if (sigma <= 0.0)
        return;

    // Build 1-D kernel.  Radius = ceil(3*sigma) so that we capture >99% of the
    // Gaussian mass.
    const int radius = static_cast<int>(std::ceil(3.0 * sigma));
    const int ksize = 2 * radius + 1;
    std::vector<double> kernel(ksize);
    double sum = 0.0;
    for (int i = 0; i < ksize; ++i)
    {
        double d = i - radius;
        kernel[i] = std::exp(-0.5 * d * d / (sigma * sigma));
        sum += kernel[i];
    }
    for (auto& v : kernel)
        v /= sum;

    // Temporary buffer for a single pass.
    std::vector<double> tmp(img.size());

    // --- Pass 1: convolve along columns (horizontal direction) ---
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            double acc = 0.0;
            for (int k = -radius; k <= radius; ++k)
            {
                int cc = c + k;
                // Mirror boundary.
                if (cc < 0)
                    cc = -cc;
                if (cc >= cols)
                    cc = 2 * cols - 2 - cc;
                acc += img[r * cols + cc] * kernel[k + radius];
            }
            tmp[r * cols + c] = acc;
        }
    }

    // --- Pass 2: convolve along rows (vertical direction) ---
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            double acc = 0.0;
            for (int k = -radius; k <= radius; ++k)
            {
                int rr = r + k;
                if (rr < 0)
                    rr = -rr;
                if (rr >= rows)
                    rr = 2 * rows - 2 - rr;
                acc += tmp[rr * cols + c] * kernel[k + radius];
            }
            img[r * cols + c] = acc;
        }
    }
}

// ---------------------------------------------------------------------------
// fftshift for 3-D magnitude array stored row-major (nx * ny * nz).
// Swaps quadrants so that the zero-frequency component is at the centre.
// ---------------------------------------------------------------------------
void fftshift3D(std::vector<double>& data, int nx, int ny, int nz)
{
    std::vector<double> shifted(data.size());
    const int hx = nx / 2;
    const int hy = ny / 2;
    const int hz = nz / 2;

    for (int ix = 0; ix < nx; ++ix)
    {
        int sx = (ix + hx) % nx;
        for (int iy = 0; iy < ny; ++iy)
        {
            int sy = (iy + hy) % ny;
            for (int iz = 0; iz < nz; ++iz)
            {
                int sz = (iz + hz) % nz;
                shifted[sx * ny * nz + sy * nz + sz] =
                    data[ix * ny * nz + iy * nz + iz];
            }
        }
    }
    data.swap(shifted);
}

// ---------------------------------------------------------------------------
// Normalise angle to [0, 90] range:  min(t, 180 - t).
// ---------------------------------------------------------------------------
double normaliseTo0_90(double deg)
{
    // Bring into [0, 180) first.
    deg = std::fmod(deg, 180.0);
    if (deg < 0.0)
        deg += 180.0;
    return std::min(deg, 180.0 - deg);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
double estimateFiberOrientation(const VoxelArray& geometry, double gaussianSigma)
{
    const int nx = geometry.nx();
    const int ny = geometry.ny();
    const int nz = geometry.nz();

    if (nx == 0 || ny == 0 || nz == 0)
        throw std::runtime_error("estimateFiberOrientation: empty VoxelArray");

    Logger::info("Estimating fiber orientation via FFT (sigma=" +
                 std::to_string(gaussianSigma) + ") on " + std::to_string(nx) +
                 "x" + std::to_string(ny) + "x" + std::to_string(nz) + " grid");

    // ------------------------------------------------------------------
    // 1.  Prepare real input for FFTW  (row-major: x varies fastest in memory
    //     is how VoxelArray stores data, but fftw_plan_dft_r2c_3d expects the
    //     LAST dimension to vary fastest, which matches our x+nx*(y+ny*z)
    //     layout when we tell FFTW the dimensions are (nz, ny, nx)).
    // ------------------------------------------------------------------
    const int n0 = nz, n1 = ny, n2 = nx;                  // FFTW dim ordering
    const int nzHermitian = nx / 2 + 1;                     // Hermitian last-dim
    const long totalReal = static_cast<long>(n0) * n1 * n2;
    const long totalComplex = static_cast<long>(n0) * n1 * nzHermitian;

    double* in = fftw_alloc_real(totalReal);
    fftw_complex* out = fftw_alloc_complex(totalComplex);
    if (!in || !out)
        throw std::runtime_error("estimateFiberOrientation: FFTW allocation failed");

    // Copy voxel data to double array.
    const auto& vdata = geometry.data();
    for (long i = 0; i < totalReal; ++i)
        in[i] = static_cast<double>(vdata[i]);

    // Create plan and execute.
    fftw_plan plan = fftw_plan_dft_r2c_3d(n0, n1, n2, in, out, FFTW_ESTIMATE);
    if (!plan)
    {
        fftw_free(in);
        fftw_free(out);
        throw std::runtime_error("estimateFiberOrientation: FFTW plan creation failed");
    }
    fftw_execute(plan);
    fftw_destroy_plan(plan);
    fftw_free(in);

    // ------------------------------------------------------------------
    // 2.  Compute full magnitude array by exploiting Hermitian symmetry,
    //     then apply fftshift.
    // ------------------------------------------------------------------
    std::vector<double> mag(static_cast<size_t>(n0) * n1 * n2, 0.0);

    // Fill from Hermitian half.
    for (int iz = 0; iz < n0; ++iz)
    {
        for (int iy = 0; iy < n1; ++iy)
        {
            for (int ix = 0; ix < nzHermitian; ++ix)
            {
                long idx = static_cast<long>(iz) * n1 * nzHermitian +
                           static_cast<long>(iy) * nzHermitian + ix;
                double re = out[idx][0];
                double im = out[idx][1];
                double m = std::sqrt(re * re + im * im);

                // Direct half.
                mag[static_cast<size_t>(iz) * n1 * n2 +
                    static_cast<size_t>(iy) * n2 + ix] = m;

                // Mirror half (ix > 0 and ix < n2/2+1 maps to n2-ix).
                if (ix > 0 && ix < n2 - ix)
                {
                    int miz = (n0 - iz) % n0;
                    int miy = (n1 - iy) % n1;
                    int mix = n2 - ix;
                    mag[static_cast<size_t>(miz) * n1 * n2 +
                        static_cast<size_t>(miy) * n2 + mix] = m;
                }
            }
        }
    }
    fftw_free(out);

    fftshift3D(mag, n0, n1, n2);

    // ------------------------------------------------------------------
    // 3.  Central z-slice projection (average +/-2 slices around centre).
    //     In the FFTW dimension ordering the first axis is the original z,
    //     so "central z-slice" means central slice along axis-0 of mag.
    // ------------------------------------------------------------------
    const int centZ = n0 / 2;
    const int sliceLo = std::max(0, centZ - 2);
    const int sliceHi = std::min(n0 - 1, centZ + 2);
    const int nSlices = sliceHi - sliceLo + 1;

    std::vector<double> projection(static_cast<size_t>(n1) * n2, 0.0);
    for (int iz = sliceLo; iz <= sliceHi; ++iz)
    {
        for (int iy = 0; iy < n1; ++iy)
        {
            for (int ix = 0; ix < n2; ++ix)
            {
                projection[static_cast<size_t>(iy) * n2 + ix] +=
                    mag[static_cast<size_t>(iz) * n1 * n2 +
                        static_cast<size_t>(iy) * n2 + ix];
            }
        }
    }
    for (auto& v : projection)
        v /= nSlices;

    // Free large magnitude array -- no longer needed.
    mag.clear();
    mag.shrink_to_fit();

    // ------------------------------------------------------------------
    // 4.  Gaussian smoothing of the 2-D projection.
    // ------------------------------------------------------------------
    gaussianBlur2D(projection, n1, n2, gaussianSigma);

    // ------------------------------------------------------------------
    // 5.  Threshold at 50% of max and collect bright-pixel coordinates.
    // ------------------------------------------------------------------
    double maxVal = *std::max_element(projection.begin(), projection.end());
    double threshold = 0.5 * maxVal;

    std::vector<Eigen::Vector2d> coords;
    coords.reserve(projection.size() / 4); // rough guess
    for (int iy = 0; iy < n1; ++iy)
    {
        for (int ix = 0; ix < n2; ++ix)
        {
            if (projection[static_cast<size_t>(iy) * n2 + ix] > threshold)
                coords.emplace_back(static_cast<double>(iy),
                                    static_cast<double>(ix));
        }
    }

    if (coords.size() < 2)
    {
        Logger::warning("estimateFiberOrientation: fewer than 2 bright pixels "
                        "after thresholding; returning 0 degrees");
        return 0.0;
    }

    // ------------------------------------------------------------------
    // 6.  PCA via Eigen: centre data, covariance, eigendecomposition.
    // ------------------------------------------------------------------
    const int N = static_cast<int>(coords.size());

    // Compute mean.
    Eigen::Vector2d mean = Eigen::Vector2d::Zero();
    for (const auto& c : coords)
        mean += c;
    mean /= N;

    // Build centred data matrix (N x 2).
    Eigen::MatrixXd centred(N, 2);
    for (int i = 0; i < N; ++i)
        centred.row(i) = (coords[i] - mean).transpose();

    // Covariance matrix (2 x 2).
    Eigen::Matrix2d cov = (centred.transpose() * centred) / (N - 1);

    // Eigendecomposition (SelfAdjointEigenSolver returns eigenvalues in
    // ascending order, so the dominant eigenvector is the last one).
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> solver(cov);
    if (solver.info() != Eigen::Success)
        throw std::runtime_error("estimateFiberOrientation: eigendecomposition failed");

    Eigen::Vector2d dominantFreqDir = solver.eigenvectors().col(1); // largest eigenvalue

    // ------------------------------------------------------------------
    // 7.  Fiber direction is orthogonal to dominant frequency direction.
    // ------------------------------------------------------------------
    Eigen::Vector2d fiberDir(-dominantFreqDir(1), dominantFreqDir(0));

    double angleRad = std::atan2(fiberDir(1), fiberDir(0));
    double angleDeg = angleRad * 180.0 / M_PI;

    // Bring into [0, 180) then normalise to [0, 90].
    angleDeg = std::fmod(angleDeg, 180.0);
    if (angleDeg < 0.0)
        angleDeg += 180.0;
    angleDeg = normaliseTo0_90(angleDeg);

    Logger::info("Estimated fiber orientation: " + std::to_string(angleDeg) + " deg");
    return angleDeg;
}

} // namespace fiberfoam
