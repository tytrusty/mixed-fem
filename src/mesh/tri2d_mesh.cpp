#include "tri2d_mesh.h"
#include <igl/doublearea.h>
#include "linear_tri2dmesh_dphi_dX.h"
#include "linear_tri2dmesh_mass_matrix.h"
#include "config.h"

using namespace Eigen;
using namespace mfem;

namespace {

  // From dphi/dX, form jacobian dphi/dq where
  // q is an elements local set of vertices
  template <typename Scalar>
  void local_jacobian(Matrix<Scalar,4,6>& B, const Matrix<Scalar,3,2>& dX) {
    B  << dX(0,0), 0, dX(1,0), 0, dX(2,0), 0,
          0, dX(0,0), 0, dX(1,0), 0, dX(2,0),
          dX(0,1), 0, dX(1,1), 0, dX(2,1), 0,
          0, dX(0,1), 0, dX(1,1), 0, dX(2,1);
  }

}

Tri2DMesh::Tri2DMesh(const Eigen::MatrixXd& V, const Eigen::MatrixXi& T,
    std::shared_ptr<MaterialModel> material,
    std::shared_ptr<MaterialConfig> material_config)
    : Mesh(V,T,material,material_config) {
  assert(V.cols() == 2);
  sim::linear_tri2dmesh_dphi_dX(dphidX_, V0_, T_);
}

void Tri2DMesh::volumes(Eigen::VectorXd& vol) {
  igl::doublearea(V0_, T_, vol);
  vol.array() *= (config_->thickness/2);
}

void Tri2DMesh::mass_matrix(Eigen::SparseMatrixdRowMajor& M,
    const VectorXd& vols) {

  VectorXd densities = VectorXd::Constant(T_.rows(), config_->density);
  sim::linear_tri2dmesh_mass_matrix(M, V0_, T_, densities, vols);
}

void Tri2DMesh::init_jacobian() {
  Jloc_.resize(T_.rows());

  std::vector<Triplet<double>> trips;
  for (int i = 0; i < T_.rows(); ++i) { 

    // Local block
    Matrix<double,4,6> B;
    Matrix32d dX = sim::unflatten<3,2>(dphidX_.row(i));
    local_jacobian(B, dX);

    Jloc_[i] = B;

    for (int j = 0; j < 4; ++j) {

      // k-th vertex of the triangle
      for (int k = 0; k < 3; ++k) {
        int vid = T_(i,k); // vertex index

        // x,y,z index for the k-th vertex
        for (int l = 0; l < 2; ++l) {
          double val = B(j,2*k+l);
          trips.push_back(Triplet<double>(4*i+j, 2*vid+l, val));
        }
      }
    }
  }
  J_.resize(4*T_.rows(), V_.size());
  J_.setFromTriplets(trips.begin(),trips.end());  
}


void Tri2DMesh::jacobian(SparseMatrixdRowMajor& J, const VectorXd& vols,
      bool weighted) {
  std::cerr << "Tri2DMesh::jacobian(J, vols, weighted) unimplemented" << std::endl;
}

void Tri2DMesh::jacobian(std::vector<MatrixXd>& J) {
  std::cerr << "Tri2DMesh::jacobian(J) unimplemented" << std::endl;
}

void Tri2DMesh::deformation_gradient(const VectorXd& x, VectorXd& F) {
  assert(x.size() == J_.cols());
  F = J_ * x;
}

// MatrixXd Tri2DMesh::vertices() {
//   MatrixXd tmp;
//   tmp.resize(V_.rows(), 3);
//   tmp.col(0) = V_.col(0);
//   tmp.col(1) = V_.col(1);
//   tmp.col(2).setZero();
//   return tmp;
// }
