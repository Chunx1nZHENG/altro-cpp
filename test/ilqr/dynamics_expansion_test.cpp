#include <gtest/gtest.h>
#include <iostream>

#include "altro/ilqr/dynamics_expansion.hpp"
#include "altro/problem/discretized_model.hpp"
#include "examples/triple_integrator.hpp"

namespace altro {
namespace ilqr {

class DynamicsExpansionTest : public ::testing::Test {
 public:
  int n = 6;
  int m = 2;
  examples::TripleIntegrator model = examples::TripleIntegrator(2);

 protected:
  void SetUp() override {}
};

constexpr int STATE_DIM = 6;
constexpr int CONTROL_DIM = 2;

TEST_F(DynamicsExpansionTest, Construction) {
  DynamicsExpansion<-1, -1> expansion(n, m);
  EXPECT_EQ(expansion.StateDimension(), n);
  EXPECT_EQ(expansion.ControlDimension(), m);
  EXPECT_EQ(expansion.StateMemorySize(), Eigen::Dynamic);
  EXPECT_EQ(expansion.ControlMemorySize(), Eigen::Dynamic);
  EXPECT_TRUE(expansion.GetJacobian().isApproxToConstant(0));
  EXPECT_EQ(expansion.GetJacobian().rows(), n);
  EXPECT_EQ(expansion.GetJacobian().cols(), n + m);
}

TEST_F(DynamicsExpansionTest, ConstructionStatic) {
  DynamicsExpansion<STATE_DIM, CONTROL_DIM> expansion(n, m);
  EXPECT_EQ(expansion.StateDimension(), n);
  EXPECT_EQ(expansion.ControlDimension(), m);
  EXPECT_EQ(expansion.StateMemorySize(), n);
  EXPECT_EQ(expansion.ControlMemorySize(), m);
  EXPECT_TRUE(expansion.GetJacobian().isApproxToConstant(0));
  EXPECT_EQ(expansion.GetJacobian().rows(), n);
  EXPECT_EQ(expansion.GetJacobian().cols(), n + m);
}

TEST_F(DynamicsExpansionTest, ConstructionDeath) {
  auto bad_state = [&]() { 
    DynamicsExpansion<STATE_DIM, CONTROL_DIM> expansion(n+1, m);
  };
  EXPECT_DEATH(bad_state(), "Assert.*State sizes must be consistent");

  auto bad_control= [&]() { 
    DynamicsExpansion<-1, CONTROL_DIM> expansion(n+1, m-1);
  };
  EXPECT_DEATH(bad_control(), "Assert.*Control sizes must be consistent");
}

TEST_F(DynamicsExpansionTest, SetJac) {
  DynamicsExpansion<-1, -1> expansion(n, m);
  expansion.GetA() = MatrixXd::Constant(n, n, 2);
  auto B = expansion.GetB();
  B = MatrixXd::Constant(n, m, 5);
  B(0, 0) = 9;

  MatrixXd jac_ans(n, n + m);
  // clang-format off
	jac_ans << 2,2,2,2,2,2, 9,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5;
  // clang-format on
  EXPECT_TRUE(expansion.GetJacobian().isApprox(jac_ans));
}

TEST_F(DynamicsExpansionTest, SetJacStatic) {
  DynamicsExpansion<STATE_DIM, CONTROL_DIM> expansion(n, m);
  expansion.GetA() = MatrixXd::Constant(n, n, 2);
  auto B = expansion.GetB();
  B = MatrixXd::Constant(n, m, 5);
  B(0, 0) = 9;

  MatrixXd jac_ans(n, n + m);
  // clang-format off
	jac_ans << 2,2,2,2,2,2, 9,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5,
	           2,2,2,2,2,2, 5,5;
  // clang-format on
  EXPECT_TRUE(expansion.GetJacobian().isApprox(jac_ans));
}

TEST_F(DynamicsExpansionTest, CalcJacobian) {
  DynamicsExpansion<-1, -1> expansion(n, m);
  KnotPoint<-1, -1> z = KnotPoint<-1, -1>::Random(n, m);
  problem::DiscretizedModel<examples::TripleIntegrator> model_d(model);

  EXPECT_THROW(expansion.CalcExpansion(model, z), std::runtime_error);
  expansion.CalcExpansion(model_d, z);

  MatrixXd jac = MatrixXd::Zero(STATE_DIM, STATE_DIM + CONTROL_DIM);
  model_d.Jacobian(z.State(), z.Control(), z.GetTime(), z.GetStep(), jac);
  EXPECT_TRUE(expansion.GetJacobian().isApprox(jac));
}

TEST_F(DynamicsExpansionTest, CalcJacobianStatic) {
  DynamicsExpansion<STATE_DIM, CONTROL_DIM> expansion(n, m);
  KnotPoint<STATE_DIM, CONTROL_DIM> z = KnotPoint<STATE_DIM, CONTROL_DIM>::Random(n, m);
  problem::DiscretizedModel<examples::TripleIntegrator> model_d(model);

  EXPECT_THROW(expansion.CalcExpansion(model, z), std::runtime_error);
  expansion.CalcExpansion(model_d, z);

  MatrixXd jac = MatrixXd::Zero(STATE_DIM, STATE_DIM + CONTROL_DIM);
  model_d.Jacobian(z.State(), z.Control(), z.GetTime(), z.GetStep(), jac);
  EXPECT_TRUE(expansion.GetJacobian().isApprox(jac));
}

}  // namespace ilqr
}  // namespace altro