// © 2021. Triad National Security, LLC. All rights reserved.  This
// program was produced under U.S. Government contract
// 89233218CNA000001 for Los Alamos National Laboratory (LANL), which
// is operated by Triad National Security, LLC for the U.S.
// Department of Energy/National Nuclear Security Administration. All
// rights in the program are reserved by Triad National Security, LLC,
// and the U.S. Department of Energy/National Nuclear Security
// Administration. The Government is granted for itself and others
// acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works,
// distribute copies to the public, perform publicly and display
// publicly, and to permit others to do so.

#ifndef MONOPOLE_GR_MONOPOLE_GR_HPP_
#define MONOPOLE_GR_MONOPOLE_GR_HPP_

// stdlib
#include <memory>

// Spiner
#include <spiner/interpolation.hpp>

// Parthenon
#include <kokkos_abstraction.hpp>
#include <parthenon/package.hpp>
#include <utils/error_checking.hpp>

// Phoebus
#include "geometry/geometry_utils.hpp"

using namespace parthenon::package::prelude;

// TODO(JMM): Clean up notation? Or is it fine?
/*
  ASSUMPTIONS:
  -------------
  ADM split:

  ds^2 = (-alpha^2 + beta_i beta^i) dt^2 + 2 beta_i dt dx^i + gamma_{ij} dx^i dx^j

  where alpha is lapse, beta is shift, gamma is 3-metric

  with spherically symmetric ansatz:

  ^(3)ds^2 = gamma_{ij} dx^i dx^j
           = a^2 dr^2 + b^2 r^2 dOmega^2

  Areal coordinates, meaning surface area of sphere = pi r^2
  => b = 1

  Extrinsic curvature defines time-evolution of metric:

  (partial_t - Lie_beta) gamma_{ij} = -2 alpha K_{ij}

  We assume trace of extrinsic curvature vanishes:

  K = K^i_i = 0

  This is so-called "maximal slicing" and provides
  singularity-avoiding coordinates.

  Note that:

  Lie_beta gamma_{ij} = gamma_{kj} \partial_i beta^k
                        + beta^k \partial_k gamma_{ij}
                        + gamma_{ki} \partial_j beta^k

  Trace-free + spherical symmetry implies that
  only non-vanishing components of extrinsic curvature are:

  K^th_th = K^ph_ph = -(1/2) K^r_r

  so we work with K^r_r


  GOVERNING EQUATIONS, SUMMARY:
  ------------------------------
  gamm_{ij} = diag(a^2, r^2, r^2 sin^2(th))

  K^i_j = K^r_r diag(1, -1/2, -1/2)

  da/dr = a (4 + a^2 (-4 + r^2 (-3 (K^r_r)^2 + 32 pi rho)))/(8 r);

  d K^r_r/dr = 8 pi a^2 j^r - (3/r) K^r_r

  r d^2 alpha/dr^2 = a^2 r alpha ((3/2) (K^r_r)^2 + 4 pi (rho + S)) +
  ((1/a)(da/dr)r-2)dalpha/dr

  beta^r = -(1/2) alpha r K^r_r


  BOUNDARY CONDITIONS
  -------------------
  at r = 0:
     da/dr = 0
     d K^r_r /dr = 0
     dalpha/dr = 0

  Examination of the equations shows that this translates to:
     a = 1
     K = 0
     dalpha/dr = 0

  at outer boundary (idealy r = infinity):
     da/dr = (a - a^3) / 2 r OR a = 1
     K^r_r = 0
     alpha = 1 - r (dalpha/dr) OR alpha = 1


  TIME DERIVATIVES
  ----------------

  The ADM constraint equations are sufficient to determine a, K,
  alpha, beta. However, the Christoffel coefficients depend on
  derivatives of these quantities. We can determine the spatial
  derivatives by taking gradients or using the above equations. For
  time derivatives, we rely on the Einstein evolution equations:

  (\partial_t - Lie_beta) g_{mu nu} = - 2 alpha K_{mu nu}

  (\partial_t - Lie_beta) K_{i j} = -D_i D_j alpha + alpha [^{(3)}R_{ij} + K K_{ij} - 2
  K_{ik} K^k_j] + 4 pi alpha [gamma_{ij} (S - rho) - 2 S_{ij}]

  where here "D" is the covariant derivative operator on a hypersurface.

  In particular, this allows us to derive that

  da/dt = (da/dr) beta^r + a (d beta^r/dr) - alpha a K^r_r

  d alpha/dr = beta^r [ a beta^r (da/dt)/alpha + a^2 K^r_r beta^r
                        + 2 (da/dr) (1 - (beta^r)^2) - 2 a^2 beta^r (d beta^r/dr)/alpha ]

  d K^r_r/dt = beta^r (d K^r_r/dr) - (1/a^2) (d^2 alpha/dr^2) + alpha [(2/(a^3 r)) (da/dr)
                - r (K^r_r)^2] + 4 pi alpha [S - rho - 2 S^r_r]

  d K^r_r/dt = beta^r (d K^r_r/dr) - (d^2 alpha/dr^2)/a^2
              + a [2 (da/dr)/(a^2 r) - 4 (K^r_r)^2 ] + 4 pi alpha (s - rho - 2 S^r_r)


  MATTER COMPONENTS:
  ------------------

  In ADM formulation:

  rho    = TOTAL mass-energy.
  j^i    = CONSERVED 3-momentum vector
  S_{ij} = 3-stress tensor
  ---------------------------------------
  rho    = n^mu n^nu T_{mu nu} = tau + D
  j^i    = - P^{i mu} n^nu T_{mu nu} = S^i
  S_{ij} = P^mu_i P^nu_i T_{munu}

  where n^mu is unit normal of hypersurface, P^{i mu} is projector

  Translates to (in Valencia variables):

  rho    = tau + D
  j^i    = S^i
  Trc(S) = (rho_0 h + b^2)W^2 + 3(P + b^2/2) - b^i b_i

  In case of no B fields,

  trc(S) = (tau + D) + 4 P - rho_0 h

  where rho_0 here is primitive density and h is enthalpy.

  We also need S^r_r = T^r_r for the Christoffel symbols:
  T^r_r = (rho + u + P + b^2) u^r u_r + (P + (1/2) b^2) - b^r b_r
 */

namespace MonopoleGR {

constexpr int MIN_NPOINTS = 5;

/*
  Array structure:
  Slowest moving index is refinement level. 0 is finest.
  Next index is variable (if appropriate)
  Next index is grid point
 */
using Matter_t = parthenon::ParArray2D<Real>;
using Matter_host_t = typename parthenon::ParArray2D<Real>::HostMirror;

constexpr int NMAT = 4;
constexpr int NMAT_H = NMAT - 2;
enum Matter {
  RHO = 0,  // Primitive density... (0,0)-component of Tmunu
  J_R = 1,  // Radial momentum, (r,t)-component of Tmunu
  trcS = 2, // Trace of the stress tensor: S = S^i_i
  Srr = 3   // The r-r component of the stress tensor
};

namespace TOV {
constexpr int NTOV = 2;

using State_t = parthenon::ParArray2D<Real>;
using State_host_t = typename parthenon::ParArray2D<Real>::HostMirror;

constexpr int M = 0;
constexpr int P = 1;

constexpr int NINTRINSIC = 2;

constexpr int RHO0 = 0;
constexpr int EPS = 1;
  
} // namespace TOV

using Hypersurface_t = parthenon::ParArray2D<Real>;
using Hypersurface_host_t = typename parthenon::ParArray2D<Real>::HostMirror;

constexpr int NHYPER = 2;
enum Hypersurface {
  A = 0, // (r,r)-component of metric
  K = 1  // K^r_r, extrinsic curvature
};

using Alpha_t = parthenon::ParArray1D<Real>;
using Alpha_host_t = typename parthenon::ParArray1D<Real>::HostMirror;

using Beta_t = parthenon::ParArray1D<Real>;

constexpr int NGRAD = 8;
using Gradients_t = parthenon::ParArray2D<Real>;
enum Gradients {
  DADR = 0,     // dadr
  DKDR = 1,     // dKdr
  DALPHADR = 2, // dalphadr
  DBETADR = 3,  // dbetadr
  DADT = 4,     // dadt
  DALPHADT = 5, // dalphadt
  DKDT = 6,     // dKdt
  DBETADT = 7   // dbetadt
};

// TODO(JMM): Do we want this?
using Radius = Spiner::RegularGrid1D;

std::shared_ptr<StateDescriptor> Initialize(ParameterInput *pin);

TaskStatus MatterToHost(StateDescriptor *pkg);

TaskStatus IntegrateHypersurface(StateDescriptor *pkg);

TaskStatus IntegrateTov(StateDescriptor *monopolepkg, StateDescriptor *eospkg);

TaskStatus LinearSolveForAlpha(StateDescriptor *pkg);

TaskStatus SpacetimeToDevice(StateDescriptor *pkg);

template <typename Array_t>
PORTABLE_INLINE_FUNCTION Real Interpolate(const Real r, const Array_t &A,
                                          const Radius &rgrid) {
  int ix;
  Spiner::weights_t w;
  rgrid.weights(r, ix, w);
  return w[0] * A(ix) + w[1] * A(ix + 1);
}

template <typename Array_t>
PORTABLE_INLINE_FUNCTION Real Interpolate(const Real r, const Array_t &A,
                                          const Radius &rgrid, const int ivar) {
  int ix;
  Spiner::weights_t w;
  rgrid.weights(r, ix, w);
  return w[0] * A(ivar, ix) + w[1] * A(ivar, ix + 1);
}

void DumpToTxt(const std::string &filename, StateDescriptor *pkg);

} // namespace MonopoleGR

#endif // MONOPOLE_GR_MONOPOLE_GR_HPP_
