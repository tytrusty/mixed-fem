#pragma once

#include "optimizers/mixed_sqp_optimizer.h"

#if defined(SIM_USE_CHOLMOD)
#include <Eigen/CholmodSupport>
#endif

namespace mfem {

  // Mixed FEM Sequential Quadratic Program
  class MixedSQPOptimizer2 : public MixedSQPOptimizer {
  public:
    
    MixedSQPOptimizer2(std::shared_ptr<SimObject> object,
        std::shared_ptr<SimConfig> config) : MixedSQPOptimizer(object, config) {}

    void reset() override;
  
  public:


    // Build system left hand side
    virtual void build_lhs() override;

    // Build linear system right hand side
    virtual void build_rhs();

    // Simulation substep for this object
    // init_guess - whether to initialize guess with a prefactor solve
    // decrement  - newton decrement norm
    virtual void substep(bool init_guess, double& decrement);

    Eigen::SparseMatrixd Minv_;
  };
}