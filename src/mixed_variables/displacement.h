#pragma once

#include "mixed_variable.h"
#include "optimizers/optimizer_data.h"
#include "sparse_utils.h"

namespace mfem {

  // Variable for DIMxDIM symmetric deformation from polar decomposition of
  // deformation gradient (F = RS) 
  template<int DIM>
  class Displacement : public MixedVariable<DIM> {

    typedef MixedVariable<DIM> Base;

  public:

    Displacement(std::shared_ptr<Mesh> mesh) : MixedVariable<DIM>(mesh)
    {}

    double energy(const Eigen::VectorXd& s) override;
    double constraint_value(const Eigen::VectorXd& x,
        const Eigen::VectorXd& s) override;
    void update(const Eigen::VectorXd& x, double dt) override;
    void reset() override;

    Eigen::VectorXd rhs() override;
    Eigen::VectorXd gradient() override;

    const Eigen::SparseMatrix<double, Eigen::RowMajor>& lhs() override {
      return A_;
    }

    void solve(const Eigen::VectorXd& dx) override;

    Eigen::VectorXd& delta() override {
      return ds_;
    }

    Eigen::VectorXd& value() override {
      return s_;
    }

  protected:

    void update_rotations(const Eigen::VectorXd& x);
    void update_derivatives(double dt);


  private:

    // Number of degrees of freedom per element
    // For DIM == 3 we have 6 DOFs per element, and
    // 3 DOFs for DIM == 2;
    static constexpr int N() {
      return DIM == 3 ? 6 : 3;
    }

    static constexpr int M() {
      return DIM * DIM;
    }

    // Matrix and vector data types
    using MatD  = Eigen::Matrix<double, DIM, DIM>; // 3x3 or 2x2
    using VecN  = Eigen::Vector<double, N()>;      // 6x1 or 3x1
    using VecM  = Eigen::Vector<double, M()>;      // 9x1
    using MatN  = Eigen::Matrix<double, N(), N()>; // 6x6 or 3x3
    using MatMN = Eigen::Matrix<double, M(), N()>; // 9x6 or 4x3

    // Nx1 vector reprenting identity matrix
    static constexpr VecN Ivec() {
      VecN I; 
      if constexpr (DIM == 3) {
        I << 1,1,1,0,0,0;
      } else {
        I << 1,1,0;
      }
      return I;
    }

    static constexpr MatN Sym() {
      MatN m; 
      if constexpr (DIM == 3) {
        m = (VecN() << 1,1,1,2,2,2).finished().asDiagonal();
      } else {
        m = (VecN() << 1,1,2).finished().asDiagonal();
      }
      return m;
    }

    static constexpr MatN Syminv() {
      MatN m; 
      if constexpr (DIM == 3) {
        m = (VecN() << 1,1,1,0.5,0.5,0.5).finished().asDiagonal();
      } else {
        m = (VecN() << 1,1,0.5).finished().asDiagonal();
      }
      return m;
    }

    using Base::mesh_;

    OptimizerData data_;      // Stores timing results
    int nelem_;               // number of elements
    Eigen::VectorXd s_;       // deformation variables
    Eigen::VectorXd ds_;      // deformation variables deltas
    Eigen::VectorXd la_;      // lagrange multipliers
    Eigen::VectorXd rhs_;     // RHS for schur complement system
    Eigen::VectorXd grad_;    // Gradient with respect to 's' variables
    Eigen::VectorXd gl_;      // tmp var: g_\Lambda in the notes
    Eigen::VectorXd Jdx_;     // tmp var: Jacobian multiplied by dx
    std::vector<MatD> R_;     // per-element rotations
    std::vector<VecN> S_;     // per-element deformation
    std::vector<VecN> g_;     // per-element gradients
    std::vector<MatN> H_;     // per-element hessians
    std::vector<MatN> Hinv_;  // per-element hessian inverse
    std::vector<MatMN> dSdF_; 
    std::vector<Eigen::MatrixXd> Aloc_;
    Eigen::SparseMatrix<double, Eigen::RowMajor> A_;
    std::shared_ptr<Assembler<double,DIM>> assembler_;
  };
}
