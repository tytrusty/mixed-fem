#pragma once

#include "linear_solver.h"
#include "pcg.h"
#include "igl/boundary_facets.h"
#include "rigid_inertia_com.h"

namespace mfem {

  struct AffinePCGInput {

  };

  template <typename Scalar, int Ordering>
  class AffinePCG : public LinearSolver<Scalar, Ordering> {
  public:

    AffinePCG(std::shared_ptr<Mesh> mesh,
        std::shared_ptr<SimConfig> config) : config_(config) {

      double k = config->h * config->h;
      k *= mesh->config_->mu;
      Eigen::SparseMatrixdRowMajor lhs = mesh->mass_matrix()
          + k * mesh->laplacian();
      solver_.compute(lhs);

      //TODO only works in 3D and on tetmeshes
      //build up reduced space
      T0_.resize(3*mesh->V0_.rows(), 12);

      //compute center of mass
      Eigen::Matrix3d I;
      Eigen::Vector3d c;
      double mass = 0;

      //std::cout<<"HERE 1 \n";
      // TODO wrong? should be F_ not T_ for tetrahedra
      Eigen::MatrixXi F;
      igl::boundary_facets(mesh->T_, F);
      sim::rigid_inertia_com(I, c, mass, mesh->V0_, F, 1.0);

      for(unsigned int ii=0; ii<mesh->V0_.rows(); ii++ ) {
        //std::cout<<"HERE 2 "<<ii<<"\n";
        T0_.block<3,3>(3*ii, 0) = Eigen::Matrix3d::Identity()*(mesh->V0_(ii,0) - c(0));
        T0_.block<3,3>(3*ii, 3) = Eigen::Matrix3d::Identity()*(mesh->V0_(ii,1) - c(1));
        T0_.block<3,3>(3*ii, 6) = Eigen::Matrix3d::Identity()*(mesh->V0_(ii,2) - c(2));
        T0_.block<3,3>(3*ii, 9) = Eigen::Matrix3d::Identity();
      }

      T0_ = mesh->P()*T0_;
    }

    void compute(const Eigen::SparseMatrix<Scalar, Ordering>& A) override {
      lhs_ = A;
    }

    Eigen::VectorXx<Scalar> solve(const Eigen::VectorXx<Scalar>& b) override {

      Eigen::Matrix<double, 12, 1> x_affine;  
      x_affine = (T0_.transpose()*lhs_*T0_).lu().solve(T0_.transpose()*b);
      x_ = T0_*x_affine;
      int niter = pcg(x_, lhs_ , b, tmp_r_, tmp_z_, tmp_zm1_, tmp_p_, tmp_Ap_,
          solver_, config_->itr_tol, config_->max_iterative_solver_iters);
      std::cout << "  - CG iters: " << niter;
      double relative_error = (lhs_*x_ - b).norm() / b.norm(); 
      std::cout << " rel error: " << relative_error << " abs error: " << (lhs_*x_-b).norm() << std::endl;
      return x_;
    }

  private:
    std::shared_ptr<SimConfig> config_;
    Eigen::SparseMatrix<Scalar, Ordering> lhs_;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<Scalar, Ordering>> solver_;
    Eigen::MatrixXd T0_;
    Eigen::VectorXd x_;

    // CG temp variables
    Eigen::VectorXd tmp_r_;
    Eigen::VectorXd tmp_z_;
    Eigen::VectorXd tmp_zm1_;
    Eigen::VectorXd tmp_p_;
    Eigen::VectorXd tmp_Ap_;

  };


}
