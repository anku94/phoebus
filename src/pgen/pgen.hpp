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

#ifndef _PGEN_H_
#define _PGEN_H_

// Parthenon includes
#include <utils/error_checking.hpp>
#include <parthenon/package.hpp>
using namespace parthenon::package::prelude;

// singularity includes
#include <singularity-eos/eos/eos.hpp>

// internal includes
#include "fluid/fluid.hpp"
#include "radiation/radiation.hpp"
#include "geometry/geometry.hpp"
#include "phoebus_utils/variables.hpp"

// add the name of a namespace that contains your new ProblemGenerator
#define FOREACH_PROBLEM                                                                  \
  PROBLEM(phoebus)                                                                       \
  PROBLEM(advection)                                                                     \
  PROBLEM(shock_tube)                                                                    \
  PROBLEM(linear_modes)                                                                  \
  PROBLEM(thin_cooling)                                                                  \
  PROBLEM(leptoneq)                                                                      \
  PROBLEM(kelvin_helmholtz)                                                              \
  PROBLEM(rhs_tester)                                                                    \
  PROBLEM(sedov)                                                                         \
  PROBLEM(blandford_mckee)                                                               \
  PROBLEM(bondi)                                                                         \
  PROBLEM(radiation_advection)                                                           \
  PROBLEM(homogeneous_sphere)                                                            \
  PROBLEM(torus)

// if you need problem-specific modifications to inputs, add the name here
#define FOREACH_MODIFIER                                                                 \
  MODIFIER(phoebus)                                                                      \
  MODIFIER(torus)

/*
// DO NOT TOUCH THE MACROS BELOW
*/

// declare all the problem generators
#define PROBLEM(name) namespace name { void ProblemGenerator(MeshBlock *pmb, ParameterInput *pin); }
  FOREACH_PROBLEM
#undef PROBLEM

// declare all the modifiers
#define MODIFIER(name) namespace name { void ProblemModifier(ParameterInput *pin); }
  FOREACH_MODIFIER
#undef MODIFIER

namespace phoebus {

// make a map so we get all the function pointers available for lookup by name
static std::map<std::string, std::function<void(MeshBlock *pmb, ParameterInput *pin)>> pgen_dict ({
#define PROBLEM(name) {#name, name::ProblemGenerator} ,
  FOREACH_PROBLEM
#undef PROBLEM
});

static std::map<std::string, std::function<void(ParameterInput *pin)>> pmod_dict ({
#define MODIFIER(name) {#name, name::ProblemModifier} ,
  FOREACH_MODIFIER
#undef MODIFIER
});

/*
// END OF UNTOUCHABLE MACRO SECTION
*/

KOKKOS_FUNCTION
Real energy_from_rho_P(const singularity::EOS &eos, const Real rho, const Real P,
		       const Real emin, const Real emax, const Real Ye = 0.0);

} // namespace phoebus

#endif
