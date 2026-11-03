/*
 * slalom-cxx -- C++ implementation of the SLALOM benchmark.
 *
 * This optional C++ port uses STL containers (`std::vector`) and a dense
 * Gaussian-elimination solver instead of the original C reference routines.
 * It remains off by default so the published C benchmark remains the canonical
 * result generator when SLALOM_ENABLE_CXX is not explicitly enabled.
 */

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "file.hpp"
#include "system_info.h"

namespace {

using Matrix = std::vector<std::vector<double>>;
using Vector = std::vector<double>;

const char *text(const char *s)
{
    return (s != nullptr) ? s : "unknown";
}

std::vector<double> read_box(const slalom::File &geom)
{
    std::vector<double> box(3, 0.0);
    int fields = 0;
    for (double &side : box)
        fields += std::fscanf(geom.get(), "%lg", &side);
    if (fields != 3)
        throw std::runtime_error("must specify exactly 3 box coordinates");
    return box;
}

Matrix make_matrix(std::size_t n, const std::vector<double> &box)
{
    Matrix a(n, Vector(n, 0.0));
    const double x = box[0];
    const double y = box[1];
    const double z = box[2];
    const double scale = 1.0 + (x + y + z) * 0.05;

    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = 0; j < n; ++j)
        {
            if (i == j)
            {
                a[i][j] = scale + 0.10 * (x / (y + z + 1.0));
            }
            else
            {
                const double dist = std::abs(static_cast<int>(i) - static_cast<int>(j));
                a[i][j] = 0.02 * std::exp(-dist / 4.0) / (1.0 + dist);
            }
        }
    }

    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = i + 1; j < n; ++j)
        {
            std::swap(a[i][j], a[j][i]);
        }
    }

    return a;
}

Vector make_rhs(std::size_t n, const std::vector<double> &box, int channel)
{
    Vector rhs(n, 0.0);
    const double x = box[0];
    const double y = box[1];
    const double z = box[2];
    const double base = 0.25 + 0.15 * (channel + 1);

    for (std::size_t i = 0; i < n; ++i)
    {
        const double view = static_cast<double>(i + 1) / static_cast<double>(std::max<std::size_t>(1, n));
        rhs[i] = base + 0.5 * view + 0.1 * ((x + y + z) / (x + 1.0 + z)) + 0.02 * channel;
    }

    return rhs;
}

Vector solve_linear_system(const Matrix &a, const Vector &rhs)
{
    const std::size_t n = a.size();
    if (n == 0)
        return {};
    if (rhs.size() != n)
        throw std::runtime_error("matrix and rhs have mismatched dimensions");

    Matrix m = a;
    Vector b = rhs;

    for (std::size_t i = 0; i < n; ++i)
    {
        std::size_t pivot = i;
        double largest = std::abs(m[i][i]);
        for (std::size_t row = i + 1; row < n; ++row)
        {
            const double val = std::abs(m[row][i]);
            if (val > largest)
            {
                largest = val;
                pivot = row;
            }
        }

        if (largest < 1e-12)
            throw std::runtime_error("singular matrix encountered while solving SLALOM C++ system");

        if (pivot != i)
        {
            std::swap(m[i], m[pivot]);
            std::swap(b[i], b[pivot]);
        }

        const double piv = m[i][i];
        for (std::size_t col = i; col < n; ++col)
            m[i][col] /= piv;
        b[i] /= piv;

        for (std::size_t row = 0; row < n; ++row)
        {
            if (row == i)
                continue;
            const double factor = m[row][i];
            if (factor == 0.0)
                continue;
            for (std::size_t col = i; col < n; ++col)
                m[row][col] -= factor * m[i][col];
            b[row] -= factor * b[i];
        }
    }

    return b;
}

Vector solve_color_system(std::size_t n, const std::vector<double> &box, int channel)
{
    const Matrix a = make_matrix(n, box);
    const Vector rhs = make_rhs(n, box, channel);
    return solve_linear_system(a, rhs);
}

std::pair<double, double> residual(const Matrix &a, const Vector &x, const Vector &rhs)
{
    double max_abs = 0.0;
    double l2 = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        double acc = 0.0;
        for (std::size_t j = 0; j < a.size(); ++j)
            acc += a[i][j] * x[j];
        const double err = std::abs(acc - rhs[i]);
        max_abs = std::max(max_abs, err);
        l2 += err * err;
    }
    return {max_abs, std::sqrt(l2)};
}

} // namespace

int main()
{
    std::cout << "slalom-cxx: C++ implementation with std::vector and dense solve\n";

    const slalom::File geom("geom", "r");
    if (!geom)
    {
        std::cerr << "slalom-cxx: 'geom' not found (looked for " << geom.path() << ")\n";
        return 1;
    }

    std::vector<double> box;
    try
    {
        box = read_box(geom);
    }
    catch (const std::exception &e)
    {
        std::cerr << "slalom-cxx: " << e.what() << '\n';
        return 1;
    }

    const MachineInfo *info = get_system_info();
    if (info == nullptr)
    {
        std::cerr << "slalom-cxx: system information unavailable.\n";
        return 1;
    }

    std::cout << "  geom      : " << geom.path() << '\n';
    std::cout << "  box       : " << box[0] << " x " << box[1] << " x " << box[2] << '\n';
    std::cout << "  machine   : " << text(info->arch) << '\n';
    std::cout << "  processor : " << text(info->proc) << '\n';
    std::cout << "  compiler  : " << text(info->compiler) << ' ' << text(info->version) << '\n';
    std::cout << "  build     : " << SLALOM_BUILD_TYPE << '\n';

    const std::size_t n = 16;
    const Matrix a = make_matrix(n, box);
    std::array<Vector, 3> colors{};
    std::array<Vector, 3> rhs{};
    std::array<double, 3> max_residual{};
    std::array<double, 3> l2_residual{};

    for (int channel = 0; channel < 3; ++channel)
    {
        rhs[channel] = make_rhs(n, box, channel);
        colors[channel] = solve_linear_system(a, rhs[channel]);
        const auto residual_pair = residual(a, colors[channel], rhs[channel]);
        max_residual[channel] = residual_pair.first;
        l2_residual[channel] = residual_pair.second;
    }

    std::cout << "\nC++ solver ready\n";
    std::cout << "  patches   : " << n << '\n';
    std::cout << "  channel R : max residual " << max_residual[0] << ", L2 " << l2_residual[0] << '\n';
    std::cout << "  channel G : max residual " << max_residual[1] << ", L2 " << l2_residual[1] << '\n';
    std::cout << "  channel B : max residual " << max_residual[2] << ", L2 " << l2_residual[2] << '\n';
    std::cout << "\nThe optional C++ port verifies the build plumbing and demonstrates a vector-based dense solve.\n";
    return 0;
}



























































































