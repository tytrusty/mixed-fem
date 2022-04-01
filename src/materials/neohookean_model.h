#pragma once

#include "materials/material_model.h"
#include "config.h"
namespace mfem {

  // Stable neohookean material model
  class StableNeohookean : public MaterialModel {
  public:
    
    StableNeohookean(const std::shared_ptr<MaterialConfig>& config)
        : MaterialModel("Stable Neohookean"), config_(config) {}

    void update_compliance(int n, int m, 
        const std::vector<Eigen::Matrix3d>& R,
        const std::vector<Eigen::Matrix6d>& Hinv,
        const Eigen::VectorXd& vols, Eigen::SparseMatrixd& mat) override;

    double energy(const Eigen::Vector6d& S) override; 

    Eigen::Vector6d gradient(const Eigen::Matrix3d& R,
        const Eigen::Vector6d& S) override; 

    Eigen::Matrix6d hessian_inv(const Eigen::Matrix3d& R,
        const Eigen::Vector6d& S) override;

    Eigen::Matrix9d WHinvW(const Eigen::Matrix3d& R,
        const Eigen::Matrix6d& Hinv) override;

    Eigen::Vector9d rhs(const Eigen::Matrix3d& R,
        const Eigen::Vector6d& S, const Eigen::Matrix6d& Hinv,
        const Eigen::Vector6d& g) override;

    Eigen::Vector6d dS(const Eigen::Matrix3d& R, 
        const Eigen::Vector6d& S, const Eigen::Vector9d& L,
        const Eigen::Matrix6d& Hinv) override;

    
  private:
    std::shared_ptr<MaterialConfig> config_;     
  };


}