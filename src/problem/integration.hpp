#pragma once

#include "eigentypes.hpp"
#include "problem/dynamics.hpp"
namespace altro {
namespace problem {
/**
 * @brief Interface class for explicit integration methods for dynamical
 * systems.
 *
 * All sub-classes must implement the `Integrate` method that integrates an
 * arbitrary functor
 * over some time step.
 *
 * @tparam DynamicsFunc the type of the function-like object that evaluates a
 * first-order
 * ordinary differential equation with the following signature:
 * dynamics(const VectorXd& x, const VectorXd& u, float t) const
 *
 * See `ContinuousDynamics` class for the expected interface.
 */
class ExplicitIntegrator
{
 public:
  virtual ~ExplicitIntegrator(){};

  /**
   * @brief Integrate the dynamics over a given time step
   *
   * @param[in] dynamics ContinuousDynamics object to evaluate the continuous
   * dynamics
   * @param[in] x state vector
   * @param[in] u control vector
   * @param[in] t independent variable (e.g. time)
   * @param[in] h discretization step length (e.g. time step)
   * @return VectorXd state vector at the end of the time step
   */
  virtual VectorXd Integrate(const ContinuousDynamics& dynamics, const VectorXd& x, const VectorXd& u, 
                             float t, float h) const = 0;

  /**
   * @brief Evaluate the Jacobian of the discrete dynamics
   *
   * Will typically call the continuous dynamics Jacobian.
   *
   * @pre Jacobian must be initialized
   *
   * @param[in] dynamics ContinuousDynamics object to evaluate the continuous
   * dynamics
   * @param[in] x state vector
   * @param[in] u control vector
   * @param[in] t independent variable (e.g. time)
   * @param[in] h discretization step length (e.g. time step)
   * @param[out] jac discrete dynamics Jacobian evaluated at x, u, t.
   */
  virtual void Jacobian(const ContinuousDynamics& dynamics, const VectorXd& x, const VectorXd& u,
                float t, float h, MatrixXd& jac) const = 0;
};

/**
 * @brief Basic explicit Euler integration
 *
 * Simplest integrator that requires only a single evaluationg of the continuous
 * dynamics but suffers from significant integration errors.
 *
 * @tparam DynamicsFunc
 */
class ExplicitEuler final : public ExplicitIntegrator
{
 public:
  VectorXd Integrate(const ContinuousDynamics& dynamics, const VectorXd& x, const VectorXd& u, 
                     float t, float h) const override {
    return x + dynamics(x, u, t) * h;
  }
  void Jacobian(const ContinuousDynamics& dynamics, const VectorXd& x, const VectorXd& u,
                float t, float h, MatrixXd& jac) const override {
    int n = x.size();
    int m = u.size();
    dynamics.Jacobian(x, u, t, jac);
    jac = MatrixXd::Identity(n, n + m) + jac * h;
  }
};

/**
 * @brief Fourth-order explicit Runge Kutta integrator.
 *
 * De-facto explicit integrator for many robotics applications.
 * Good balance between accuracy and computational effort.
 *
 * @tparam DynamicsFunc
 */
class RungeKutta4 final : public ExplicitIntegrator
{
 public:
  VectorXd Integrate(const ContinuousDynamics& dynamics, const VectorXd& x, const VectorXd& u, 
                     float t, float h) const override {
    VectorXd k1 = dynamics(x, u, t) * h;
    VectorXd k2 = dynamics(x + k1 * 0.5, u, t + 0.5 * h) * h;
    VectorXd k3 = dynamics(x + k2 * 0.5, u, t + 0.5 * h) * h;
    VectorXd k4 = dynamics(x + k3, u, t + h) * h;
    VectorXd xnext = x + (k1 + 2 * k2 + 2 * k3 + k4) / 6;
    return xnext;
  }
  void Jacobian(const ContinuousDynamics& dynamics, const VectorXd& x, const VectorXd& u,
                float t, float h, MatrixXd& jac) const override {
    dynamics.Jacobian(x, u, t, jac);
    int n = dynamics.StateDimension();
    int m = dynamics.ControlDimension();

    VectorXd k1 = dynamics(x, u, t) * h;
    VectorXd k2 = dynamics(x + k1 * 0.5, u, t + 0.5 * h) * h;
    VectorXd k3 = dynamics(x + k2 * 0.5, u, t + 0.5 * h) * h;
    // VectorXd k4 = dynamics(x + k3, u, t) * h;

    // TODO(bjackson): SW-14463 avoid allocation temporary matrices
    dynamics.Jacobian(x, u, t, jac);
    MatrixXd A1 = jac.topLeftCorner(n, n);
    MatrixXd B1 = jac.topRightCorner(n, m);
    dynamics.Jacobian(x + 0.5 * k1, u, 0.5 * t, jac);
    MatrixXd A2 = jac.topLeftCorner(n, n);
    MatrixXd B2 = jac.topRightCorner(n, m);
    dynamics.Jacobian(x + 0.5 * k2, u, 0.5 * t, jac);
    MatrixXd A3 = jac.topLeftCorner(n, n);
    MatrixXd B3 = jac.topRightCorner(n, m);
    dynamics.Jacobian(x + k3, u, t, jac);
    MatrixXd A4 = jac.topLeftCorner(n, n);
    MatrixXd B4 = jac.topRightCorner(n, m);

    MatrixXd dA1 = A1 * h;
    MatrixXd dA2 = A2 * (MatrixXd::Identity(n, n) + 0.5 * dA1) * h;
    MatrixXd dA3 = A3 * (MatrixXd::Identity(n, n) + 0.5 * dA2) * h;
    MatrixXd dA4 = A4 * (MatrixXd::Identity(n, n) + dA3) * h;

    MatrixXd dB1 = B1 * h;
    MatrixXd dB2 = B2 * h + 0.5 * A2 * dB1 * h;
    MatrixXd dB3 = B3 * h + 0.5 * A3 * dB2 * h;
    MatrixXd dB4 = B4 * h + A4 * dB3 * h;

    jac.topLeftCorner(n, n) =
        MatrixXd::Identity(n, n) + (dA1 + 2 * dA2 + 2 * dA3 + dA4) / 6;
    jac.topRightCorner(n, m) = (dB1 + 2 * dB2 + 2 * dB3 + dB4) / 6;
  }
};

}  // namespace problem 
}  // namespace altro
