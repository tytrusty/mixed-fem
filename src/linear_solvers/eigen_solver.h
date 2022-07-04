#pragma once

#include "linear_solver.h"

namespace mfem {

  template <typename Solver, typename Scalar, int Ordering>
  class EigenSolver : public LinearSolver<Scalar, Ordering> {
  public:

    EigenSolver() : has_init_(false) {}

    void compute(const Eigen::SparseMatrix<Scalar, Ordering>& A) override {
      if (!has_init_) {
        solver_.analyzePattern(A);
        has_init_ = true;
      }
      solver_.factorize(A);
    }

    Eigen::VectorXx<Scalar> solve(const Eigen::VectorXx<Scalar>& b) override {
      assert(has_init_);
      return solver_.solve(b);
    }

  private:

    Solver solver_;
    bool has_init_;

  };


}
