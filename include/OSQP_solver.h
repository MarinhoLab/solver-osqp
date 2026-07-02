#pragma once

#include <vector>
#include <Eigen/Dense>
using namespace Eigen;

#include <osqp.h>

namespace M3
{

class OSQP_Solver
{
    public:
        struct Configuration
        {
            //Maximum number of ADMM iterations. See OSQPSettings::max_iter.
            OSQPInt maximum_iterations = OSQP_MAX_ITER;
            //Absolute/relative solution tolerances. See OSQPSettings::eps_abs/eps_rel.
            OSQPFloat eps_absolute = OSQP_EPS_ABS;
            OSQPFloat eps_relative = OSQP_EPS_REL;
            //Primal/dual infeasibility tolerances. See OSQPSettings::eps_prim_inf/eps_dual_inf.
            OSQPFloat eps_primal_infeasibility = OSQP_EPS_PRIM_INF;
            OSQPFloat eps_dual_infeasibility = OSQP_EPS_DUAL_INF;
            //boolean; write out solver progress. See OSQPSettings::verbose.
            OSQPInt verbose = 0;
            //boolean; polish ADMM solution. See OSQPSettings::polishing.
            OSQPInt polishing = OSQP_POLISHING;
            //boolean; use OSQP's own warm-starting between consecutive osqp_solve() calls. See OSQPSettings::warm_starting.
            OSQPInt warm_starting = OSQP_WARM_STARTING;
            //If true, and the problem dimensions/sparsity pattern are unchanged since the last call,
            //solve_quadratic_program() will reuse the existing OSQPSolver instance and update its data
            //in place (osqp_update_data_vec/osqp_update_data_mat) instead of calling osqp_setup() again.
            bool use_hotstart = true;
            Configuration(); //https://stackoverflow.com/questions/53408962/try-to-understand-compiler-error-message-default-member-initializer-required-be
        };

        /**
         * @brief Named solution-quality values obtained from the last successful call to
         * solve_quadratic_program(). See OSQPInfo in osqp_api_types.h for further details.
         */
        struct Info
        {
            //Primal objective value. See OSQPInfo::obj_val.
            OSQPFloat obj_val = 0.0;
            //Dual objective value. See OSQPInfo::dual_obj_val.
            OSQPFloat dual_obj_val = 0.0;
            //Norm of the primal residual. See OSQPInfo::prim_res.
            OSQPFloat prim_res = 0.0;
            //Norm of the dual residual. See OSQPInfo::dual_res.
            OSQPFloat dual_res = 0.0;
            //Dual solution, i.e. the Lagrange multiplier associated with l <= Ax <= u.
            //See OSQPSolution::y.
            VectorXd dual_solution;
            Info(); //https://stackoverflow.com/questions/53408962/try-to-understand-compiler-error-message-default-member-initializer-required-be
        };
    protected:
        //Holds the CSC (compressed-sparse-column) arrays of a converted Eigen matrix.
        //The vectors own the storage so that it outlives the temporary OSQPCscMatrix
        //wrapper used when calling osqp_setup()/osqp_update_data_mat().
        struct CSCMatrixData
        {
            std::vector<OSQPFloat> x;
            std::vector<OSQPInt> i;
            std::vector<OSQPInt> p;
            OSQPInt rows{0};
            OSQPInt cols{0};
        };

        bool osqp_solve_first_time_;
        ::OSQPSolver* osqp_solver_;
        Configuration configuration_;

        //Dimensions used in the last successful osqp_setup() call. Used to detect whether
        //a new call to solve_quadratic_program() is compatible with hotstarting or requires
        //a fresh osqp_setup().
        OSQPInt problem_size_;
        OSQPInt constraint_size_;

        //https://github.com/SmartArmStack/sas_conversions/blob/master/src/eigen3_std_conversions.cpp
        //A copy from sas
        std::vector<double> _vectorxd_to_std_vector_double(const VectorXd& vectorxd);

        //Another copy from sas
        VectorXd _std_vector_double_to_vectorxd(std::vector<double> std_vector_double) const;

        //Converts a dense n x n matrix into the upper-triangular CSC representation required by OSQP for P.
        static CSCMatrixData _dense_to_csc_upper_triangular(const MatrixXd& M);

        //Converts a dense m x n matrix into a structurally-dense CSC representation (i.e. every
        //entry, including zeros, is stored explicitly). Keeping the sparsity pattern stable
        //across calls is what allows osqp_update_data_mat() to be used when hotstarting.
        static CSCMatrixData _dense_to_csc(const MatrixXd& M);

        //Releases the current osqp_solver_ instance, if any.
        void _cleanup();

    public:
        OSQP_Solver(const Configuration& configuration = OSQP_Solver::Configuration());
        ~OSQP_Solver();

        //Not copyable, as this class owns a raw ::OSQPSolver* that is not reference counted.
        OSQP_Solver(const OSQP_Solver&) = delete;
        OSQP_Solver& operator=(const OSQP_Solver&) = delete;

        /**
         * @brief
         *   Solves the following quadratic program
         *   min(x)  0.5*x'Hx + f'x
         *   s.t.    Ax <= b
         *           Aeqx = beq.
         * Method signature is compatible with MATLAB's 'quadprog'.
         * @param H the n x n matrix of the quadratic coefficients of the decision variables.
         * @param f the n x 1 vector of the linear coefficients of the decision variables.
         * @param A the m x n matrix of inequality constraints.
         * @param b the m x 1 value for the inequality constraints.
         * @param Aeq the m x n matrix of equality constraints.
         * @param beq the m x 1 value for the inequality constraints.
         * @param x0 optional n x 1 warm-start for the primal variable x, e.g. a known feasible
         *   solution. Passed to OSQP via osqp_warm_start(). Pass an empty vector (the default)
         *   to skip warm-starting. Must be compatible with H.rows() when provided.
         * @param y0 optional warm-start for the dual variable y, e.g. a dual solution obtained
         *   from get_info().dual_solution in a previous call. Passed to OSQP via
         *   osqp_warm_start(). Pass an empty vector (the default) to skip dual warm-starting.
         *   Must have b.size()+beq.size() entries when provided.
         * @return the optimal x
         */
        VectorXd solve_quadratic_program(const MatrixXd& H, const VectorXd& f, const MatrixXd& A, const VectorXd& b, const MatrixXd& Aeq, const VectorXd& beq, const VectorXd& x0 = VectorXd(), const VectorXd& y0 = VectorXd());

        /**
         * @brief Returns named solution-quality values (obj_val, dual_obj_val, prim_res, dual_res)
         * and the dual solution (dual_solution) from the last successful call to
         * solve_quadratic_program().
         * @throws std::runtime_error if solve_quadratic_program() has not been called successfully yet.
         * @return an Info instance with the values populated.
         */
        Info get_info() const;

        VectorXd test_vectorxd(const VectorXd& v);
        MatrixXd test_matrixxd(const MatrixXd& m);

};

}
