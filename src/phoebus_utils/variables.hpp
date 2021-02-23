#ifndef PHOEBUS_UTILS_VARIABLES_HPP_
#define PHOEBUS_UTILS_VARIABLES_HPP_

namespace primitive_variables {
  constexpr char density[] = "p.density";
  constexpr char velocity[] = "p.velocity";
  constexpr char energy[] = "p.energy";
  constexpr char bfield[] = "p.bfield";
  constexpr char ye[] = "p.ye";
  constexpr char pressure[] = "pressure";
  constexpr char temperature[] = "temperature";
  constexpr char gamma1[] = "gamma1";
  constexpr char cs[] = "cs";
}

namespace conserved_variables {
  constexpr char density[] = "c.density";
  constexpr char momentum[] = "c.momentum";
  constexpr char energy[] = "c.energy";
  constexpr char bfield[] = "c.bfield";
  constexpr char ye[] = "c.ye";
}

#endif // PHOEBUS_UTILS_VARIABLES_HPP_