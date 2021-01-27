#include "con2prim.hpp"

namespace con2prim {

const Real rel_tolerance = 1.e-8;
const int max_iter =  20;

//static int calls = 0;
//static Real avg_iters = 0.0;

template <typename Data_t,typename T>
ConToPrimStatus ConToPrim<Data_t,T>::Solve(const VarAccessor<T> &v, const CellGeom &g) const {
  // converge on rho and T
  // constraints: rho <= D, T > 0


  const Real D = v(crho)/g.gdet;
  const Real tau = v(ceng)/g.gdet;

  // todo(jcd): really compute these when there are B fields
  Real BdotS = 0.0;
  Real Bsq = 0.0;
  const Real BdotSsq = BdotS*BdotS;

  Real Ssq = 0.0;
  Real W = 0.0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      Ssq += g.gcon[i][j] * v(cmom_lo+i)*v(cmom_lo+j);
      W += g.gcov[i][j] * v(pvel_lo+i)*v(pvel_hi+j);
    }
  }
  Ssq /= (g.gdet*g.gdet);
  W = sqrt(1.0/(1.0 - W));
  Real rho_guess = D/W;
  Real T_guess = v(tmp);

  auto sfunc = [&](const Real z, const Real Wp) {
    const Real zBsq = z + Bsq;
    return zBsq*zBsq*(Wp*Wp-1.0)/Wp - (2.0*z + Bsq)*BdotSsq/(z*z) - Ssq;
  };

  auto taufunc = [&](const Real z, const Real Wp, const Real p) {
    //std::cout << "taufunc: " << z << " " << p << " " << D << " " << tau << std::endl;
    return z + Bsq - p - Bsq/(2.0*Wp*Wp) - BdotSsq/(2.0*z*z) - D - tau;
  };

  auto Rfunc = [&](const Real rho, const Real Temp, Real res[2]) {
    const Real p = eos.PressureFromDensityTemperature(rho, Temp);
    const Real sie = eos.InternalEnergyFromDensityTemperature(rho, Temp);
    const Real Wp = D/rho;
    const Real z = (rho*(1.0 + sie) + p)*Wp*Wp;
    //std::cout << "Rfunc: " << tau << " " << rho*sie << " " << p << " " << taufunc(z, Wp, p) << std::endl;
    res[0] = sfunc(z, Wp);
    res[1] = taufunc(z, Wp, p);
  };

  int iter = 0;
  bool converged = false;
  Real res[2], resp[2];
  Real jac[2][2];
  do {
    Rfunc(rho_guess, T_guess, res);
    Real drho = 1.0e-8*rho_guess;
    Rfunc(rho_guess + drho, T_guess, resp);
    jac[0][0] = (resp[0] - res[0])/drho;
    jac[1][0] = (resp[1] - res[1])/drho;
    Real dT = 1.0e-8*T_guess;
    Rfunc(rho_guess, T_guess+dT, resp);
    jac[0][1] = (resp[0] - res[0])/dT;
    jac[1][1] = (resp[1] - res[1])/dT;

    const Real det = (jac[0][0]*jac[1][1] - jac[0][1]*jac[1][0]);
    Real delta_rho = -(res[0]*jac[1][1] - jac[0][1]*res[1])/det;
    Real delta_T = -(jac[0][0]*res[1] - jac[1][0]*res[0])/det;

    if (std::abs(delta_rho)/rho_guess < rel_tolerance &&
        std::abs(delta_T)/T_guess < rel_tolerance) {
          converged = true;
    }

    if (rho_guess + delta_rho < 0.0) {
      delta_rho = -0.8*rho_guess;
    }
    if (rho_guess + delta_rho > D) {
      delta_rho = 0.99*(D-rho_guess);
    }
    if (T_guess + delta_T < 0.0) {
      delta_T = -0.8*T_guess;
    }
    rho_guess += delta_rho;
    T_guess += delta_T;
    iter++;

    //std::cout << "iter " << iter << ": "
    //          << rho_guess << " " << T_guess << " "
    //          << delta_rho << " " << delta_T << std::endl;

  } while(converged != true && iter < max_iter);

  if(!converged) {
    std::cout << "ConToPrim Failed state:" << rho_guess << " " << T_guess << " "
                                           << v(crho) << " " 
                                           << v(cmom_lo) << " "
                                           << v(cmom_lo+1) << " "
                                           << v(cmom_lo+2) << " " 
                                           << v(ceng) << std::endl;
    return ConToPrimStatus::failure;
  }

  //calls++;
  //avg_iters += (iter - avg_iters)/calls;
  //std::cout << "avg iter = " << avg_iters << std::endl;

  v(tmp) = T_guess;
  v(prho) = rho_guess;
  v(prs) = eos.PressureFromDensityTemperature(rho_guess, T_guess);
  v(peng) = rho_guess*eos.InternalEnergyFromDensityTemperature(rho_guess, T_guess);
  v(cs) = eos.BulkModulusFromDensityTemperature(rho_guess, T_guess)/rho_guess;
  v(gm1) = v(cs)*rho_guess/v(prs);
  v(cs) = sqrt(v(cs));

  W = D/rho_guess;
  const Real z = (rho_guess + v(peng) + v(prs))*W*W;
  for (int i = 0; i < 3; i++) {
    Real sconi = 0.0;
    for (int j = 0; j < 3; j++) {
      sconi += g.gcon[i][j]*v(cmom_lo+j);
    }
    sconi /= g.gdet;
    v(pvel_lo+i) = sconi/(z + Bsq) + BdotS*0.0/(z*(z + Bsq)); // this 0.0 should be B^i
  }

  //std::cout << "Converged: " << v(prho) << " " << rho_guess << " " << T_guess << std::endl;
  return ConToPrimStatus::success;
}

template class ConToPrim<MeshBlockData<Real>,VariablePack<Real>>;
//template class ConToPrim<MeshData<Real>,MeshBlockPack<Real>>;

} // namespace con2prim
