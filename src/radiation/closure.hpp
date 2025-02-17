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

#ifndef CLOSURE_HPP_
#define CLOSURE_HPP_

#include <kokkos_abstraction.hpp>
#include <parthenon/package.hpp>
#include <utils/error_checking.hpp>
#include "geometry/geometry_utils.hpp"
#include "phoebus_utils/linear_algebra.hpp"

#include <cmath>
#include <iostream>
#include <type_traits>
#include <limits>

using namespace LinearAlgebra;

namespace radiation
{

  template <class T>
  KOKKOS_FORCEINLINE_FUNCTION
      T
      ratio(T num, T denom)
  {
    const T sgn = denom >= 0 ? 1 : -1;
    return num / (denom + sgn * std::numeric_limits<T>::min());
  }

  template <class T>
  KOKKOS_FORCEINLINE_FUNCTION int sgn(T val)
  {
    return (T(0) <= val) - (val < T(0));
  }

  /// Status of closure calculation
  enum class Status
  {
    success = 0,
    failure = 1
  };

  enum class ClosureType {Eddington, M1, MOCMC};

  /// Store results of M1 closure root find
  struct M1Result
  {
    Status status;
    int iter;
    Real xi, phi;
    Real fXi, fPhi;
    Real errXi, errPhi;
  };

  /// Holds methods for closing the radiation moment equations as well as calculating radiation
  /// moment source terms.
  template <class Vec, class Tens2>
  class Closure
  {
  public:
    //-------------------------------------------------------------------------------------
    /// Constructor just calculates the inverse 3-metric, covariant three-velocity, and the
    /// Lorentz factor for the given background state.
    template <class V, class T2>
    KOKKOS_FUNCTION
    Closure(const V con_v_in, const T2 cov_gamma_in);
    
    //-------------------------------------------------------------------------------------
    /// Calculate the update values dE and cov_dF for a linear, implicit source term update
    /// from the starred state using the optical depths tauJ = lapse kappa_a dt and 
    /// tauH = lapse (kappa_a + kappa_s) dt 
    template <class VA, class VB, class T2>
    KOKKOS_FUNCTION
      Status
      LinearSourceUpdate(const Real Estar, const VA cov_Fstar,
               const T2 con_tilPi, const Real JBB,  
               const Real tauJ, const Real tauH,
               Real *dE, VB *cov_dF);

    //-------------------------------------------------------------------------------------
    /// Calculate the momentum density flux P^i_j from J, \tilde H_i, and \tilde pi^{ij}.
    template <class V, class T2A, class T2B>
    KOKKOS_FUNCTION
        Status
        getConCovPFromPrim(const Real J, const V cov_tilH, const T2A con_tilPi,
                           T2B *concov_P);
    
    //-------------------------------------------------------------------------------------
    /// Calculate the momentum density flux P^i_j from J, \tilde H_i, and \tilde pi^{ij}.
    template <class V, class T2A, class T2B>
    KOKKOS_FUNCTION
        Status
        getConPFromPrim(const Real J, const V cov_tilH, const T2A con_tilPi,
                           T2B *con_P);

    //-------------------------------------------------------------------------------------
    /// Transform from J, \tilde H_i, and \tilde \pi_{ij} to E and F_i. Should be used for
    /// Eddington approximation (i.e. \tilde \pi_{ij} = 0) and MoCMC where \tilde \pi^{ij}
    /// is externally specified
    template <class VA, class VB, class T2>
    KOKKOS_FUNCTION
        Status
        Prim2Con(const Real J, const VA cov_H, const T2 con_tilPi, Real *E, VB *cov_F);

    //-------------------------------------------------------------------------------------
    /// Transform from J, \tilde H_i, and \tilde \pi_{ij} to E and F_i. Should be used for
    /// Eddington approximation (i.e. \tilde \pi_{ij} = 0) and MoCMC where \tilde \pi^{ij}
    /// is externally specified
    template <class VA, class VB, class T2>
    KOKKOS_FUNCTION
        Status
        Con2Prim(const Real E, const VA cov_F, const T2 con_tilPi, Real *J, VB *cov_H);

    //-------------------------------------------------------------------------------------
    /// Prim2ConM1 returns conservative variables E and F_i, as well as \tilde \pi^{ij} from
    /// the closure for a given J and \tilde H_i.
    template <class VA, class VB, class T2>
    KOKKOS_FUNCTION
        Status
        Prim2ConM1(const Real J, const VA cov_tilH, Real *E, VB *cov_F, T2 *con_tilPi);

    //-------------------------------------------------------------------------------------
    /// Con2PrimM1 returns primitive variables J, \tilde H_i, and \tilde \pi^{ij} for given
    /// E and F_i. Requires an internal root find, so a bit expensive.
    template <class VA, class VB, class T2>
    KOKKOS_FUNCTION
        M1Result
        Con2PrimM1(const Real E, const VA cov_F, Real *J, VB *cov_H, T2 *con_tilPi);
    /// Same as above, but explicitly provide guesses for ratio H/J and angle relative to
    /// basis vectors.
    template <class VA, class VB, class T2>
    KOKKOS_FUNCTION
        M1Result
        Con2PrimM1(const Real E, const VA cov_F, Real xi_guess, Real phi_guess,
                   Real *J, VB *cov_H, T2 *con_tilPi);

    //-------------------------------------------------------------------------------------
    /// Routines for raising, lowering, contracting, etc. three vectors.
    template <class VA, class VB>
    KOKKOS_FORCEINLINE_FUNCTION void lower3Vector(const VA &con_U, VB *cov_U) const
    {
      SPACELOOP(i)
      (*cov_U)(i) = 0.0;
      SPACELOOP(i)
      {
        SPACELOOP(j) { (*cov_U)(i) += con_U(j) * cov_gamma(i, j); }
      }
    }

    template <class VA, class VB>
    KOKKOS_FORCEINLINE_FUNCTION void raise3Vector(const VA &cov_U, VB *con_U) const
    {
      SPACELOOP(i)
      (*con_U)(i) = 0.0;
      SPACELOOP(i)
      {
        SPACELOOP(j) { (*con_U)(i) += cov_U(j) * con_gamma(i, j); }
      }
    }

    template <class VA, class VB>
    KOKKOS_FORCEINLINE_FUNCTION
        Real
        contractCon3Vectors(const VA &con_A, const VB &con_B) const
    {
      Real s(0.0);
      SPACELOOP(i)
      {
        SPACELOOP(j) { s += cov_gamma(i, j) * con_A(i) * con_B(j); }
      }
      return s;
    }

    template <class VA, class VB>
    KOKKOS_FORCEINLINE_FUNCTION
        Real
        contractCov3Vectors(const VA &cov_A, const VB &cov_B) const
    {
      Real s(0.0);
      SPACELOOP(i)
      {
        SPACELOOP(j) { s += con_gamma(i, j) * cov_A(i) * cov_B(j); }
      }
      return s;
    }

    template <class VA, class VB>
    KOKKOS_FORCEINLINE_FUNCTION
        Real
        contractConCov3Vectors(const VA &con_A, const VB &cov_B) const
    {
      Real s(0.0);
      SPACELOOP(i) { s += con_A(i) * cov_B(i); }
      return s;
    }

    Real v2, W, W2, W3, W4;
    Vec cov_v;
    Vec con_v;
    Real gdet, alpha;
    Vec con_beta;
    Tens2 cov_gamma;
    Tens2 con_gamma;
    //-------------------------------------------------------------------------------------
    /// Calculate \tilde \pi^{ij} and \tilde f_i = \tilde H_i/\sqrt{H_\alpha H^\alpha} from
    /// J and \tilde H_i.
    template <class V>
    KOKKOS_FUNCTION
        Status
        M1FluidPressureTensor(const Real J, const V cov_H,
                              Tens2 *con_tilPi, Vec *con_tilf);
  
  protected:
    //-------------------------------------------------------------------------------------
    /// Calculate the residuals of the M1 root equations
    template <class V>
    KOKKOS_FUNCTION
        Status
        M1Residuals(const Real E, const V cov_F,
                    const Real xi, const Real phi,
                    const Vec con_tilg, const Vec con_tild,
                    Real *fXi, Real *fPhi);


    //-------------------------------------------------------------------------------------
    /// Calculate \tilde \pi^{ij} and \tilde f_i = \tilde H_i/\sqrt{H_\alpha H^\alpha} from
    /// E, F_i, \xi = H/J, and \phi that are a root of the M1 residual equations.
    template <class V>
    KOKKOS_FUNCTION
        Status
        M1FluidPressureTensor(const Real E, const V cov_F,
                              const Real xi, const Real phi,
                              Tens2 *con_tilPi, Vec *con_tilf)
    {
      Vec con_tilg, con_tild;
      GetBasisVectors(E, cov_F, &con_tilg, &con_tild);
      return M1FluidPressureTensor(E, cov_F, xi, phi, con_tilg, con_tild, con_tilPi, con_tilf);
    }

    //-------------------------------------------------------------------------------------
    /// Calculate \tilde \pi^{ij} and \tilde f_i = \tilde H_i/\sqrt{H_\alpha H^\alpha} from
    /// E, F_i, \xi = H/J, and \phi that are a root of the M1 residual equations. Basis
    /// vectors are explicitly passed so they don't need to be recalculated repeatedly.
    template <class V>
    KOKKOS_FUNCTION
        Status
        M1FluidPressureTensor(const Real E, const V cov_F,
                              const Real xi, const Real phi,
                              const Vec con_tilg, const Vec con_tild,
                              Tens2 *con_tilPi, Vec *con_tilf);

    //-------------------------------------------------------------------------------------
    /// Perform NR iteration to find a root (xi, phi) of the M1 residual equations for a
    /// given E and F_i
    template <class V>
    KOKKOS_FUNCTION
        M1Result
        SolveClosure(Real E, V cov_F,
                     Real *xi_out, Real *phi_out,
                     const Real xi_guess = 0.5, const Real phi_guess = 3.14159);

    //-------------------------------------------------------------------------------------
    /// Calculate the basis vectors for \tilde f_i as described in the notes.
    template <class V>
    KOKKOS_FUNCTION
        Status
        GetBasisVectors(const Real E, const V cov_F,
                        Vec *con_tilg, Vec *con_tild);

    template <class T2>
    KOKKOS_FORCEINLINE_FUNCTION void GetTilPiContractions(const T2 &con_tilPi, Vec *cov_vTilPi, Real *vvTilPi)
    {
      Vec con_vTilPi;
      *vvTilPi = 0.0;
      SPACELOOP(i)
      {
        con_vTilPi(i) = 0.0;
        SPACELOOP(j)
        {
          con_vTilPi(i) += cov_v(j) * con_tilPi(i, j);
        }
        *vvTilPi += cov_v(i) * con_vTilPi(i);
      }
      lower3Vector(con_vTilPi, cov_vTilPi);
    }

    KOKKOS_FORCEINLINE_FUNCTION
    Real closure(Real xi)
    {
      // MEFD Closure
      return (1.0 - 2*std::pow(xi, 2) + 4*std::pow(xi,3))/3.0;
      //return (1.0 + 2 * xi * xi) / 3;
    }

  private:
  };

  template <class Vec, class Tens2>
  template <class V, class T2>
  KOKKOS_FUNCTION
  Closure<Vec, Tens2>::Closure(const V con_v_in, const T2 cov_gamma_in)
  {
    SPACELOOP(i)
    {
      SPACELOOP(j)
      {
        cov_gamma(i, j) = cov_gamma_in(i, j);
      }
    }

    SPACELOOP(i)
    con_v(i) = con_v_in(i);

    gdet = matrixInverse3x3(cov_gamma, con_gamma);

    lower3Vector(con_v, &cov_v);
    v2 = 0.0;
    SPACELOOP(i)
    v2 += con_v(i) * cov_v(i);
    W = 1 / std::sqrt(1 - v2);
    W2 = W * W;
    W3 = W * W2;
    W4 = W * W3;
  }
   
  template <class Vec, class Tens2>
  template <class VA, class VB, class T2>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::LinearSourceUpdate(const Real Estar, const VA cov_Fstar,
                                    const T2 con_tilPi, const Real JBB,  
                                    const Real tauJ, const Real tauH,
                                    Real *dE, VB *cov_dF) {
    
    Real vvTilPi; 
    Vec cov_vTilPi; 
    GetTilPiContractions(con_tilPi, &cov_vTilPi, &vvTilPi);
    
    Real A[2][2], x[2], b[2]; 
    A[0][0] = (4*W2-1)/3 + W2*vvTilPi + W*tauJ; 
    A[0][1] = 2*W + tauH;  
    A[1][0] = A[0][0] - 1 - tauJ/W;
    A[1][1] = A[0][1] - 1/W; 
    
    Real vFstar = contractConCov3Vectors(con_v, cov_Fstar); 
    b[0] = Estar + W*tauJ*JBB; 
    b[1] = vFstar + (W2-1)/W*tauJ*JBB;

    SolveAxb2x2(A, b, x);
    //if (std::isnan(b[0])) throw 1; 

    Real &J = x[0];
    Real &zeta = x[1]; //v^i \tilde H_i  

    *dE = (4*W2 - 1 + 3*W2*vvTilPi) / 3 * J + 2*W*zeta - Estar;      
    SPACELOOP(i)
      (*cov_dF)(i) = (cov_v(i)*tauH*(4*W2/3*J + W*zeta) + tauH*cov_vTilPi(i) 
                   + tauJ*W2*cov_v(i)*(JBB-J) + W*cov_Fstar(i))/(W+tauH) - cov_Fstar(i); 
    
    return Status::success;  
  }
  
  template <class Vec, class Tens2>
  template <class VA, class VB, class T2>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::Prim2Con(const Real J, const VA cov_H,
                                    const T2 con_tilPi,
                                    Real *E, VB *cov_F)
  {
    Real vvPi;
    Vec cov_vPi;
    GetTilPiContractions(con_tilPi, &cov_vPi, &vvPi);

    Real vH = 0.0;
    SPACELOOP(i)
    vH += con_v(i) * cov_H(i);
    *E = (4 * W2 - 1 + 3 * W2 * vvPi) / 3 * J + 2 * W * vH;
    SPACELOOP(i)
    (*cov_F)(i) = 4 * W2 / 3 * cov_v(i) * J + W * cov_v(i) * vH + W * cov_H(i) + J * cov_vPi(i);
    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class VA, class VB, class T2>
  Status Closure<Vec, Tens2>::Con2Prim(const Real E, const VA cov_F,
                                       const T2 con_tilPi,
                                       Real *J, VB *cov_tilH)
  {
    Real vvTilPi;
    Vec cov_vTilPi;
    GetTilPiContractions(con_tilPi, &cov_vTilPi, &vvTilPi);

    double vF = 0.0;
    SPACELOOP(i)
    vF += con_v(i) * cov_F(i);

    // lam is proportional to the determinant of the 2x2 linear system relating
    // E and v_i F^i to J and v_i H^i for fixed tilde pi^ij
    const Real lam = (2.0 * W2 + 1.0) / 3.0 + (2 * W4 - 3 * W2) * vvTilPi;

    // zeta = v_i H^i
    // const Real zeta = -W/lam*(((4*W2-4)/3 + vvPi)*E
    //                   -((4*W2-1)/3 + W2*vvPi)*vF);
    // a = 4 W^2/3 J + W zeta
    const Real a = ratio(W2, lam) * ((4 * W2 / 3 - vvTilPi) * E - ((4 * W2 + 1) / 3 - W2 * vvTilPi) * vF);

    // Calculate fluid rest frame (i.e. primitive) quantities
    *J = ratio((2 * W2 - 1) * E - 2 * W2 * vF, lam);
    SPACELOOP(i)
    (*cov_tilH)(i) = (cov_F(i) - (*J) * cov_vTilPi(i) - cov_v(i) * a) / W;
    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class VA, class VB, class T2>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::Prim2ConM1(const Real J, const VA cov_H,
                                      Real *E, VB *cov_F, T2 *con_tilPi)
  {
    Vec con_tilf;
    M1FluidPressureTensor(J, cov_H, con_tilPi, &con_tilf);
    Prim2Con(J, cov_H, *con_tilPi, E, cov_F);

    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class VA, class VB, class T2>
  KOKKOS_FUNCTION
      M1Result
      Closure<Vec, Tens2>::Con2PrimM1(const Real E, const VA cov_F,
                                      Real *J, VB *cov_H, T2 *con_tilPi)
  {
    Real xi, phi;
    Vec con_tilf;

    // Get the basis vectors for the search
    Vec con_tilg, con_tild;
    GetBasisVectors(E, cov_F, &con_tilg, &con_tild);

    // Perform an Eddington approximation Con2Prim to get initial guesses
    Tens2 con_PiEdd;
    SPACELOOP2(i, j) con_PiEdd(i,j) = 0.0;
    Real JEdd;
    Vec cov_HEdd;
    Con2Prim(E, cov_F, con_PiEdd, &JEdd, &cov_HEdd);
    Real vHEdd = contractConCov3Vectors(con_v, cov_HEdd);
    Real HEdd = sqrt(contractCov3Vectors(cov_HEdd, cov_HEdd) - vHEdd * vHEdd);

    xi = std::min(ratio(HEdd, JEdd), 0.99);
    phi = 1.000001 * acos(-1);

    return Con2PrimM1(E, cov_F, xi, phi, J, cov_H, con_tilPi);
  }

  template <class Vec, class Tens2>
  template <class VA, class VB, class T2>
  KOKKOS_FUNCTION
      M1Result
      Closure<Vec, Tens2>::Con2PrimM1(const Real E, const VA cov_F,
                                      const Real xi_guess, const Real phi_guess,
                                      Real *J, VB *cov_H, T2 *con_tilPi)
  {
    Real xi, phi;
    Vec con_tilf;
    auto status = SolveClosure(E, cov_F, &xi, &phi, xi_guess, phi_guess);
    M1FluidPressureTensor(E, cov_F, xi, phi, con_tilPi, &con_tilf);
    Con2Prim(E, cov_F, *con_tilPi, J, cov_H);
    return status;
  }
  
  template <class Vec, class Tens2>
  template <class V, class T2A, class T2B>
  KOKKOS_FORCEINLINE_FUNCTION
      Status
      Closure<Vec, Tens2>::getConPFromPrim(const Real J, const V cov_tilH,
                                              const T2A con_tilPi,
                                              T2B *con_P)
  {
    Vec con_tilH;
    raise3Vector(cov_tilH, &con_tilH);
    SPACELOOP2(i,j) (*con_P)(i, j) = 4.0 / 3.0 * W2 * con_v(i) * con_v(j) * J 
            + W * (con_v(i) * con_tilH(j) + con_v(j) * con_tilH(i)) + J * con_tilPi(i, j) 
            + J/3.0*con_gamma(i,j);
    
    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class V, class T2A, class T2B>
  KOKKOS_FORCEINLINE_FUNCTION
      Status
      Closure<Vec, Tens2>::getConCovPFromPrim(const Real J, const V cov_tilH,
                                              const T2A con_tilPi,
                                              T2B *concov_P)
  {
    Vec con_tilH;
    Tens2 concov_tilPi;
    SPACELOOP(i)
    {
      SPACELOOP(j)
      {
        concov_tilPi(i, j) = 0.0;
        SPACELOOP(k)
        {
          concov_tilPi(i, j) += con_tilPi(i, k) * cov_gamma(k, j);
        }
      }
    }
    raise3Vector(cov_tilH, &con_tilH);
    SPACELOOP(i)
    {
      SPACELOOP(j)
      {
        (*concov_P)(i, j) = 4.0 / 3.0 * W2 * con_v(i) * cov_v(j) * J 
            + W * (con_v(i) * cov_tilH(j) + cov_v(j) * con_tilH(i)) + J * concov_tilPi(i, j);
      }
      (*concov_P)(i, i) += J / 3.0;
    }
    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class V>
  KOKKOS_FUNCTION
      M1Result
      Closure<Vec, Tens2>::SolveClosure(Real E, V cov_F, Real *xi_out, Real *phi_out,
                                        const Real xi_guess, const Real phi_guess)
  {
    const int max_iter = 30;
    const Real tol = 1.e6 * std::numeric_limits<Real>::epsilon();
    const Real eps = std::sqrt(10 * std::numeric_limits<Real>::epsilon());
    const Real delta = 20 * std::numeric_limits<Real>::epsilon();

    // Normalize input and make sure we are slightly away from zero
    cov_F(0) = ratio(cov_F(0), E) + 1.e3 * sgn(cov_F(0)) * std::numeric_limits<Real>::min();
    cov_F(1) = ratio(cov_F(1), E) + 1.e3 * sgn(cov_F(1)) * std::numeric_limits<Real>::min();
    cov_F(2) = ratio(cov_F(2), E) + 1.e3 * sgn(cov_F(2)) * std::numeric_limits<Real>::min();
    E = 1.0;

    // Get the basis vectors for the search
    Vec con_tilg, con_tild;
    GetBasisVectors(E, cov_F, &con_tilg, &con_tild);

    int iter = 0;
    Real fXi = 1.e3;
    Real fPhi = 1.e3;
    Real Jac[2][2], f[2], dx[2];
    Real err_phi, err_xi;
    Real xi = xi_guess;
    Real phi = phi_guess;
    phi = acos(-1.0);
    do
    {
      Real fPhi_up, fPhi_lo;
      Real fXi_up, fXi_lo;

      const Real xi_up = std::min(xi * (1.0 + eps) + delta, 1.0);
      const Real xi_lo = std::max(xi * (1.0 - eps) - delta, 0.0);
      M1Residuals(E, cov_F, xi_up, phi, con_tilg, con_tild, &fXi_up, &fPhi_up);
      M1Residuals(E, cov_F, xi_lo, phi, con_tilg, con_tild, &fXi_lo, &fPhi_lo);
      Jac[0][0] = (fXi_up - fXi_lo) / (xi_up - xi_lo);
      Jac[1][0] = (fPhi_up - fPhi_lo) / (xi_up - xi_lo);

      const Real phi_up = phi * (1.0 + eps) + delta;
      const Real phi_lo = phi * (1.0 - eps) - delta;
      M1Residuals(E, cov_F, xi, phi_up, con_tilg, con_tild, &fXi_up, &fPhi_up);
      M1Residuals(E, cov_F, xi, phi_lo, con_tilg, con_tild, &fXi_lo, &fPhi_lo);
      Jac[0][1] = (fXi_up - fXi_lo) / (phi_up - phi_lo);
      Jac[1][1] = (fPhi_up - fPhi_lo) / (phi_up - phi_lo);

      M1Residuals(E, cov_F, xi, phi, con_tilg, con_tild, &fXi, &fPhi);
      f[0] = fXi;
      f[1] = fPhi;

      SolveAxb2x2(Jac, f, dx);

      // Keep phi fixed for the first two updates
      if (iter < max_iter)
      {
        dx[0] = fXi / Jac[0][0];
        dx[1] = 0.0;
        fPhi = 0.0;
      }

      Real corrected_delta_xi = std::min(dx[0], xi - delta * xi);
      corrected_delta_xi = std::max(corrected_delta_xi, xi - (1 - delta * (1 - xi)));
      Real alpha = ratio(corrected_delta_xi, dx[0]);
      alpha = std::min(alpha, 0.5 * 3.14159 / fabs(dx[1]));
      xi -= alpha * dx[0];
      phi -= alpha * dx[1];

      err_xi = std::abs(dx[0]) / (std::abs(xi) + delta);
      err_phi = std::abs(dx[1]) / (std::abs(phi) + delta);
      ++iter;

    } while (iter < max_iter && (fabs(fXi) > tol || fabs(fPhi) > tol));

    *xi_out = xi;
    *phi_out = phi;
    M1Result result{Status::success, iter, xi, phi, fXi, fPhi, err_xi, err_phi};
    if (std::isnan(xi))
      result.status = Status::failure;
    if (iter == max_iter)
      result.status = Status::failure;
    return result;
  }

  template <class Vec, class Tens2>
  template <class V>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::M1Residuals(const Real E, const V cov_F,
                                       const Real xi, const Real phi,
                                       const Vec con_tilg, const Vec con_tild,
                                       Real *fXi, Real *fPhi)
  {

    // Calculate rest frame fluid variables
    Tens2 con_tilPi;
    Vec con_tilf;
    M1FluidPressureTensor(E, cov_F, xi, phi, con_tilg, con_tild, &con_tilPi, &con_tilf);
    Real J;
    Vec cov_tilH;
    Con2Prim(E, cov_F, con_tilPi, &J, &cov_tilH);

    // Construct the residual functions
    Real H(0.0), vTilH(0.0), Hf(0.0), vTilf(0.0);
    SPACELOOP(i)
    {
      SPACELOOP(j) { H += cov_tilH(i) * cov_tilH(j) * con_gamma(i, j); }
    }
    SPACELOOP(i)
    vTilH += con_v(i) * cov_tilH(i);
    SPACELOOP(i)
    Hf += cov_tilH(i) * con_tilf(i);
    SPACELOOP(i)
    vTilf += cov_v(i) * con_tilf(i);
    Hf -= vTilf * vTilH;
    H = std::sqrt(H - vTilH * vTilH);
    *fXi = xi - ratio(H, J);
    *fPhi = Hf - H;

    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class V>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::M1FluidPressureTensor(const Real J, const V cov_tilH,
                                                 Tens2 *con_tilPi, Vec *con_tilf)
  {
    Vec con_tilH;
    raise3Vector(cov_tilH, &con_tilH);
    Real H(0.0), vH(0.0);
    SPACELOOP(i)
    H += cov_tilH(i) * con_tilH(i);
    SPACELOOP(i)
    vH += cov_tilH(i) * con_v(i);
    H = std::sqrt(H - vH * vH);
    SPACELOOP(i)
    (*con_tilf)(i) = ratio(con_tilH(i), H);
    const Real xi = std::max(std::min(1.0, ratio(H, J)), 0.0);
    const Real athin = 0.5 * (3 * closure(xi) - 1);
    // Calculate the projected rest frame radiation pressure tensor
    SPACELOOP(i)
    {
      SPACELOOP(j)
      {
        (*con_tilPi)(i, j) = (*con_tilf)(i) * (*con_tilf)(j) - (W2 * con_v(i) * con_v(j) + con_gamma(i, j)) / 3;
        (*con_tilPi)(i, j) *= athin;
      }
    }

    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class V>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::M1FluidPressureTensor(const Real E, const V cov_F,
                                                 const Real xi, const Real phi,
                                                 const Vec con_tilg, const Vec con_tild,
                                                 Tens2 *con_tilPi, Vec *con_tilf)
  {

    // Build projected flux direction from basis vectors and given angle
    const Real s = sin(phi);
    const Real c = cos(phi);
    SPACELOOP(i)
    (*con_tilf)(i) = -c * con_tilg(i) + s * con_tild(i);

    // Calculate the closure interpolation
    const Real athin = 0.5 * (3 * closure(xi) - 1);

    // Calculate the projected rest frame radiation pressure tensor
    SPACELOOP(i)
    {
      SPACELOOP(j)
      {
        (*con_tilPi)(i, j) = (*con_tilf)(i) * (*con_tilf)(j) - (W2 * con_v(i) * con_v(j) + con_gamma(i, j)) / 3;
        (*con_tilPi)(i, j) *= athin;
      }
    }

    return Status::success;
  }

  template <class Vec, class Tens2>
  template <class V>
  KOKKOS_FUNCTION
      Status
      Closure<Vec, Tens2>::GetBasisVectors(const Real E, const V cov_F,
                                           Vec *con_tilg, Vec *con_tild)
  {
    // Build projected basis vectors for flux direction
    // These vectors are actually fixed, so there is no need to recalculate
    // them every step in the root find 
    Vec con_F;
    raise3Vector(cov_F, &con_F);
    Real Fmag(0.0), vl(0.0), v2(0.0);
    SPACELOOP(i)
    Fmag += cov_F(i) * con_F(i);
    Fmag = std::sqrt(Fmag);
    SPACELOOP(i)
    vl += cov_F(i) * con_v(i);
    vl = ratio(vl, Fmag);
    SPACELOOP(i)
    v2 += cov_v(i) * con_v(i);
    const Real lam = ratio(1.0, W * (1 - vl));

    // This goes to zero if F and v lie along one another and can create problems, probably should be
    // checking elsewhere that they are not along one another
    const Real aa = std::sqrt(v2 * (1 + 10 * std::numeric_limits<Real>::epsilon()) - vl * vl);
    SPACELOOP(i)
    {
      (*con_tilg)(i) = ratio(con_F(i) * lam, Fmag) - W * con_v(i);
      (*con_tild)(i) = ratio(ratio(con_F(i), Fmag) * (1 - lam / W) + con_v(i), aa);
    }
    //if (aa < 1.e-6) (*con_tild) = {{0,0,0}};

    return Status::success;
  }
} // namespace radiation

#endif // CLOSURE_HPP_
