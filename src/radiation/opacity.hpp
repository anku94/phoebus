// TODO(BRR) use singularity-eos

#ifndef RADIATION_OPACITY_HPP_
#define RADIATION_OPACITY_HPP_

namespace radiation {

#define SCONST (3)

#define OPACITY_MODEL_TOPHAT
#define OPACITY_MODEL_GRAY
#define OPACITY_MODEL OPACITE_MODEL_GRAY

KOKKOS_INLINE_FUNCTION
Real GetBnu(const Real T, const Real nu) {
  Real x = pc.h*nu/(pc.kb*T);
  Real Bnu = CONST*(2.*pc.h*nu*nu*nu/(pc.c*pc.c))*1./(exp(x) + 1.);
  return Bnu;
}

KOKKOS_INLINE_FUNCTION
Real GetB(const Real T) {
  return 8.*pow(M_PI,5)*pow(pc.kb,4)*SCONST*pow(T,4)*KAPPA/(15.*pow(pc.c,2)*pow(pc.h,3));
}

KOKKOS_INLINE_FUNCTION
Real Getkappanu(const Real Ye, const Real T, const Real nu, const NeutrinoSpecies s) {
  Real Bnu = GetBnu(T, nu);
  Real jnu = Getjnu(Ye, s, nu);
  return jnu/Bnu;
}

#if OPACITY_MODEL == OPACITY_MODEL_TOPHAT

#define C (1.e15)
#define numax (1.e17)
#define numin (1.e15)

KOKKOS_INLINE_FUNCTION
Real Getyf(Real Ye, NeutrinoSpecies s) {
  if (s == NeutrinoSpecies::Electron) {
    return 2. * Ye;
  } else if (s == NeutrinoSpecies::ElectronAnti) {
    return 1. - 2. * Ye;
  } else {
    return 0.;
  }
}

KOKKOS_INLINE_FUNCTION
Real Getjnu(const Real Ye, const NeutrinoSpecies s, const Real nu) {
  if (nu > numin && nu < numax) {
    return C*Getyf(Ye, s)/(4.*M_PI);
  } else {
    return 0.;
  }
}

KOKKOS_INLINE_FUNCTION
Real GetJnu(const Real Ye, const NeutrinoSpecies s, const Real nu) {
  if (nu > numin && nu < numax) {
    return C*Getyf(Ye, s);
    //return 4.*M_PI*C*Getyf(Ye, s);
  } else {
    return 0.;
  }
}

KOKKOS_INLINE_FUNCTION
Real GetJ(const Real Ye, const NeutrinoSpecies s) {
  Real Bc = C * (numax - numin);
  Real J = Bc * Getyf(Ye, s);
  return J;
}

KOKKOS_INLINE_FUNCTION
Real GetJye(const Real rho_cgs, const Real Ye, const NeutrinoSpecies s) {
  Real Ac = pc.mp / (pc.h * rho_cgs) * C * log(numax / numin);
  return rho_cgs * Ac * Getyf(Ye, s);
}

#undef C
#undef numax
#undef numin

#elif OPACITY_MODEL == OPACITY_MODEL_GRAY

#define KAPPA (1.0) # cgs

KOKKOS_INLINE_FUNCTION
Real Getjnu(const Real T, const Real Ye, const NeutrinoSpecies s, const Real nu) {
  Real Bnu = GetBnu(T, nu);
  return KAPPA*Bnu;
}

KOKKOS_INLINE_FUNCTION
Real GetJnu(const Real T, const Real Ye, const NeutrinoSpecies s, const Real nu) {
  return 4.*M_PI*Getjnu(T, Ye, s, nu);
}

KOKKOS_INLINE_FUNCTION
Real GetJ(const Real T, const Real Ye, const NeutrinoSpecies s) {
  Real GetB(T);
  return KAPPA*B;
}

KOKKOS_INLINE_FUNCTION
Real GetJye(const Real T, const Real rho_cgs, const Real Ye, const NeutrinoSpecies s) {
  const Real zeta3 = 1.20206;
  return 12.*pow(pc.kb,3)*pc.mp*M_PI*SCONST*pow(T,3)*KAPPA*zeta3/(pow(pc.c,2)*pow(pc.h,3));
}

#endif

} // namespace radiation

#undef KAPPA
#undef SCONST

#endif // RADIATION_OPACITY_HPP_
