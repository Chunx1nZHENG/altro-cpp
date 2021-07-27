#pragma once

#include "altro/problem/integration.hpp"

namespace altro {
namespace problem {

template <class Model, class Integrator = RungeKutta4>
class DiscretizedModel : public DiscreteDynamics {
 public:
  explicit DiscretizedModel(const Model& model) : model_(model) {}

  void EvaluateInplace(const VectorXdRef& x,
                       const VectorXdRef& u, const float t,
                       const float h, Eigen::Ref<VectorXd> xnext) const override {
    integrator_.Integrate(model_, x, u, t, h, xnext);
  }

  void Jacobian(const VectorXdRef& x,
                const VectorXdRef& u, const float t,
                const float h, Eigen::Ref<MatrixXd> jac) const override {
    integrator_.Jacobian(model_, x, u, t, h, jac);
  }

  void Hessian(const VectorXdRef& x,
               const VectorXdRef& u, const float t,
               const float h, const VectorXdRef& b,
               Eigen::Ref<MatrixXd> hess) const override {
    ALTRO_UNUSED(x);
    ALTRO_UNUSED(u);
    ALTRO_UNUSED(t);
    ALTRO_UNUSED(h);
    ALTRO_UNUSED(b);
    ALTRO_UNUSED(hess);
  }

  bool HasHessian() const override { return model_.HasHessian(); }
  int StateDimension() const override { return model_.StateDimension(); }
  int ControlDimension() const override { return model_.ControlDimension(); }

 private:
  Model model_;
  Integrator integrator_;
};

}  // namespace problem
}  // namespace altro
