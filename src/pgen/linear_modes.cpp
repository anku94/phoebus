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

// #include <complex>
#include <Kokkos_Complex.hpp>
#include <sstream>
#include <typeinfo>

#include "geometry/geometry.hpp"

#include "pgen/pgen.hpp"

// Relativistic hydro linear modes.
// TODO: Make this 3D instead of 2D.

using Kokkos::complex;
using Geometry::NDFULL;
using Geometry::NDSPACE;

namespace linear_modes {

void ProblemGenerator(MeshBlock *pmb, ParameterInput *pin) {

<<<<<<< HEAD
  // TEMPORARY
  {
    Real a = 0.9375;
    Real r = 4.;
    Real th = M_PI/4.;
    //Real X[4] = {0, r, th, M_PI/2.};
    Real X[4] = {0, log(r), 0.25, M_PI/2.};

    //auto metric = Geometry::SphericalKerrSchild(a);
    auto metric = Geometry::FMKS();

    Real gcov[4][4];
    metric.SpacetimeMetric(X[0], X[1], X[2], X[3], gcov);
    Real gcon[4][4];
    metric.SpacetimeMetricInverse(X[0], X[1], X[2], X[3], gcon);
    printf("gcov:\n");
    SPACETIMELOOP2(mu, nu) {
      printf("  [%i][%i] = %e\n", mu, nu, gcov[mu][nu]);
    }
    printf("gcon:\n");
    SPACETIMELOOP2(mu, nu) {
      printf("  [%i][%i] = %e\n", mu, nu, gcon[mu][nu]);
    }
    printf("gdet:\n");
    printf("  %e\n", metric.DetG(X[0], X[1], X[2], X[3]));
    printf("alpha:\n");
    printf("  %e\n", metric.Lapse(X[0], X[1], X[2], X[3]));
    Real beta_con[3];
    metric.ContravariantShift(X[0], X[1], X[2], X[3], beta_con);
    printf("betacon:\n");
    SPACELOOP(mu) {
      printf("  [%i] = %e\n", mu, beta_con[mu]);
    }
    Real gammacov[3][3];
    metric.Metric(X[0], X[1], X[2], X[3], gammacov);
    Real gammacon[3][3];
    metric.MetricInverse(X[0], X[1], X[2], X[3], gammacon);
    printf("gammacov:\n");
    SPACELOOP2(mu, nu) {
      printf("  [%i][%i] = %e\n", mu, nu, gammacov[mu][nu]);
    }
    printf("gammacon:\n");
    SPACELOOP2(mu, nu) {
      printf("  [%i][%i] = %e\n", mu, nu, gammacon[mu][nu]);
    }
    printf("gammadet:\n");
    printf("  %e\n", metric.DetGamma(X[0], X[1], X[2], X[3]));
    Real conn[4][4][4];
    metric.ConnectionCoefficient(X[0], X[1], X[2], X[3], conn);
    Real dg[4][4][4];
    metric.MetricDerivative(X[0], X[1], X[2], X[3], dg);
    printf("Connection:\n");
    SPACETIMELOOP3(mu,nu,lam) {
      printf("  [%i][%i][%i] = %e\n", mu,nu,lam,conn[mu][nu][lam]);
    }
    printf("Metric Derivative:\n");
    SPACETIMELOOP3(mu,nu,lam) {
      printf("  [%i][%i][%i] = %e\n", mu,nu,lam,dg[mu][nu][lam]);
    }
    printf("Grad log alpha:\n");
    Real gla[4];
    metric.GradLnAlpha(X[0], X[1], X[2], X[3], gla);
    SPACETIMELOOP(mu) {
      printf("  [%i] = %e\n", mu,gla[mu]);
    }
    exit(-1);
  }
=======
  PARTHENON_REQUIRE(typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Minkowski),
    "Problem \"linear_modes\" requires \"Minkowski\" geometry!");
>>>>>>> origin/main

  auto &rc = pmb->meshblock_data.Get();
  const int ndim = pmb->pmy_mesh->ndim;

  PackIndexMap imap;
  auto v = rc->PackVariables({"p.density",
                              "p.velocity",
                              "p.energy",
                              fluid_prim::bfield,
                              "pressure",
                              "temperature",
                              "gamma1",
                              "cs"},
                              imap);

  const int irho = imap["p.density"].first;
  const int ivlo = imap["p.velocity"].first;
  const int ivhi = imap["p.velocity"].second;
  const int ieng = imap["p.energy"].first;
  const int ib_lo = imap[fluid_prim::bfield].first;
  const int ib_hi = imap[fluid_prim::bfield].second;
  const int iprs = imap["pressure"].first;
  const int itmp = imap["temperature"].first;
  const int nv = ivhi - ivlo + 1;

  const Real gam = pin->GetReal("eos", "Gamma");

  PARTHENON_REQUIRE_THROWS(nv == 3, "3 have 3 velocities");

  const std::string mode = pin->GetOrAddString("hydro_modes", "mode", "entropy");
  const std::string physics = pin->GetOrAddString("hydro_modes", "physics", "mhd");
  const Real amp = pin->GetReal("hydro_modes", "amplitude");

  // Parameters
  double rho0 = 1.;
  double ug0 = 1.;
  double u10 = 0.;
  double u20 = 0.;
  double u30 = 0.;
  double B10 = 1.;
  double B20 = 0.;
  double B30 = 0.;

  // Wavevector
  constexpr double kk = 2*M_PI;
  double k1 = kk;
  double k2 = kk;

  complex<double> omega = 0.;
  complex<double> drho = 0.;
  complex<double> dug = 0.;
  complex<double> du1 = 0.;
  complex<double> du2 = 0.;
  complex<double> du3 = 0.;
  complex<double> dB1 = 0.;
  complex<double> dB2 = 0.;
  complex<double> dB3 = 0.;

  if (physics == "hydro") {
    if (mode == "entropy") {
      omega = complex<double>(0, 2.*M_PI/10.);
      drho = 1.;
      dug = 0.;
      du1 = 0.;
      //u10 = 0.1; // Uniform advection
    } else if (mode == "sound") {
      if (ndim == 1) {
        omega = complex<double>(0., 2.7422068833892093);
        drho = 0.5804294924639215;
        dug = 0.7739059899518946;
        du1 = -0.2533201985524494;
      } else if (ndim == 2) {
        omega = complex<double>(0., 3.8780661653218766);
        drho = 0.5804294924639213;
        dug = 0.7739059899518947;
        du1 = 0.1791244302079596;
        du2 = 0.1791244302079596;
      } else {
        PARTHENON_FAIL("ndim == 3 not supported!");
      }
    } else {
      std::stringstream msg;
      msg << "Mode \"" << mode << "\" not recognized!";
      PARTHENON_FAIL(msg);
    }
  } else if (physics == "mhd") {
    if (mode == "slow") {
      omega = complex<double>(0., 2.41024185339);
      drho = 0.558104461559;
      dug = 0.744139282078;
      du1 = -0.277124827421;
      du2 = 0.063034892770;
      dB1 = -0.164323721928;
      dB2 = 0.164323721928;
    } else if (mode == "alfven") {
      omega = complex<double>(0., 3.44144232573);
      du3 = 0.480384461415;
      dB3 = 0.877058019307;
    } else if (mode == "fast") {
      omega = complex<double>(0., 5.53726217331);
      drho = 0.476395427447;
      dug = 0.635193903263;
      du1 = -0.102965815319;
      du2 = -0.316873207561;
      dB1 = 0.359559114174;
      dB2 = -0.359559114174;
    } else {
      std::stringstream msg;
      msg << "Mode \"" << mode << "\" not recognized!";
      PARTHENON_FAIL(msg);
    }
  } else {
    std::stringstream msg;
    msg << "Physics option \"" << physics << "\" not recognized!";
    PARTHENON_FAIL(msg);
  }
  Real tf = 2.*M_PI/omega.imag();
  Real cs = omega.imag() / (std::sqrt(2)*kk);

  auto &coords = pmb->coords;
  auto eos = pmb->packages.Get("eos")->Param<singularity::EOS>("d.EOS");
  auto gpkg = pmb->packages.Get("geometry");
  auto geom = Geometry::GetCoordinateSystem(rc.get());

  IndexRange ib = pmb->cellbounds.GetBoundsI(IndexDomain::entire);
  IndexRange jb = pmb->cellbounds.GetBoundsJ(IndexDomain::entire);
  IndexRange kb = pmb->cellbounds.GetBoundsK(IndexDomain::entire);

  Real a_snake, k_snake, alpha, betax, betay, betaz;
  alpha = 1;
  a_snake = k_snake = betax = betay = betaz = 0;
  if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Snake) ||
      typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Inchworm)) {
    a_snake = gpkg->Param<Real>("a");
    k_snake = gpkg->Param<Real>("k");
    alpha = gpkg->Param<Real>("alpha");
    betay = gpkg->Param<Real>("vy");
    PARTHENON_REQUIRE_THROWS(alpha > 0, "lapse must be positive");

    printf("a, k, alpha, beta = %g, %g, %g, %g\n",
	   a_snake, k_snake, alpha, betay);
    tf /= alpha;
  }
  if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Lumpy)) {
    a_snake = gpkg->Param<Real>("a");
    k_snake = gpkg->Param<Real>("k");
    betax = gpkg->Param<Real>("betax");
  }
  if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::BoostedMinkowski)) {
    betax = gpkg->Param<Real>("vx");
    betay = gpkg->Param<Real>("vy");
    betaz = gpkg->Param<Real>("vz");
  }

  // Set final time to be one period
  pin->SetReal("parthenon/time", "tlim", tf);
  tf = pin->GetReal("parthenon/time", "tlim");
  std::cout << "Resetting final time to 1 wave period: "
	    << tf
	    << std::endl;
  std::cout << "Wave frequency is: "
	    << 1./tf
	    << std::endl;
  std::cout << "Wave speed is: "
	    << cs
	    << std::endl;

  pmb->par_for(
    "Phoebus::ProblemGenerator::Linear_Modes", kb.s, kb.e, jb.s, jb.e, ib.s, ib.e,
    KOKKOS_LAMBDA(const int k, const int j, const int i) {
      Real x = coords.x1v(i);
      Real y = coords.x2v(j);

      if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Snake)) {
        y = y - a_snake*sin(k_snake*x);
      }
      if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Inchworm)) {
        x = x - a_snake*sin(k_snake*x);
      }

      const double mode = amp*cos(k1*x + k2*y);

      double rho = rho0 + (drho*mode).real();
      v(irho, k, j, i) = rho;
      double ug = ug0 + (dug*mode).real();
      double Pg = (gam - 1.)*ug;
      v(ieng, k, j, i) = ug;
      v(iprs, k, j, i) = Pg;
      // This line causes NaNs and I don't know why
      //v(iprs, k, j, i) = eos.PressureFromDensityInternalEnergy(rho, v(ieng, k, j, i)/rho);
      v(itmp, k, j, i) = eos.TemperatureFromDensityInternalEnergy(rho, v(ieng, k, j, i)/rho);
      if (ivhi > 0) {
        v(ivlo, k, j, i) = u10 + (du1*mode).real();
      }
      if (ivhi >= 2) {
        v(ivlo + 1, k, j, i) = u20 + (du2*mode).real();
      }
      if (ivhi >= 3) {
        v(ivlo + 2, k, j, i) = u30 + (du3*mode).real();
      }
      if (ib_hi >= 1) {
        v(ib_lo, k, j, i) = B10 + (dB1*mode).real();
      }
      if (ib_hi >= 2) {
        v(ib_lo + 1, k, j, i) = B20 + (dB2*mode).real();
      }
      if (ib_hi >= 3) {
        v(ib_lo + 2, k, j, i) = B30 + (dB3*mode).real();
      }

      if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Snake) ||
          typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Inchworm) ||
          typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Lumpy) ||
	  typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::BoostedMinkowski)) {
        PARTHENON_REQUIRE(ivhi == 3, "Only works for 3D velocity!");
        // Transform velocity
        Real vsq = 0.;
        Real gcov[NDFULL][NDFULL] = {0};
        Real vcon[NDSPACE] = {v(ivlo, k, j, i), v(ivlo+1, k, j, i), v(ivlo+2, k, j, i)};
        geom.SpacetimeMetric(CellLocation::Cent, k, j, i, gcov);
//<<<<<<< HEAD
//        Real lapse = geom.Lapse(CellLocation::Cent, k, j, i);
//        Real shift[NDSPACE];
//        //geom.ContravariantShift(CellLocation::Cent, k, j, i);
//=======
//>>>>>>> 41fe7ca7ee405458907ea7ae00e7a76aaec74c72
        Real gcov_mink[4][4] = {0};
        gcov_mink[0][0] = -1.;
        gcov_mink[1][1] = 1.;
        gcov_mink[2][2] = 1.;
        gcov_mink[3][3] = 1.;
        SPACELOOP2(m, n) {
          //vsq += gcov[m+1][n+1]*vcon[m]*vcon[n];
          vsq += gcov_mink[m+1][n+1]*vcon[m]*vcon[n];
        }
        Real Gamma = 1./sqrt(1. - vsq);
        Real ucon[NDFULL] = {Gamma, // alpha = 1 in Minkowski
                             Gamma*v(ivlo, k, j, i),
                             Gamma*v(ivlo+1, k, j, i),
                             Gamma*v(ivlo+2, k, j, i)};
        Real J[NDFULL][NDFULL] = {0};
        if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Snake)) {
          J[0][0] = 1/alpha;
	  J[2][0] = -betay/alpha;
          J[2][1] = a_snake*k_snake*cos(k_snake*x);
	  J[1][1] = J[2][2] = J[3][3] = 1;
	} else if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::BoostedMinkowski)) {
	  J[0][0] = J[1][1] = J[2][2] = J[3][3] = 1;
	  J[1][0] = -betax;
	  J[2][0] = -betay;
	  J[3][0] = -betaz;
        } else if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Inchworm)) {
          PARTHENON_FAIL("This geometry isn't supported with a J!");
        } else if (typeid(PHOEBUS_GEOMETRY) == typeid(Geometry::Lumpy)) {
          PARTHENON_FAIL("This geometry isn't supported with a J!");
        }
        Real ucon_transformed[NDFULL] = {0, 0, 0, 0};
        SPACETIMELOOP(mu) SPACETIMELOOP(nu){
          ucon_transformed[mu] += J[mu][nu]*ucon[nu];
        }
        const Real lapse = geom.Lapse(CellLocation::Cent, k, j, i);
        Gamma = lapse * ucon_transformed[0];
        Real shift[NDSPACE];
        geom.ContravariantShift(CellLocation::Cent, k, j, i, shift);
        v(ivlo, k, j, i) = ucon_transformed[1]/Gamma + shift[0]/lapse;
        v(ivlo+1, k, j, i) = ucon_transformed[2]/Gamma + shift[1]/lapse;
        v(ivlo+2, k, j, i) = ucon_transformed[3]/Gamma + shift[2]/lapse;

        // Enforce zero B fields for now
        if (ib_hi >= 3) {
          v(ib_lo, k, j, i) = 0.;
          v(ib_lo + 1, k, j, i) = 0.;
          v(ib_lo + 2, k, j, i) = 0.;
        }

        if (i == 64 && j == 64) {
          printf("x = %e y = %e\n", x, y);
          printf("rho ug P v1 v2: %e %e %e %e %e\n",
            v(irho, k, j, i), v(ieng, k, j, i), v(iprs, k, j, i), v(ivlo, k, j, i),
            v(ivlo+1, k, j, i));
        }
      }
    });

  fluid::PrimitiveToConserved(rc.get());
}

} // namespace linear_modes
