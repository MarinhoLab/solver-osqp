/**
Based on the M3::qpOASES_Solver wrapper in solver-qpoases, adapted to use the OSQP solver.
*/
#include <OSQP_solver.h>

#include <stdexcept>
#include <string>

namespace M3
{

// https://stackoverflow.com/questions/53408962/try-to-understand-compiler-error-message-default-member-initializer-required-be
OSQP_Solver::Configuration::Configuration() = default;

// https://stackoverflow.com/questions/53408962/try-to-understand-compiler-error-message-default-member-initializer-required-be
OSQP_Solver::Info::Info() = default;

OSQP_Solver::OSQP_Solver(const Configuration& configuration):
    osqp_solve_first_time_(true),
    osqp_solver_(nullptr),
    configuration_(configuration),
    problem_size_(0),
    constraint_size_(0)
{

}

OSQP_Solver::~OSQP_Solver()
{
    _cleanup();
}

void OSQP_Solver::_cleanup()
{
    if(osqp_solver_ != nullptr)
    {
        osqp_cleanup(osqp_solver_);
        osqp_solver_ = nullptr;
    }
}

std::vector<double> OSQP_Solver::_vectorxd_to_std_vector_double(const VectorXd& vectorxd)
{
    std::vector<double> vec(vectorxd.data(), vectorxd.data() + vectorxd.rows() * vectorxd.cols());
    return vec;
}

VectorXd OSQP_Solver::_std_vector_double_to_vectorxd(std::vector<double> std_vector_double) const
{
    double* ptr = &std_vector_double[0];
    Eigen::Map<Eigen::VectorXd> vec(ptr,std_vector_double.size());
    return vec;
}

OSQP_Solver::CSCMatrixData OSQP_Solver::_dense_to_csc_upper_triangular(const MatrixXd& M)
{
    if(M.rows()!=M.cols())
        throw std::runtime_error("OSQP_Solver::_dense_to_csc_upper_triangular(): M must be square. M.rows()="+std::to_string(M.rows())+" but M.cols()="+std::to_string(M.cols())+".");

    const OSQPInt n = static_cast<OSQPInt>(M.rows());

    CSCMatrixData csc;
    csc.rows = n;
    csc.cols = n;
    csc.p.resize(n + 1);

    OSQPInt nnz = 0;
    for(OSQPInt col = 0; col < n; ++col)
    {
        csc.p[col] = nnz;
        for(OSQPInt row = 0; row <= col; ++row)
        {
            csc.x.push_back(M(row, col));
            csc.i.push_back(row);
            ++nnz;
        }
    }
    csc.p[n] = nnz;

    return csc;
}

OSQP_Solver::CSCMatrixData OSQP_Solver::_dense_to_csc(const MatrixXd& M)
{
    const OSQPInt rows = static_cast<OSQPInt>(M.rows());
    const OSQPInt cols = static_cast<OSQPInt>(M.cols());

    CSCMatrixData csc;
    csc.rows = rows;
    csc.cols = cols;
    csc.p.resize(cols + 1);
    csc.x.resize(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols));
    csc.i.resize(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols));

    OSQPInt nnz = 0;
    for(OSQPInt col = 0; col < cols; ++col)
    {
        csc.p[col] = nnz;
        for(OSQPInt row = 0; row < rows; ++row)
        {
            csc.x[nnz] = M(row, col);
            csc.i[nnz] = row;
            ++nnz;
        }
    }
    csc.p[cols] = nnz;

    return csc;
}

void evaluate_osqp_exitflag(OSQPInt exitflag, const std::string& context)
{
    if(exitflag != 0)
    {
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): "+context+" failed. OSQP returned error code "+std::to_string(exitflag)+": "+std::string(osqp_error_message(exitflag)));
    }
}

VectorXd OSQP_Solver::solve_quadratic_program(const MatrixXd& H, const VectorXd& f, const MatrixXd& A, const VectorXd& b, const MatrixXd& Aeq, const VectorXd& beq, const VectorXd& x0)
{
    const OSQPInt PROBLEM_SIZE = static_cast<OSQPInt>(H.rows());
    const OSQPInt INEQUALITY_CONSTRAINT_SIZE = static_cast<OSQPInt>(b.size());
    const OSQPInt EQUALITY_CONSTRAINT_SIZE = static_cast<OSQPInt>(beq.size());
    const OSQPInt TOTAL_CONSTRAINT_SIZE = INEQUALITY_CONSTRAINT_SIZE + EQUALITY_CONSTRAINT_SIZE;

    ///Check sizes
    //Objective function
    if(H.rows()!=H.cols())
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): H must be symmetric. H.rows()="+std::to_string(H.rows())+" but H.cols()="+std::to_string(H.cols())+".");
    if(f.size()!=H.rows())
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): f must be compatible with H. H.rows()=H.cols()="+std::to_string(H.rows())+" but f.size()="+std::to_string(f.size())+".");

    //Optional warm-start (e.g. a known feasible solution) for the primal variable x.
    if(x0.size()!=0 && x0.size()!=PROBLEM_SIZE)
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): x0 must be compatible with H. H.rows()=H.cols()="+std::to_string(H.rows())+" but x0.size()="+std::to_string(x0.size())+".");

    //Inequality constraints
    if(b.size()!=A.rows())
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): size of b="+std::to_string(b.size())+" should be compatible with rows of A="+std::to_string(A.rows())+".");
    if(INEQUALITY_CONSTRAINT_SIZE!=0 && A.cols()!=PROBLEM_SIZE)
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): A.cols()="+std::to_string(A.cols())+" should be compatible with H.rows()="+std::to_string(PROBLEM_SIZE)+".");

    //Equality constraints
    if(beq.size()!=Aeq.rows())
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): size of beq="+std::to_string(beq.size())+" should be compatible with rows of Aeq="+std::to_string(Aeq.rows())+".");
    if(EQUALITY_CONSTRAINT_SIZE!=0 && Aeq.cols()!=PROBLEM_SIZE)
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): Aeq.cols()="+std::to_string(Aeq.cols())+" should be compatible with H.rows()="+std::to_string(PROBLEM_SIZE)+".");

    //Stack the inequality and equality constraints into OSQP's single l <= Ax <= u form.
    //Equality rows get l==u==beq. Inequality rows get l=-infinity, u=b.
    MatrixXd A_extended(TOTAL_CONSTRAINT_SIZE, PROBLEM_SIZE);
    VectorXd l_extended(TOTAL_CONSTRAINT_SIZE);
    VectorXd u_extended(TOTAL_CONSTRAINT_SIZE);

    if(INEQUALITY_CONSTRAINT_SIZE > 0)
    {
        A_extended.topRows(INEQUALITY_CONSTRAINT_SIZE) = A;
        l_extended.head(INEQUALITY_CONSTRAINT_SIZE) = VectorXd::Constant(INEQUALITY_CONSTRAINT_SIZE, -OSQP_INFTY);
        u_extended.head(INEQUALITY_CONSTRAINT_SIZE) = b;
    }
    if(EQUALITY_CONSTRAINT_SIZE > 0)
    {
        A_extended.bottomRows(EQUALITY_CONSTRAINT_SIZE) = Aeq;
        l_extended.tail(EQUALITY_CONSTRAINT_SIZE) = beq;
        u_extended.tail(EQUALITY_CONSTRAINT_SIZE) = beq;
    }

    //Convert the dense Eigen data into the CSC format required by OSQP. Every entry (including
    //zeros) is stored explicitly so that the sparsity pattern is stable across calls, which is
    //what allows osqp_update_data_mat() to be used below when hotstarting.
    CSCMatrixData P_csc = _dense_to_csc_upper_triangular(H);
    CSCMatrixData A_csc = _dense_to_csc(A_extended);
    auto q_vec = _vectorxd_to_std_vector_double(f);
    auto l_vec = _vectorxd_to_std_vector_double(l_extended);
    auto u_vec = _vectorxd_to_std_vector_double(u_extended);

    const bool problem_shape_changed = (PROBLEM_SIZE != problem_size_) || (TOTAL_CONSTRAINT_SIZE != constraint_size_);

    if(osqp_solve_first_time_ || problem_shape_changed || !configuration_.use_hotstart)
    {
        //(Re)create the solver from scratch. This is required the first time, whenever the
        //problem dimensions change, or whenever hotstarting is disabled in the configuration.
        _cleanup();

        OSQPSettings* settings = OSQPSettings_new();
        if(settings == nullptr)
            throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): unable to allocate OSQPSettings.");

        settings->max_iter = configuration_.maximum_iterations;
        settings->eps_abs = configuration_.eps_absolute;
        settings->eps_rel = configuration_.eps_relative;
        settings->eps_prim_inf = configuration_.eps_primal_infeasibility;
        settings->eps_dual_inf = configuration_.eps_dual_infeasibility;
        settings->verbose = configuration_.verbose;
        settings->polishing = configuration_.polishing;
        settings->warm_starting = configuration_.warm_starting;

        OSQPCscMatrix* P = OSQPCscMatrix_new(PROBLEM_SIZE, PROBLEM_SIZE, static_cast<OSQPInt>(P_csc.x.size()), P_csc.x.data(), P_csc.i.data(), P_csc.p.data());
        OSQPCscMatrix* A_mat = OSQPCscMatrix_new(TOTAL_CONSTRAINT_SIZE, PROBLEM_SIZE, static_cast<OSQPInt>(A_csc.x.size()), A_csc.x.data(), A_csc.i.data(), A_csc.p.data());

        const OSQPInt exitflag = osqp_setup(&osqp_solver_, P, q_vec.data(), A_mat, l_vec.data(), u_vec.data(), TOTAL_CONSTRAINT_SIZE, PROBLEM_SIZE, settings);

        //osqp_setup() copies whatever it needs out of P, A_mat and settings, so these can be freed
        //right away (the underlying std::vector storage in P_csc/A_csc/q_vec/l_vec/u_vec is only
        //required to stay alive up to this point).
        OSQPCscMatrix_free(P);
        OSQPCscMatrix_free(A_mat);
        OSQPSettings_free(settings);

        evaluate_osqp_exitflag(exitflag, "osqp_setup()");

        problem_size_ = PROBLEM_SIZE;
        constraint_size_ = TOTAL_CONSTRAINT_SIZE;
        osqp_solve_first_time_ = false;
    }
    else
    {
        //Same problem shape as before: update the existing solver's data in place instead of
        //rebuilding it, analogous to qpOASES_Solver's hotstart() call.
        evaluate_osqp_exitflag(osqp_update_data_vec(osqp_solver_, q_vec.data(), l_vec.data(), u_vec.data()), "osqp_update_data_vec()");
        evaluate_osqp_exitflag(osqp_update_data_mat(osqp_solver_,
                                                     P_csc.x.data(), nullptr, static_cast<OSQPInt>(P_csc.x.size()),
                                                     A_csc.x.data(), nullptr, static_cast<OSQPInt>(A_csc.x.size())),
                               "osqp_update_data_mat()");
    }

    //If the user provided a warm-start (e.g. a known feasible solution) for x, forward it to
    //OSQP. The dual variable y is left untouched (nullptr) since only x is user-provided.
    if(x0.size() != 0)
    {
        auto x0_vec = _vectorxd_to_std_vector_double(x0);
        evaluate_osqp_exitflag(osqp_warm_start(osqp_solver_, x0_vec.data(), nullptr), "osqp_warm_start()");
    }

    evaluate_osqp_exitflag(osqp_solve(osqp_solver_), "osqp_solve()");

    const OSQPInt status = osqp_solver_->info->status_val;
    if(status != OSQP_SOLVED && status != OSQP_SOLVED_INACCURATE)
        throw std::runtime_error("OSQP_Solver::solve_quadratic_program(): unable to solve quadratic program. OSQP status: "+std::string(osqp_solver_->info->status));

    std::vector<double> return_value_std(osqp_solver_->solution->x, osqp_solver_->solution->x + PROBLEM_SIZE);

    return _std_vector_double_to_vectorxd(return_value_std);
}

OSQP_Solver::Info OSQP_Solver::get_info() const
{
    if(osqp_solver_ == nullptr || osqp_solver_->info == nullptr)
        throw std::runtime_error("OSQP_Solver::get_info(): no solution information available. solve_quadratic_program() must be called successfully first.");

    Info info;
    info.obj_val = osqp_solver_->info->obj_val;
    info.dual_obj_val = osqp_solver_->info->dual_obj_val;
    info.prim_res = osqp_solver_->info->prim_res;
    info.dual_res = osqp_solver_->info->dual_res;

    //The dual solution y (Lagrange multiplier associated with l <= Ax <= u) has
    //constraint_size_ entries, i.e. the total number of inequality/equality rows used
    //in the last successful solve_quadratic_program() call.
    if(constraint_size_ > 0 && osqp_solver_->solution != nullptr && osqp_solver_->solution->y != nullptr)
    {
        std::vector<double> y_std(osqp_solver_->solution->y, osqp_solver_->solution->y + constraint_size_);
        info.dual_solution = _std_vector_double_to_vectorxd(y_std);
    }

    return info;
}

// Helper functions to help evaluate the wrapper when needed.
VectorXd OSQP_Solver::test_vectorxd(const VectorXd& v)
{
    return v;
}

MatrixXd OSQP_Solver::test_matrixxd(const MatrixXd& m)
{
    return m;
}

} // namespace M3
