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

#include <memory>
#include <string>
#include <vector>

//TODO(JCD): this should be exported by parthenon
#include <refinement/refinement.hpp>

// Local Includes
#include "fluid/fluid.hpp"
#include "geometry/geometry.hpp"
#include "microphysics/eos_phoebus/eos_phoebus.hpp"
#include "phoebus_driver.hpp"
#include "radiation/radiation.hpp"

using namespace parthenon::driver::prelude;

namespace phoebus {

// *************************************************//
// define the application driver. in this case,    *//
// that mostly means defining the MakeTaskList     *//
// function.                                       *//
// *************************************************//
PhoebusDriver::PhoebusDriver(ParameterInput *pin, ApplicationInput *app_in, Mesh *pm)
    : EvolutionDriver(pin, app_in, pm),
      integrator(std::make_unique<StagedIntegrator>(pin)) {

  // fail if these are not specified in the input file
  pin->CheckRequired("parthenon/mesh", "ix1_bc");
  pin->CheckRequired("parthenon/mesh", "ox1_bc");
  pin->CheckRequired("parthenon/mesh", "ix2_bc");
  pin->CheckRequired("parthenon/mesh", "ox2_bc");

  // warn if these fields aren't specified in the input file
  pin->CheckDesired("parthenon/mesh", "refinement");
  pin->CheckDesired("parthenon/mesh", "numlevel");

  dt_init = pin->GetOrAddReal("parthenon/time", "dt_init", 1.e300);
  dt_init_fact = pin->GetOrAddReal("parthenon/time", "dt_init_fact", 1.0);
}

TaskListStatus PhoebusDriver::Step() {
  static bool first_call = true;
  TaskListStatus status;
  Real dt_trial = tm.dt;
  if (first_call) dt_trial = std::min(dt_trial, dt_init);
  dt_trial *= dt_init_fact;
  dt_init_fact = 1.0;
  if (tm.time + dt_trial > tm.tlim) dt_trial = tm.tlim-tm.time;
  tm.dt = dt_trial;
  integrator->dt = dt_trial;

  for (int stage = 1; stage <= integrator->nstages; stage++) {
    TaskCollection tc = RungeKuttaStage(stage);
    status = tc.Execute();
    if (status != TaskListStatus::complete) break;
  }

  {
    TaskCollection tc = RadiationStep();
    status = tc.Execute();
  }

  return status;
}

TaskCollection PhoebusDriver::RungeKuttaStage(const int stage) {
  using namespace ::parthenon::Update;
  TaskCollection tc;
  TaskID none(0);

  BlockList_t &blocks = pmesh->block_list;

  const Real beta = integrator->beta[stage - 1];
  const Real dt = integrator->dt;
  const auto &stage_name = integrator->stage_name;

  std::vector<std::string> src_names({conserved_variables::momentum, conserved_variables::energy});

  auto num_independent_task_lists = blocks.size();
  TaskRegion &async_region = tc.AddRegion(num_independent_task_lists);

  for (int ib = 0; ib < num_independent_task_lists; ib++) {
    auto pmb = blocks[ib].get();
    auto &tl = async_region[ib];

    // first make other useful containers
    auto &base = pmb->meshblock_data.Get();
    if (stage == 1) {
      pmb->meshblock_data.Add("dUdt", base);
      for (int i = 1; i < integrator->nstages; i++) {
        pmb->meshblock_data.Add(stage_name[i], base);
      }
      pmb->meshblock_data.Add("geometric source terms", base, src_names);
    }

    // pull out the container we'll use to get fluxes and/or compute RHSs
    auto &sc0 = pmb->meshblock_data.Get(stage_name[stage - 1]);
    // pull out a container we'll use to store dU/dt.
    // This is just -flux_divergence in this example
    auto &dudt = pmb->meshblock_data.Get("dUdt");
    // pull out the container that will hold the updated state
    // effectively, sc1 = sc0 + dudt*dt
    auto &sc1 = pmb->meshblock_data.Get(stage_name[stage]);
    // pull out a container for the geometric source terms
    auto &gsrc = pmb->meshblock_data.Get("geometric source terms");

    auto start_recv = tl.AddTask(none, &MeshBlockData<Real>::StartReceiving,
                                 sc1.get(), BoundaryCommSubset::all);

    auto hydro_flux = tl.AddTask(none, fluid::CalculateFluxes, sc0.get());
    auto flux_ct = tl.AddTask(hydro_flux, fluid::FluxCT, sc0.get());
    auto geom_src = tl.AddTask(none, fluid::CalculateFluidSourceTerms, sc0.get(), gsrc.get());

    auto send_flux =
        tl.AddTask(flux_ct, &MeshBlockData<Real>::SendFluxCorrection, sc0.get());

    auto recv_flux = tl.AddTask(
        flux_ct, &MeshBlockData<Real>::ReceiveFluxCorrection, sc0.get());

    // compute the divergence of fluxes of conserved variables
    auto flux_div =
        tl.AddTask(recv_flux, parthenon::Update::FluxDivergence<MeshBlockData<Real>>, sc0.get(), dudt.get());

    auto add_rhs = tl.AddTask(flux_div|geom_src, SumData<std::string,MeshBlockData<Real>>,
                              src_names, dudt.get(), gsrc.get(), dudt.get());

    auto avg_container = tl.AddTask(add_rhs, AverageIndependentData<MeshBlockData<Real>>,
                                    sc0.get(), base.get(), beta);
    // apply du/dt to all independent fields in the container
    auto update_container = tl.AddTask(avg_container, UpdateIndependentData<MeshBlockData<Real>>,
                                       sc0.get(), dudt.get(), beta*dt, sc1.get());

    // update ghost cells
    auto send = tl.AddTask(update_container,
                           &MeshBlockData<Real>::SendBoundaryBuffers, sc1.get());
    auto recv =
        tl.AddTask(send, &MeshBlockData<Real>::ReceiveBoundaryBuffers, sc1.get());
    auto fill_from_bufs =
        tl.AddTask(recv, &MeshBlockData<Real>::SetBoundaries, sc1.get());
    auto clear_comm_flags =
        tl.AddTask(fill_from_bufs, &MeshBlockData<Real>::ClearBoundary, sc1.get(),
                   BoundaryCommSubset::all);

    auto prolongBound = tl.AddTask(fill_from_bufs, parthenon::ProlongateBoundaries, sc1);

    // set physical boundaries
    auto set_bc =
      tl.AddTask(prolongBound, parthenon::ApplyBoundaryConditions, sc1);

    // fill in derived fields
    auto fill_derived =
        tl.AddTask(set_bc, parthenon::Update::FillDerived<MeshBlockData<Real>>, sc1.get());

    // estimate next time step
    if (stage == integrator->nstages) {
      auto new_dt = tl.AddTask(
          fill_derived, parthenon::Update::EstimateTimestep<MeshBlockData<Real>>, sc1.get());

      auto divb = tl.AddTask(set_bc, fluid::CalculateDivB, sc1.get());

      // Update refinement
      if (pmesh->adaptive) {
        //using tag_type = TaskStatus(std::shared_ptr<MeshBlockData<Real>> &);
        auto tag_refine = tl.AddTask(
            fill_derived, parthenon::Refinement::Tag<MeshBlockData<Real>>, sc1.get());
      }
    }
  }

  return tc;
}

TaskCollection PhoebusDriver::RadiationStep() {
  //using namespace ::parthenon::Update;
  TaskCollection tc;
  TaskID none(0);

  BlockList_t &blocks = pmesh->block_list;

  const Real dt = integrator->dt;
  printf("idt: %e\n", integrator->dt);
  const auto &stage_name = integrator->stage_name;

  auto num_independent_task_lists = blocks.size();
  TaskRegion &async_region = tc.AddRegion(num_independent_task_lists);

  for (int ib = 0; ib < num_independent_task_lists; ib++) {
    auto pmb = blocks[ib].get();
    StateDescriptor *rad = pmb->packages.Get("radiation").get();
    auto rad_active = rad->Param<bool>("active");
    if (rad_active) {
      auto &tl = async_region[ib];
      auto &sc0 = pmb->meshblock_data.Get(stage_name[integrator->nstages]);

      //auto sc = pmb->swarm_data.Get();

      auto calculate_four_force = tl.AddTask(none, radiation::CalculateRadiationFourForce,
       sc0.get(), dt);

      auto apply_four_force = tl.AddTask(calculate_four_force,
       radiation::ApplyRadiationFourForce, sc0.get(), dt);
    }
  }

  return tc;
}

parthenon::Packages_t ProcessPackages(std::unique_ptr<ParameterInput> &pin) {
  parthenon::Packages_t packages;

  packages.Add(Microphysics::EOS::Initialize(pin.get()));
  packages.Add(Geometry::Initialize(pin.get()));
  if (pin->GetBoolean("physics", "hydro") == true) {
    packages.Add(fluid::Initialize(pin.get()));
  }
  if (pin->GetBoolean("physics", "rad") == true) {
    packages.Add(radiation::Initialize(pin.get()));
  }

  return packages;
}

} // namespace phoebus
