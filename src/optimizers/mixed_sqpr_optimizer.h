#pragma once

#include "optimizers/mixed_sqp_optimizer.h"

#if defined(SIM_USE_CHOLMOD)
#include <Eigen/CholmodSupport>
#endif

namespace mfem {

  // Mixed FEM Sequential Quadratic Program
  class MixedSQPROptimizer : public MixedSQPOptimizer {
  public:
    
    MixedSQPROptimizer(std::shared_ptr<SimObject> object,
        std::shared_ptr<SimConfig> config) : MixedSQPOptimizer(object, config) {}

  public:

    virtual void step() override;

    // Build system left hand side
    virtual void build_lhs() override;

    // Build linear system right hand side
    virtual void build_rhs() override;

    // Simulation substep for this object
    // init_guess - whether to initialize guess with a prefactor solve
    // decrement  - newton decrement norm
    virtual void substep(bool init_guess, double& decrement) override;

    virtual bool linesearch_x(Eigen::VectorXd& x,
        const Eigen::VectorXd& dx) override;

    Eigen::VectorXd gl_;
    Eigen::SimplicialLDLT<Eigen::SparseMatrixd> solver_;

  };
}
