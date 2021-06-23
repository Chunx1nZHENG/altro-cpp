#pragma once

#include <iostream>

#include "altro/common/knotpoint.hpp"
#include "altro/common/state_control_sized.hpp"
#include "altro/eigentypes.hpp"
#include "altro/problem/dynamics.hpp"
#include "altro/utils/assert.hpp"

namespace altro {
namespace ilqr {

template <int n, int m>
class DynamicsExpansion : public StateControlSized<n, m> {
  using JacType = Eigen::Matrix<double, n, AddSizes(n, m)>;

 public:
  explicit DynamicsExpansion(int state_dim, int control_dim)
      : StateControlSized<n, m>(state_dim, control_dim),
        jac_(JacType::Zero(state_dim, state_dim + control_dim)) {}

  Eigen::Block<JacType, n, n> GetA() {
    return jac_.template topLeftCorner<n, n>(this->StateDimension(),
                                             this->StateDimension());
  }
  Eigen::Block<JacType, n, m> GetB() {
    return jac_.template topRightCorner<n, m>(this->StateDimension(),
                                              this->ControlDimension());
  }
  JacType& GetJacobian() { return jac_; };

  template <int n2, int m2, class Dynamics>
  void CalcExpansion(const Dynamics& model,
                     const KnotPoint<n2, m2>& z) {
    CalcExpansion(model, z.State(), z.Control(), z.GetTime(), z.GetStep());
  }

  void CalcExpansion(const problem::DiscreteDynamics& model,
                     const Eigen::Ref<const VectorXd>& x,
                     const Eigen::Ref<const VectorXd>& u, const float t,
                     const float h) {
    model.Jacobian(x, u, t, h, jac_);
  }

  // Include this just to provide a more descriptive error message
  void CalcExpansion(const problem::ContinuousDynamics& model,
                     const Eigen::Ref<const VectorXd>& x,
                     const Eigen::Ref<const VectorXd>& u, const float t,
                     const float h) {
    ALTRO_UNUSED(model);
    ALTRO_UNUSED(x);
    ALTRO_UNUSED(u);
    ALTRO_UNUSED(t);
    ALTRO_UNUSED(h);
    throw std::runtime_error(
        "Cannot call CalcExpansion on Continuous Dynamics.");
  }

 private:
  JacType jac_;
};

}  // namespace ilqr
}  // namespace altro