#include <eigen3/Eigen/Dense>
#include <gtest/gtest.h>
#include <iostream>

#include "altro/common/trajectory.hpp"
#include "altro/ilqr/cost_expansion.hpp"
#include "altro/ilqr/ilqr.hpp"
#include "altro/problem/discretized_model.hpp"
#include "altro/problem/problem.hpp"
#include "examples/quadratic_cost.hpp"
#include "examples/quadratic_cost.hpp"
#include "examples/triple_integrator.hpp"

constexpr int dof = 2;
constexpr int n_static = 3 * dof;
constexpr int m_static = dof;
constexpr int HEAP = Eigen::Dynamic;
using ModelType =
    altro::problem::DiscretizedModel<altro::examples::TripleIntegrator>;
using CostFunType = altro::examples::QuadraticCost;

class TripleIntegratoriLQRTest : public ::testing::Test {
 protected:
  void SetUp() {
    xf(0) = 1;
    xf(1) = 2;
    x0 = - xf;
  }

  CostFunType GenCostFun(bool term = false) {
    Eigen::VectorXd uref = Eigen::VectorXd::Zero(m_static);
    CostFunType qcost =
        CostFunType::LQRCost((term ? Qf : Q), R * !term, xf, uref, term);
    return qcost;
  }

  template <int n_size, int m_size>
  altro::Trajectory<n_size, m_size> InitialTrajectory() {
    altro::Trajectory<n_size, m_size> Z(n, m, N);
    Z.SetUniformStep(h);
    return Z;
  }

  template <int n_size, int m_size>
  void RolloutZeroControls(altro::Trajectory<n_size, m_size> &Z) {
    for (int k = 0; k < N; ++k) {
      Z.State(k) = x0;
      Z.Control(k).setZero();
    }
    Z.State(N) = x0;
  }

  altro::problem::Problem MakeProblem() {
    // Initialize Problem
    altro::problem::Problem prob(N);

    // Cost Function
    std::shared_ptr<CostFunType> costfun_ptr =
        std::make_shared<CostFunType>(GenCostFun());
    std::shared_ptr<CostFunType> costfun_term_ptr =
        std::make_shared<CostFunType>(GenCostFun(true));
    prob.SetCostFunction(costfun_ptr, 0, N);
    prob.SetCostFunction(costfun_term_ptr, N, N + 1);

    // Dynamics
    altro::examples::TripleIntegrator model_cont(dof);
    std::shared_ptr<ModelType> model = std::make_shared<ModelType>(model_cont);
    prob.SetDynamics(model, 0, N);

    // Initial State
    prob.SetInitialState(x0);

    return prob;
  }

  template <int n_size, int m_size>
  altro::ilqr::iLQR<n_size, m_size> MakeSolver() {
    altro::problem::Problem prob = MakeProblem();
    altro::ilqr::iLQR<n_size, m_size> solver(prob);

    std::shared_ptr<altro::Trajectory<n_size, m_size>> traj_ptr =
        std::make_shared<altro::Trajectory<n_size, m_size>>(
            InitialTrajectory<n_size, m_size>());

    solver.SetTrajectory(traj_ptr);
    return solver;
  }

  int N = 10;
  float h = 0.1;
  int n = n_static;
  int m = m_static;
  Eigen::MatrixXd Q = Eigen::VectorXd::Constant(n_static, 1.0).asDiagonal();
  Eigen::MatrixXd R = Eigen::VectorXd::Constant(m_static, 0.001).asDiagonal();
  Eigen::MatrixXd Qf = Eigen::VectorXd::Constant(n_static, 1e5).asDiagonal();
  Eigen::VectorXd xf = Eigen::VectorXd::Zero(n_static);
  Eigen::VectorXd x0 = Eigen::VectorXd::Zero(n_static);
};

TEST_F(TripleIntegratoriLQRTest, BuildProblem) {
  altro::problem::Problem prob = MakeProblem();
  EXPECT_TRUE(prob.IsFullyDefined());
}

TEST_F(TripleIntegratoriLQRTest, iLQRConstructorStatic) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();
  EXPECT_EQ(solver.NumSegments(), N);
}

TEST_F(TripleIntegratoriLQRTest, iLQRConstructorDynamic) {
  altro::ilqr::iLQR<HEAP, HEAP> solver = MakeSolver<HEAP, HEAP>();
  EXPECT_EQ(solver.NumSegments(), N);
}

TEST_F(TripleIntegratoriLQRTest, CostExpansion) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();
  RolloutZeroControls(*Z);

  solver.UpdateExpansions();
  altro::ilqr::KnotPointFunctions<n_static, m_static> &kpf =
      solver.GetKnotPointFunction(0);
  EXPECT_TRUE(kpf.GetCostExpansion().dxdx().isApprox(Q));
  EXPECT_TRUE(kpf.GetCostExpansion().dudu().isApprox(R));
  EXPECT_TRUE(kpf.GetCostExpansion().dx().isApprox(Q * (x0 - xf)));
  EXPECT_TRUE(kpf.GetCostExpansion().du().isApproxToConstant(0.0));

  kpf = solver.GetKnotPointFunction(N);
  EXPECT_TRUE(kpf.GetCostExpansion().dxdx().isApprox(Qf));
  EXPECT_TRUE(kpf.GetCostExpansion().dx().isApprox(Qf * (x0 - xf)));
}

TEST_F(TripleIntegratoriLQRTest, DynamicsExpansion) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();
  RolloutZeroControls(*Z);

  solver.UpdateExpansions();

  altro::ilqr::KnotPointFunctions<n_static, m_static>& kpf =
      solver.GetKnotPointFunction(0);
  
  Eigen::MatrixXd A(n,n);
  Eigen::MatrixXd B(n,m);
  // clang-autoformat off
  A << 1, 0, 0.1,   0, 0.005,     0,
       0, 1,   0, 0.1,     0, 0.005,
       0, 0,   1,   0,   0.1,     0,
       0, 0,   0,   1,     0,   0.1, 
       0, 0,   0,   0,     1,     0, 
       0, 0,   0,   0,     0,     1;
  B << 1 / 6e3, 0, 
       0, 1 / 6e3, 
       5e-3, 0, 
       0, 5e-3, 
       0.1, 0, 
       0, 0.1;
  // clang-autoformat on

  for (int k = 0; k < N; ++k) {
    kpf = solver.GetKnotPointFunction(k);
    EXPECT_TRUE(kpf.GetDynamicsExpansion().GetA().isApprox(A, 1e-6));
    EXPECT_TRUE(kpf.GetDynamicsExpansion().GetB().isApprox(B, 1e-6));
  }
}

TEST_F(TripleIntegratoriLQRTest, BackwardPass) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();
  RolloutZeroControls(*Z);

  solver.UpdateExpansions();

  solver.BackwardPass();

  Eigen::VectorXd ctg_grad0(n);
  Eigen::VectorXd d0(m);
  // clang-autoformat off

  // These numbers were computed using Altro.jl
  ctg_grad0 << -389.04658272629644, -778.0931654525915, -181.40881931288234,
      -362.81763862576514, -9.704677110465038, -19.409354220930084;
  d0 << 127.9313782698078, 255.862756539616;
  // clang-autoformat on

  EXPECT_TRUE(solver.GetKnotPointFunction(0).GetCostToGoGradient().isApprox(
      ctg_grad0, 1e-2));
  EXPECT_TRUE(
      solver.GetKnotPointFunction(0).GetFeedforwardGain().isApprox(d0, 1e-2));
}

TEST_F(TripleIntegratoriLQRTest, Cost) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();
  solver.Rollout();
  double J0 = solver.Cost();
  EXPECT_DOUBLE_EQ(J0, 100 + 1e6);

  // Modify the trajectory and make sure the cost changes
  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();
  for (int k = 0; k <= N; ++k) {
    Z->State(k) = xf;
    Z->Control(k).setZero();
  }
  double J = solver.Cost();

  // should be zero since the entire trajectory is at the goal
  EXPECT_DOUBLE_EQ(J, 0);

  // Test on a different trajectory
  altro::Trajectory<n_static, m_static> Z_copy(*Z);
  Z_copy.SetZero();
  double J2 = solver.Cost(Z_copy);
  J = solver.Cost();
  EXPECT_GT(std::abs(J - J2), 1e-3);
  EXPECT_DOUBLE_EQ(J, 0);
}

TEST_F(TripleIntegratoriLQRTest, Rollout) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();

  EXPECT_TRUE(Z->State(N).isApprox(Eigen::VectorXd::Zero(n)));
  solver.Rollout();
  for (const auto z : *Z) {
    EXPECT_TRUE(z.State().isApprox(solver.GetInitialState()));
  }

  for (auto z : *Z) {
    z.Control().setConstant(0.1);
  }
  solver.Rollout();
  std::shared_ptr<altro::problem::DiscreteDynamics> model =
      solver.GetKnotPointFunction(0).GetModelPtr();
  altro::KnotPoint<n_static, m_static> &z = Z->GetKnotPoint(0);
  Eigen::VectorXd x1 =
      model->Evaluate(z.State(), z.Control(), z.GetTime(), z.GetTime());
  x1.isApprox(Z->State(1));
}

TEST_F(TripleIntegratoriLQRTest, ForwardPass) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();

  solver.Rollout();
  solver.UpdateExpansions();
  solver.BackwardPass();
  double J0 = solver.Cost();
  solver.ForwardPass();
  double J = solver.Cost();
  EXPECT_LT(J, J0);
  double J_expected = 1945.2329136;  // from Altro.jl
  EXPECT_LT(std::abs(J - J_expected), 1e-3);
}

TEST_F(TripleIntegratoriLQRTest, TwoSteps) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();

  solver.Rollout();
  std::vector<double> costs;
  costs.push_back(solver.Cost());
  for (int iter = 0; iter < 2; ++iter) {
    solver.UpdateExpansions();
    solver.BackwardPass();
    solver.ForwardPass();
    costs.push_back(solver.Cost());
  }

  // Test converges on first iteration
  EXPECT_LT(costs[1] - costs[2], 1e-10);

  // Check Feedback gain at first time step
  // Compare with result from Altro.jl
  Eigen::MatrixXd K0(m_static, n_static);
  // clang-format off
  K0 << -63.9657,   0.0,    -42.7673,   0.0,    -11.5189,   0.0,
          0.0,    -63.9657,   0.0,    -42.7673,   0.0,    -11.5189;
  // clang-format on
  EXPECT_TRUE(
      solver.GetKnotPointFunction(0).GetFeedbackGain().isApprox(K0, 1e-3));

  // Check that the feedforward gains are all close to zero
  for (int k = 0; k < N; ++k) {
    EXPECT_LT(solver.GetKnotPointFunction(0).GetFeedforwardGain().norm(), 1e-8);
  }
}

TEST_F(TripleIntegratoriLQRTest, FullSolve) {
  altro::ilqr::iLQR<n_static, m_static> solver =
      MakeSolver<n_static, m_static>();

  std::shared_ptr<altro::Trajectory<n_static, m_static>> Z =
      solver.GetTrajectory();

  solver.Solve();
  EXPECT_EQ(solver.GetStatus(), altro::ilqr::SolverStatus::kSolved);
  EXPECT_EQ(solver.GetStats().iterations, 2);

  // Check Feedback gain at first time step
  // Compare with result from Altro.jl
  Eigen::MatrixXd K0(m_static, n_static);
  // clang-format off
  K0 << -63.9657,   0.0,    -42.7673,   0.0,    -11.5189,   0.0,
          0.0,    -63.9657,   0.0,    -42.7673,   0.0,    -11.5189;
  // clang-format on
  EXPECT_TRUE(
      solver.GetKnotPointFunction(0).GetFeedbackGain().isApprox(K0, 1e-3));
}