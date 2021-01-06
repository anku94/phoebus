//========================================================================================
// (C) (or copyright) 2020. Triad National Security, LLC. All rights reserved.
//
// This program was produced under U.S. Government contract 89233218CNA000001 for Los
// Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC
// for the U.S. Department of Energy/National Nuclear Security Administration. All rights
// in the program are reserved by Triad National Security, LLC, and the U.S. Department
// of Energy/National Nuclear Security Administration. The Government is granted for
// itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide
// license in this material to reproduce, prepare derivative works, distribute copies to
// the public, perform publicly and display publicly, and to permit others to do so.
//========================================================================================

#ifndef PHOEBUS_DRIVER_HPP_
#define PHOEBUS_DRIVER_HPP_

#include <memory>

#include <parthenon/driver.hpp>

using namespace parthenon::driver::prelude;

namespace phoebus {

// TODO(JMM): What kind of driver should this be?
class PhoebusDriver : public Driver {
 public:
  PhoebusDriver(ParameterInput *pin, ApplicationInput *app_in, Mesh *pm);

  template <typename T>
  TaskCollection MakeTaskCollection(T &blocks);

  DriverStatus Execute() override;
};

parthenon::Packages_t ProcessPackages(std::unique_ptr<ParameterInput> &pin);

} // namespace phoebus

#endif // PHOEBUS_DRIVER_HPP_
