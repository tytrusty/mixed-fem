#include "mixed_sqpr_optimizer.h"

#include <chrono>
#include "sparse_utils.h"
#include "svd/dsvd.h"
#include "svd/svd3x3_sse.h"
#include "linesearch.h"
#include "pinning_matrix.h"
#include "pcg.h"
#include "svd/newton_procrustes.h"

#include <fstream>
#include "unsupported/Eigen/src/SparseExtra/MarketIO.h"

using namespace mfem;
using namespace Eigen;
using namespace std::chrono;

void MixedSQPROptimizer::step() {
  data_.clear();

  E_prev_ = 0;
  // setup_preconditioner();

  int i = 0;
  double grad_norm;
  double res;
  // q_.setZero();
  // q_.segment(0, x_.size()) = x_ - x0_;

  do {
    data_.timer.start("step");
    update_system();
    substep(i, grad_norm);

    linesearch_x(x_, dx_);
    // linesearch_s(s_, ds_);
    linesearch_s_local(s_,ds_);
    // linesearch(x_, dx_, s_, ds_);

    // x_ += dx_;
    // s_ += ds_;

    double E = energy(x_, s_, la_);
    res = std::abs((E - E_prev_) / E);
    data_.egrad_.push_back(grad_norm);
    data_.energies_.push_back(E);
    data_.energy_residuals_.push_back(res);
    E_prev_ = E;
    data_.timer.stop("step");

    ++i;
  } while (i < config_->outer_steps && grad_norm > config_->newton_tol
    && (res > 1e-12));

  data_.print_data(config_->show_timing);

  update_configuration();
}

void MixedSQPROptimizer::build_lhs() {
  data_.timer.start("LHS");

  double ih2 = 1. / (config_->h * config_->h);
  double h2 = (config_->h * config_->h);

  static Eigen::Matrix6d Syminv = (Eigen::Vector6d() <<
    1, 1, 1, .5, .5, .5).finished().asDiagonal();


  data_.timer.start("Hinv");
  #pragma omp parallel for
  for (int i = 0; i < nelem_; ++i) {
    const Vector6d& si = s_.segment(6*i,6);
    Matrix6d H = h2 * object_->material_->hessian(si);
    Hinv_[i] = H.inverse();
    g_[i] = h2 * object_->material_->gradient(si);
    H_[i] = (1.0 / vols_[i]) * (Syminv * H * Syminv);
  }
  data_.timer.stop("Hinv");

  // std::cout << "HX: " << std::endl;
  //SparseMatrixd Hs;
  //SparseMatrix<double, RowMajor> Gx = Gx_;
  //init_block_diagonal<6,6>(Hs, nelem_);
  //update_block_diagonal<6,6>(H_, Hs);

  //lhs_ += Gx * Hs * Gx.transpose();
  // SparseMatrixd G = Gx * Hs * Gx.transpose();
  // saveMarket(M_, "M_.mkt");
  // saveMarket(Hs, "Hs.mkt");
  
  data_.timer.start("Local H");
  std::vector<Matrix12d> Hloc(nelem_); 
  #pragma omp parallel for
  for (int i = 0; i < nelem_; ++i) {
    Hloc[i] = (Jloc_[i].transpose() * (dS_[i] * H_[i]
        * dS_[i].transpose()) * Jloc_[i]) * (vols_[i] * vols_[i]);
  }
  data_.timer.stop("Local H");
  //saveMarket(G, "GHG.mkt");
  //saveMarket(assembler_->A, "lhs2.mkt");
  //saveMarket(M_, "M_.mkt");
  data_.timer.start("Update LHS");
  assembler_->update_matrix(Hloc);
  data_.timer.stop("Update LHS");

  lhs_ = M_ + assembler_->A;

  //saveMarket(assembler_->A, "GHG2.mkt");
  //   MatrixXd lhs(lhs_);
  //   EigenSolver<MatrixXd> es(lhs);
  //   std::cout << "LHS EVALS: \n" << es.eigenvalues().real() << std::endl;
  data_.timer.stop("LHS");

}

void MixedSQPROptimizer::build_rhs() {
  data_.timer.start("RHS");

  rhs_.resize(x_.size());
  rhs_.setZero();
  gl_.resize(6*nelem_);
  double h = config_->h;
  double h2 = h*h;

  VectorXd tmp(9*nelem_);
  #pragma omp parallel for
  for (int i = 0; i < nelem_; ++i) {
    const Vector6d& si = s_.segment(6*i,6);

    // W * c(x^k, s^k) + H^-1 * g_s
    gl_.segment<6>(6*i) = vols_[i] * H_[i] * Sym * (S_[i] - si + Hinv_[i] * g_[i]);
    tmp.segment<9>(9*i) = dS_[i]*gl_.segment<6>(6*i);

  }
  rhs_ = -PJ_ * tmp - M_*(x_ - x0_ - h*vt_ - h2*f_ext_);
  data_.timer.stop("RHS");


  grad_.resize(x_.size() + 6*nelem_);
  #pragma omp parallel for
  for (int i = 0; i < nelem_; ++i) {
    tmp.segment<9>(9*i) = dS_[i]*la_.segment<6>(6*i);
    grad_.segment<6>(x_.size() + 6*i) = vols_[i] * (g_[i] + Sym*la_.segment<6>(6*i));
  }
  grad_.segment(0,x_.size()) = M_*(x_ - x0_ - h*vt_ - h2*f_ext_) - PJ_ * tmp;
}

void MixedSQPROptimizer::update_system() {

  // Compute rotations and rotation derivatives
  update_rotations();

  // Assemble blocks for left and right hand side
  build_lhs();
  build_rhs();
}

void MixedSQPROptimizer::substep(int step, double& decrement) {
  int niter = 0;

  if (step == 0 || true) {
    data_.timer.start("prefactor");

    solver_.compute(lhs_);
    if(solver_.info()!=Success) {
    std::cerr << "prefactor failed! " << std::endl;
    exit(1);
    }
    dx_ = solver_.solve(rhs_);
    data_.timer.stop("prefactor");
  // saveMarket(lhs_, "lhs0.mkt");

  } else {
    data_.timer.start("global");
// // sanity check preconditioner on cg, make sure cond is good
    //BiCGSTAB<SparseMatrix<double>,  Eigen::IncompleteLUT<double>> cg;
    // LeastSquaresConjugateGradient<SparseMatrix<double>,  Eigen::IncompleteLUT<double>> cg;
    // Eigen::IncompleteLUT<double>& precon = cg.preconditioner();
    // precon.setFillfactor(10);
    // precon.setDroptol(1e-8);
    // cg.compute(lhs_);
    // cg.setTolerance(1e-4);
    // cg.setMaxIterations(100);
//     // // dx_ = solver_.solve(rhs_);
    // dx_ = cg.solve(rhs_);
    // dx_ = cg.solveWithGuess(rhs_, dx_);
    // niter = cg.iterations();


    // dx_ = solver_.solve(rhs_);
    // niter = pcr(dx_, lhs_ , rhs_, tmp_r_, tmp_z_, tmp_p_, tmp_Ap_, solver_arap_, 1e-4);
    // std::cout << "  - CG iters: " << niter;
    // double relative_error = (lhs_*dx_ - rhs_).norm() / rhs_.norm(); 
    // std::cout << " rel error: " << relative_error << " abs error: " << (lhs_*dx_-rhs_).norm() << std::endl;

    // typedef amgcl::backend::eigen<double> Backend;

    // // typedef amgcl::make_solver<
    // //     amgcl::relaxation::as_preconditioner<Backend, amgcl::relaxation::ilu0>,
    // //     amgcl::solver::bicgstab<Backend>
    // // > Solver;
    // typedef amgcl::make_solver<
    //     amgcl::amg<
    //         Backend,
    //         amgcl::coarsening::smoothed_aggregation,
    //         amgcl::relaxation::spai0
    //         >,
    //     amgcl::solver::bicgstab<Backend>
    //     > Solver;
  
    // Solver::params prm2;
    // // prm2.solver.verbose = 1;
    // prm2.solver.tol = 1e-3;
    // //prm2.precond.coarse_enough = 100;
    // prm2.precond.npre = 1;
    // prm2.precond.npost = 1;
    // prm2.precond.ncycle = 5;
    // //prm2.solver.pside = amgcl::preconditioner::side::left;
    // prm2.solver.maxiter = 1000;
    // // prm2.precond.tau = 1e-6;
    // // prm2.precond.p = 10;
    // data_.timer.start("setup");
    // Solver solve(lhs_, prm2);
    // std::cout << "SOLVE:\n" << solve << std::endl;
    // data_.timer.stop("setup");

    amg_solver_->precond().rebuild(lhs_);

    int    iters;
    double error;
    dx_ = solver_arap_.solve(rhs_);
    std::tie(iters, error) = amg_solver_->operator()(rhs_, dx_);
    std::cout << iters << " " << error << std::endl;


    data_.timer.stop("global");

  }

  data_.timer.start("local");
  VectorXd Jdx = - PJ_.transpose() * dx_;
  la_ = -gl_;

  // Update per-element R & S matrices
  ds_.resize(6*nelem_);

  #pragma omp parallel for 
  for (int i = 0; i < nelem_; ++i) {
    la_.segment<6>(6*i) += H_[i] * (dS_[i].transpose() * Jdx.segment<9>(9*i));
    // ds_.segment<6>(6*i) = -Hinv_[i] * (Sym * la_.segment<6>(6*i)+ g_[i]);
    Vector6d si = s_.segment<6>(6*i);
    Vector6d gs = vols_[i]*(Sym * la_.segment<6>(6*i)+ g_[i]);
    ds_.segment<6>(6*i) = (vols_[i]*config_->h*config_->h*object_->material_->hessian(si,false)).completeOrthogonalDecomposition().solve(-gs);
  }
  data_.timer.stop("local");

  // #pragma omp parallel for 
  // for (int i = 0; i < nelem_; ++i) {
  //   // ds_.segment<6>(6*i) = -Hinv_[i] * (Sym * la_.segment<6>(6*i)+ g_[i]);
  //   if (ds_.segment<6>(6*i).norm() > 100) {
  //     Vector6d si = s_.segment<6>(6*i);
  //     Matrix6d H = object_->material_->hessian(si);
  //     std::cout << "si: \n " << si << std::endl;
  //     std::cout << "H[s]: " << H << std::endl;
  //     std::cout << "H[s]2: " << object_->material_->hessian(si,false) << std::endl;
  //     std::cout << "evals: " << H.eigenvalues().real() << std::endl;
  //     std::cout << "evals2: " << object_->material_->hessian(si,false).eigenvalues().real() << std::endl;

  //     std::cout << "i: " << i << std::endl;
  //     Vector6d gs = (Sym * la_.segment<6>(6*i)+ g_[i]);
  //     std::cout << "DS tmp: \n" << (config_->h*config_->h*object_->material_->hessian(si,false)).completeOrthogonalDecomposition().solve(-gs);
  //     std::cout << "ds i: \n" << ds_.segment<6>(6*i) << std::endl;
  //     std::cout << "l i: \n" << la_.segment<6>(6*i) << std::endl;
  //     //std::cout << "Hinv_ i: \n" << Hinv_[i] << std::endl;
  //     std::cout << "g_ i: \n" << g_[i] << std::endl;
  //     std::cout << "gs \n" << gs << std::endl;

  //     SparseMatrixd Hs;
  //     SparseMatrix<double, RowMajor> Gx = Gx_;
  //     init_block_diagonal<6,6>(Hs, nelem_);
  //     update_block_diagonal<6,6>(H_, Hs);
  // SparseMatrixd G = Gx * Hs * Gx.transpose();
  // saveMarket(lhs_, "lhs.mkt");
  // // saveMarket(M_, "M_.mkt");
  // saveMarket(Hs, "Hs.mkt");
  // saveMarket(G, "GHG.mkt");

  //     exit(1);
    // }
  // }
  // std::cout << "dsinf: " << ds_.lpNorm<Infinity>() << " la inf: " << la_.lpNorm<Infinity>() << std::endl;
// exit(1);
  decrement = std::sqrt(dx_.squaredNorm() + ds_.squaredNorm());
  data_.egrad_x_.push_back(dx_.norm());
  data_.egrad_s_.push_back(ds_.norm());
  data_.egrad_la_.push_back(la_.norm());

}

bool MixedSQPROptimizer::linesearch_x(VectorXd& x, const VectorXd& dx) {
  return MixedOptimizer::linesearch_x(x,dx);
  data_.timer.start("LS_x");

  auto value = [&](const VectorXd& x)->double {
    //double h = config_->h;
    //VectorXd xdiff = x - x0_ - h*vt_ - h*h*f_ext_;
    //return 0.5*xdiff.transpose()*M_*xdiff;
    return energy(x,s_,la_);
  };

  VectorXd xt = x;
  VectorXd tmp;
  double alpha = 1.0;
  SolverExitStatus status = linesearch_backtracking_bisection(xt, dx, value,
      tmp, alpha, config_->ls_iters, 0.1, 0.66, E_prev_);
  bool done = status == MAX_ITERATIONS_REACHED;
  if (done)
    std::cout << "linesearch_x max iters" << std::endl;
  x = xt;
  data_.timer.stop("LS_x");
  // std::cout << "x alpha: " << alpha << std::endl;
  return done;
}

void MixedSQPROptimizer::reset() {
  MixedSQPOptimizer::reset();

  SparseMatrixdRowMajor A;
  A.resize(nelem_*9, nelem_*9);
  std::vector<Triplet<double>> trips;
  for (int i = 0; i < nelem_; ++i) {
    for (int j = 0; j < 9; ++j) {
      trips.push_back(Triplet<double>(9*i+j, 9*i+j, object_->config_->mu / vols_[i]));
    }
  }
  A.setFromTriplets(trips.begin(),trips.end());

  double h2 = config_->h * config_->h;
  SparseMatrixdRowMajor L = PJ_ * A * PJ_.transpose();
  SparseMatrixdRowMajor lhs = M_ + h2*L;
  solver_arap_.compute(lhs);

    Solver::params prm2;
  // prm2.solver.verbose = 1;
  prm2.solver.tol = 1e-3;
  prm2.precond.coarse_enough = 100;
  prm2.precond.npre = 1;
  prm2.precond.npost = 1;
  prm2.precond.ncycle = 2;
  //prm2.solver.pside = amgcl::preconditioner::side::left;
  prm2.solver.maxiter = 1000;
  prm2.precond.allow_rebuild = true;
  // prm2.precond.tau = 1e-6;
  // prm2.precond.p = 10;
  data_.timer.start("setup");
  amg_solver_ = std::make_shared<Solver>(lhs_,prm2);
  std::cout << "SOLVE:\n" << amg_solver_ << std::endl;
}

