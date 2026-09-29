/**
Based on the marinholab::solvers::qpoases::Solver wrapper in solver-qpoases,
adapted to use the OSQP solver.
*/
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <variant>

#include <marinholab/solvers/osqp.h>

#include <osqp.h>

namespace marinholab
{

namespace solvers
{

namespace osqp
{

// ---------------------------------------------------------------------------
// Option metadata
// ---------------------------------------------------------------------------

enum class OptionKind
{
    Boolean,
    Integer,
    Real,
    LinsysSolverType,
    PreconditionerType,
};

namespace detail
{

inline const std::map<std::string, OptionKind>& option_kinds()
{
    static const std::map<std::string, OptionKind> kinds = {
        // Wrapper-specific options.
        {"use_hotstart", OptionKind::Boolean},
        // OSQP `OSQPSettings` fields (1:1 mapping).
        // Linear algebra settings
        {"device", OptionKind::Integer},
        {"linsys_solver", OptionKind::LinsysSolverType},
        // Control settings
        {"allocate_solution", OptionKind::Boolean},
        {"verbose", OptionKind::Boolean},
        {"profiler_level", OptionKind::Integer},
        {"warm_starting", OptionKind::Boolean},
        {"scaling", OptionKind::Integer},
        {"polishing", OptionKind::Boolean},
        // ADMM parameters
        {"rho", OptionKind::Real},
        {"rho_is_vec", OptionKind::Boolean},
        {"sigma", OptionKind::Real},
        {"alpha", OptionKind::Real},
        // CG settings
        {"cg_max_iter", OptionKind::Integer},
        {"cg_tol_reduction", OptionKind::Integer},
        {"cg_tol_fraction", OptionKind::Real},
        {"cg_precond", OptionKind::PreconditionerType},
        // Adaptive rho logic
        {"adaptive_rho", OptionKind::Integer},
        {"adaptive_rho_interval", OptionKind::Integer},
        {"adaptive_rho_fraction", OptionKind::Real},
        {"adaptive_rho_tolerance", OptionKind::Real},
        // Termination parameters
        {"max_iter", OptionKind::Integer},
        {"eps_abs", OptionKind::Real},
        {"eps_rel", OptionKind::Real},
        {"eps_prim_inf", OptionKind::Real},
        {"eps_dual_inf", OptionKind::Real},
        {"scaled_termination", OptionKind::Boolean},
        {"check_termination", OptionKind::Integer},
        {"check_dualgap", OptionKind::Boolean},
        {"time_limit", OptionKind::Real},
        // Polishing parameters
        {"delta", OptionKind::Real},
        {"polish_refine_iter", OptionKind::Integer},
    };
    return kinds;
}

inline bool is_known_option(const std::string& key)
{
    return option_kinds().find(key) != option_kinds().end();
}

inline std::string to_lower(const std::string& s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/**
 * @brief Parses a boolean string, returning true when the value is true.
 */
inline bool parse_bool(const std::string& value)
{
    const std::string v = to_lower(value);
    if(v == "true" || v == "1" || v == "on")
        return true;
    if(v == "false" || v == "0" || v == "off")
        return false;
    throw std::invalid_argument("Invalid boolean value '" + value + "' (expected true/false).");
}

/**
 * @brief Parses an integer string.
 */
inline long long parse_int(const std::string& value)
{
    const char* begin = value.c_str();
    char* end = nullptr;
    const long long parsed = std::strtoll(begin, &end, 10);
    if(begin == end || *end != '\0')
        throw std::invalid_argument("Invalid integer value '" + value + "'.");
    return parsed;
}

/**
 * @brief Parses a floating-point string.
 */
inline double parse_real(const std::string& value)
{
    const char* begin = value.c_str();
    char* end = nullptr;
    const double parsed = std::strtod(begin, &end);
    if(begin == end || *end != '\0')
        throw std::invalid_argument("Invalid number value '" + value + "'.");
    return parsed;
}

/**
 * @brief Parses an enumeration value by name, e.g. "OSQP_DIRECT_SOLVER".
 */
template <typename T>
inline T parse_enum(const std::string& key, const std::string& value,
                    const std::map<std::string, T>& allowed)
{
    const auto it = allowed.find(value);
    if(it == allowed.end())
    {
        std::string options;
        for(const auto& pair : allowed)
            options += (options.empty() ? "" : ", ") + pair.first;
        throw std::invalid_argument("Invalid value '" + value + "' for option '" + key + "'. Allowed values: " + options + ".");
    }
    return it->second;
}

/** @brief Names of the OSQP `osqp_linsys_solver_type` values. */
inline const std::map<std::string, ::osqp_linsys_solver_type>& linsys_solver_types()
{
    static const std::map<std::string, ::osqp_linsys_solver_type> values = {
        {"OSQP_UNKNOWN_SOLVER", ::OSQP_UNKNOWN_SOLVER},
        {"OSQP_DIRECT_SOLVER", ::OSQP_DIRECT_SOLVER},
        {"OSQP_INDIRECT_SOLVER", ::OSQP_INDIRECT_SOLVER},
    };
    return values;
}

/** @brief Names of the OSQP `osqp_precond_type` values. */
inline const std::map<std::string, ::osqp_precond_type>& preconditioner_types()
{
    static const std::map<std::string, ::osqp_precond_type> values = {
        {"OSQP_NO_PRECONDITIONER", ::OSQP_NO_PRECONDITIONER},
        {"OSQP_DIAGONAL_PRECONDITIONER", ::OSQP_DIAGONAL_PRECONDITIONER},
    };
    return values;
}

/**
 * @brief The name of an enumeration value, from one of the tables above.
 */
template <typename T>
inline std::string name_of(const std::map<std::string, T>& names, T value)
{
    for(const auto& pair : names)
        if(pair.second == value)
            return pair.first;
    throw std::logic_error("Enumeration value without a name.");
}

/**
 * @brief Converts `value` to the kind of option `key`.
 *
 * Booleans accept `bool` or "true"/"false"; integers accept `long long` or
 * an integer literal; reals accept `double`, `long long` or a
 * floating-point literal; enumerations accept a value name.
 */
inline OptionValue normalize(const std::string& key, const OptionValue& value)
{
    const auto wrong_type = [&key](const std::string& expected) {
        return std::invalid_argument("Invalid value for option '" + key + "': expected " + expected + ".");
    };
    const auto* text = std::get_if<std::string>(&value);
    switch(option_kinds().at(key)) {
        case OptionKind::Boolean:
            if(const auto* b = std::get_if<bool>(&value))
                return *b;
            if(text)
                return parse_bool(*text);
            throw wrong_type("a bool");
        case OptionKind::Integer:
            if(const auto* i = std::get_if<long long>(&value))
                return *i;
            if(text)
                return parse_int(*text);
            throw wrong_type("an integer");
        case OptionKind::Real:
            if(const auto* d = std::get_if<double>(&value))
                return *d;
            if(const auto* i = std::get_if<long long>(&value))
                return static_cast<double>(*i);
            if(text)
                return parse_real(*text);
            throw wrong_type("a real number");
        case OptionKind::LinsysSolverType:
            if(text)
                return name_of(linsys_solver_types(), parse_enum(key, *text, linsys_solver_types()));
            throw wrong_type("a linsys_solver name, e.g. \"OSQP_DIRECT_SOLVER\"");
        case OptionKind::PreconditionerType:
            if(text)
                return name_of(preconditioner_types(), parse_enum(key, *text, preconditioner_types()));
            throw wrong_type("a preconditioner name, e.g. \"OSQP_DIAGONAL_PRECONDITIONER\"");
    }
    throw std::logic_error("Unhandled option kind.");
}

/**
 * @brief Every option's default value, taken directly from OSQP's
 *        `osqp_set_default_settings()` for a double-precision,
 *        direct-solver build.
 */
inline const std::map<std::string, OptionValue>& default_values()
{
    static const std::map<std::string, OptionValue> defaults = [] {
        OSQPSettings settings;
        osqp_set_default_settings(&settings);
        const auto integer = [](OSQPInt value) { return static_cast<long long>(value); };
        const auto real = [](OSQPFloat value) { return static_cast<double>(value); };
        std::map<std::string, OptionValue> d;
        d["use_hotstart"] = true;
        // Linear algebra settings
        d["device"] = integer(settings.device);
        d["linsys_solver"] = name_of(linsys_solver_types(), settings.linsys_solver);
        // Control settings
        d["allocate_solution"] = integer(settings.allocate_solution) != 0;
        d["verbose"] = false;  // quiet by default (see Configuration)
        d["profiler_level"] = integer(settings.profiler_level);
        d["warm_starting"] = integer(settings.warm_starting) != 0;
        d["scaling"] = integer(settings.scaling);
        d["polishing"] = integer(settings.polishing) != 0;
        // ADMM parameters
        d["rho"] = real(settings.rho);
        d["rho_is_vec"] = integer(settings.rho_is_vec) != 0;
        d["sigma"] = real(settings.sigma);
        d["alpha"] = real(settings.alpha);
        // CG settings
        d["cg_max_iter"] = integer(settings.cg_max_iter);
        d["cg_tol_reduction"] = integer(settings.cg_tol_reduction);
        d["cg_tol_fraction"] = real(settings.cg_tol_fraction);
        d["cg_precond"] = name_of(preconditioner_types(), settings.cg_precond);
        // Adaptive rho logic
        d["adaptive_rho"] = integer(settings.adaptive_rho);
        d["adaptive_rho_interval"] = integer(settings.adaptive_rho_interval);
        d["adaptive_rho_fraction"] = real(settings.adaptive_rho_fraction);
        d["adaptive_rho_tolerance"] = real(settings.adaptive_rho_tolerance);
        // Termination parameters
        d["max_iter"] = integer(settings.max_iter);
        d["eps_abs"] = real(settings.eps_abs);
        d["eps_rel"] = real(settings.eps_rel);
        d["eps_prim_inf"] = real(settings.eps_prim_inf);
        d["eps_dual_inf"] = real(settings.eps_dual_inf);
        d["scaled_termination"] = integer(settings.scaled_termination) != 0;
        d["check_termination"] = integer(settings.check_termination);
        d["check_dualgap"] = integer(settings.check_dualgap) != 0;
        d["time_limit"] = real(settings.time_limit);
        // Polishing parameters
        d["delta"] = real(settings.delta);
        d["polish_refine_iter"] = integer(settings.polish_refine_iter);
        return d;
    }();
    return defaults;
}

inline int64_t integer_from(const Configuration& configuration, const std::string& key)
{
    return std::get<long long>(configuration.get(key));
}

inline double real_from(const Configuration& configuration, const std::string& key)
{
    return std::get<double>(configuration.get(key));
}

inline bool boolean_from(const Configuration& configuration, const std::string& key)
{
    return std::get<bool>(configuration.get(key));
}

/**
 * @brief Translates the user configuration into an `OSQPSettings` object.
 *
 * Starts from OSQP's own defaults, overrides `verbose` with the (quieter)
 * wrapper default, then applies every option that was explicitly set.
 */
inline OSQPSettings settings_from(const Configuration& configuration)
{
    OSQPSettings settings;
    osqp_set_default_settings(&settings);

    settings.verbose = 0;  // quiet by default (see Configuration)

    // Apply only the explicitly set options.
    {
        auto has_set = [&configuration](const std::string& key) {
            return configuration.has(key);
        };
        if(has_set("device"))
            settings.device = static_cast<OSQPInt>(integer_from(configuration, "device"));
        if(has_set("linsys_solver"))
            settings.linsys_solver = parse_enum("linsys_solver", std::get<std::string>(configuration.get("linsys_solver")), linsys_solver_types());
        if(has_set("allocate_solution"))
            settings.allocate_solution = static_cast<OSQPInt>(boolean_from(configuration, "allocate_solution"));
        if(has_set("verbose"))
            settings.verbose = static_cast<OSQPInt>(boolean_from(configuration, "verbose"));
        if(has_set("profiler_level"))
            settings.profiler_level = static_cast<OSQPInt>(integer_from(configuration, "profiler_level"));
        if(has_set("warm_starting"))
            settings.warm_starting = static_cast<OSQPInt>(boolean_from(configuration, "warm_starting"));
        if(has_set("scaling"))
            settings.scaling = static_cast<OSQPInt>(integer_from(configuration, "scaling"));
        if(has_set("polishing"))
            settings.polishing = static_cast<OSQPInt>(boolean_from(configuration, "polishing"));
        if(has_set("rho"))
            settings.rho = real_from(configuration, "rho");
        if(has_set("rho_is_vec"))
            settings.rho_is_vec = static_cast<OSQPInt>(boolean_from(configuration, "rho_is_vec"));
        if(has_set("sigma"))
            settings.sigma = real_from(configuration, "sigma");
        if(has_set("alpha"))
            settings.alpha = real_from(configuration, "alpha");
        if(has_set("cg_max_iter"))
            settings.cg_max_iter = static_cast<OSQPInt>(integer_from(configuration, "cg_max_iter"));
        if(has_set("cg_tol_reduction"))
            settings.cg_tol_reduction = static_cast<OSQPInt>(integer_from(configuration, "cg_tol_reduction"));
        if(has_set("cg_tol_fraction"))
            settings.cg_tol_fraction = real_from(configuration, "cg_tol_fraction");
        if(has_set("cg_precond"))
            settings.cg_precond = parse_enum("cg_precond", std::get<std::string>(configuration.get("cg_precond")), preconditioner_types());
        if(has_set("adaptive_rho"))
            settings.adaptive_rho = static_cast<OSQPInt>(integer_from(configuration, "adaptive_rho"));
        if(has_set("adaptive_rho_interval"))
            settings.adaptive_rho_interval = static_cast<OSQPInt>(integer_from(configuration, "adaptive_rho_interval"));
        if(has_set("adaptive_rho_fraction"))
            settings.adaptive_rho_fraction = real_from(configuration, "adaptive_rho_fraction");
        if(has_set("adaptive_rho_tolerance"))
            settings.adaptive_rho_tolerance = real_from(configuration, "adaptive_rho_tolerance");
        if(has_set("max_iter"))
            settings.max_iter = static_cast<OSQPInt>(integer_from(configuration, "max_iter"));
        if(has_set("eps_abs"))
            settings.eps_abs = real_from(configuration, "eps_abs");
        if(has_set("eps_rel"))
            settings.eps_rel = real_from(configuration, "eps_rel");
        if(has_set("eps_prim_inf"))
            settings.eps_prim_inf = real_from(configuration, "eps_prim_inf");
        if(has_set("eps_dual_inf"))
            settings.eps_dual_inf = real_from(configuration, "eps_dual_inf");
        if(has_set("scaled_termination"))
            settings.scaled_termination = static_cast<OSQPInt>(boolean_from(configuration, "scaled_termination"));
        if(has_set("check_termination"))
            settings.check_termination = static_cast<OSQPInt>(integer_from(configuration, "check_termination"));
        if(has_set("check_dualgap"))
            settings.check_dualgap = static_cast<OSQPInt>(boolean_from(configuration, "check_dualgap"));
        if(has_set("time_limit"))
            settings.time_limit = real_from(configuration, "time_limit");
        if(has_set("delta"))
            settings.delta = real_from(configuration, "delta");
        if(has_set("polish_refine_iter"))
            settings.polish_refine_iter = static_cast<OSQPInt>(integer_from(configuration, "polish_refine_iter"));
    }
    return settings;
}

inline void evaluate_osqp_exitflag(OSQPInt exitflag, const std::string& context)
{
    if(exitflag != 0)
    {
        throw std::runtime_error("Solver::solve_quadratic_program(): "+context+" failed. OSQP returned error code "+std::to_string(exitflag)+": "+std::string(osqp_error_message(exitflag)));
    }
}

} // namespace detail

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

Configuration::Configuration() = default;
Configuration::Configuration(const Configuration& other) = default;
Configuration& Configuration::operator=(const Configuration& other) = default;
Configuration::~Configuration() = default;

void Configuration::set(const std::string& key, const OptionValue& value)
{
    if(!detail::is_known_option(key))
        throw std::invalid_argument("Unknown option '" + key + "'. Use keys() for the valid option names.");
    // Validate and convert up-front so type errors surface at set-time.
    options_[key] = detail::normalize(key, value);
}

void Configuration::set(const std::string& key, const char* value)
{
    set(key, OptionValue(std::string(value)));
}

OptionValue Configuration::get(const std::string& key) const
{
    if(!detail::is_known_option(key))
        throw std::invalid_argument("Unknown option '" + key + "'.");
    const auto it = options_.find(key);
    if(it != options_.end())
        return it->second;
    return detail::default_values().at(key);
}

bool Configuration::has(const std::string& key) const
{
    if(!detail::is_known_option(key))
        throw std::invalid_argument("Unknown option '" + key + "'. Use keys() for the valid option names.");
    return options_.find(key) != options_.end();
}

std::vector<std::string> Configuration::keys() const
{
    const auto& kinds = detail::option_kinds();
    std::vector<std::string> out;
    out.reserve(kinds.size());
    for(const auto& pair : kinds)
        out.push_back(pair.first);
    return out;
}

void Configuration::reset(const std::string& key)
{
    if(!detail::is_known_option(key))
        throw std::invalid_argument("Unknown option '" + key + "'. Use keys() for the valid option names.");
    options_.erase(key);
}

void Configuration::reset_all()
{
    options_.clear();
}

std::map<std::string, OptionValue> Configuration::defaults() const
{
    return detail::default_values();
}

// ---------------------------------------------------------------------------
// Solver
// ---------------------------------------------------------------------------

struct Solver::Impl
{
    /** @brief True until the first solve has set up the solver. */
    bool osqp_solve_first_time_;
    /** @brief The underlying OSQP solver. */
    ::OSQPSolver* osqp_solver_;
    /** @brief Active configuration, copied at construction. */
    Configuration configuration_;
    /**
     * @brief Dimensions used in the last successful `osqp_setup()` call.
     * Used to detect whether a new call is compatible with hotstarting or
     * requires a fresh `osqp_setup()`.
     */
    OSQPInt problem_size_;
    OSQPInt constraint_size_;

    /**
     * @brief Holds the CSC (compressed-sparse-column) arrays of a
     *        converted Eigen matrix.
     *
     * The vectors own the storage so that it outlives the temporary
     * `OSQPCscMatrix` wrapper used when calling `osqp_setup()` /
     * `osqp_update_data_mat()`.
     */
    struct CSCMatrixData
    {
        std::vector<OSQPFloat> x;
        std::vector<OSQPInt> i;
        std::vector<OSQPInt> p;
        OSQPInt rows{0};
        OSQPInt cols{0};
    };

    Impl(const Configuration& configuration)
        : osqp_solve_first_time_(true)
        , osqp_solver_(nullptr)
        , configuration_(configuration)
        , problem_size_(0)
        , constraint_size_(0)
    {
    }

    ~Impl()
    {
        cleanup();
    }

    /**
     * @brief Copies an Eigen vector into a std::vector<double>.
     * @param vectorxd Source vector.
     * @return A copy of the vector's data.
     */
    std::vector<double> vectorxd_to_std_vector_double(const Eigen::VectorXd& vectorxd) const
    {
        std::vector<double> vec(vectorxd.data(), vectorxd.data() + vectorxd.rows() * vectorxd.cols());
        return vec;
    }

    /**
     * @brief Maps a std::vector<double> into an Eigen vector.
     * @param std_vector_double Source vector.
     * @return An Eigen vector wrapping the source data.
     */
    Eigen::VectorXd std_vector_double_to_vectorxd(std::vector<double> std_vector_double) const
    {
        double* ptr = &std_vector_double[0];
        Eigen::Map<Eigen::VectorXd> vec(ptr, std_vector_double.size());
        return vec;
    }

    /**
     * @brief Converts a dense n x n matrix into the upper-triangular CSC
     *        representation required by OSQP for P.
     * @param M Source matrix.
     * @return The CSC arrays of the (upper triangle of the) matrix.
     */
    static CSCMatrixData dense_to_csc_upper_triangular(const Eigen::MatrixXd& M)
    {
        if(M.rows() != M.cols())
            throw std::runtime_error("Solver::solve_quadratic_program(): H must be square. H.rows()="+std::to_string(M.rows())+" but H.cols()="+std::to_string(M.cols())+".");

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

    /**
     * @brief Converts a dense m x n matrix into a structurally-dense CSC
     *        representation (i.e. every entry, including zeros, is stored
     *        explicitly).
     *
     * Keeping the sparsity pattern stable across calls is what allows
     * `osqp_update_data_mat()` to be used when hotstarting.
     * @param M Source matrix.
     * @return The CSC arrays of the matrix.
     */
    static CSCMatrixData dense_to_csc(const Eigen::MatrixXd& M)
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

    /**
     * @brief Releases the current `osqp_solver_` instance, if any.
     */
    void cleanup()
    {
        if(osqp_solver_ != nullptr)
        {
            osqp_cleanup(osqp_solver_);
            osqp_solver_ = nullptr;
        }
    }
};

Solver::Solver(const Configuration& configuration)
    : impl_(std::make_unique<Impl>(configuration))
{
}

Solver::Solver(Solver&& other) noexcept = default;

Solver& Solver::operator=(Solver&& other) noexcept
{
    if(this != &other)
        impl_ = std::move(other.impl_);
    return *this;
}

Solver::~Solver() = default;

Eigen::VectorXd Solver::solve_quadratic_program(const Eigen::MatrixXd& H, const Eigen::VectorXd& f, const Eigen::MatrixXd& A, const Eigen::VectorXd& b, const Eigen::MatrixXd& Aeq, const Eigen::VectorXd& beq, const Eigen::VectorXd& x0, const Eigen::VectorXd& y0)
{
    const OSQPInt PROBLEM_SIZE = static_cast<OSQPInt>(H.rows());
    const OSQPInt INEQUALITY_CONSTRAINT_SIZE = static_cast<OSQPInt>(b.size());
    const OSQPInt EQUALITY_CONSTRAINT_SIZE = static_cast<OSQPInt>(beq.size());
    const OSQPInt TOTAL_CONSTRAINT_SIZE = INEQUALITY_CONSTRAINT_SIZE + EQUALITY_CONSTRAINT_SIZE;

    ///Check sizes
    //Objective function
    if(H.rows() != H.cols())
        throw std::runtime_error("Solver::solve_quadratic_program(): H must be symmetric. H.rows()="+std::to_string(H.rows())+" but H.cols()="+std::to_string(H.cols())+".");
    if(f.size() != H.rows())
        throw std::runtime_error("Solver::solve_quadratic_program(): f must be compatible with H. H.rows()=H.cols()="+std::to_string(H.rows())+" but f.size()="+std::to_string(f.size())+".");

    //Optional warm-start (e.g. a known feasible solution) for the primal variable x.
    if(x0.size() != 0 && x0.size() != PROBLEM_SIZE)
        throw std::runtime_error("Solver::solve_quadratic_program(): x0 must be compatible with H. H.rows()=H.cols()="+std::to_string(H.rows())+" but x0.size()="+std::to_string(x0.size())+".");

    //Optional warm-start (e.g. a dual solution obtained from get_info().dual_solution in a
    //previous call) for the dual variable y.
    if(y0.size() != 0 && y0.size() != TOTAL_CONSTRAINT_SIZE)
        throw std::runtime_error("Solver::solve_quadratic_program(): y0 must be compatible with the total number of constraints. b.size()+beq.size()="+std::to_string(TOTAL_CONSTRAINT_SIZE)+" but y0.size()="+std::to_string(y0.size())+".");

    //Inequality constraints
    if(b.size() != A.rows())
        throw std::runtime_error("Solver::solve_quadratic_program(): size of b="+std::to_string(b.size())+" should be compatible with rows of A="+std::to_string(A.rows())+".");
    if(INEQUALITY_CONSTRAINT_SIZE != 0 && A.cols() != PROBLEM_SIZE)
        throw std::runtime_error("Solver::solve_quadratic_program(): A.cols()="+std::to_string(A.cols())+" should be compatible with H.rows()="+std::to_string(PROBLEM_SIZE)+".");

    //Equality constraints
    if(beq.size() != Aeq.rows())
        throw std::runtime_error("Solver::solve_quadratic_program(): size of beq="+std::to_string(beq.size())+" should be compatible with rows of Aeq="+std::to_string(Aeq.rows())+".");
    if(EQUALITY_CONSTRAINT_SIZE != 0 && Aeq.cols() != PROBLEM_SIZE)
        throw std::runtime_error("Solver::solve_quadratic_program(): Aeq.cols()="+std::to_string(Aeq.cols())+" should be compatible with H.rows()="+std::to_string(PROBLEM_SIZE)+".");

    //Stack the inequality and equality constraints into OSQP's single l <= Ax <= u form.
    //Equality rows get l==u==beq. Inequality rows get l=-infinity, u=b.
    Eigen::MatrixXd A_extended(TOTAL_CONSTRAINT_SIZE, PROBLEM_SIZE);
    Eigen::VectorXd l_extended(TOTAL_CONSTRAINT_SIZE);
    Eigen::VectorXd u_extended(TOTAL_CONSTRAINT_SIZE);

    if(INEQUALITY_CONSTRAINT_SIZE > 0)
    {
        A_extended.topRows(INEQUALITY_CONSTRAINT_SIZE) = A;
        l_extended.head(INEQUALITY_CONSTRAINT_SIZE) = Eigen::VectorXd::Constant(INEQUALITY_CONSTRAINT_SIZE, -OSQP_INFTY);
        u_extended.head(INEQUALITY_CONSTRAINT_SIZE) = b;
    }
    if(EQUALITY_CONSTRAINT_SIZE > 0)
    {
        A_extended.bottomRows(EQUALITY_CONSTRAINT_SIZE) = Aeq;
        l_extended.tail(EQUALITY_CONSTRAINT_SIZE) = beq;
        u_extended.tail(EQUALITY_CONSTRAINT_SIZE) = beq;
    }

    auto& impl = *impl_;

    //Convert the dense Eigen data into the CSC format required by OSQP. Every entry (including
    //zeros) is stored explicitly so that the sparsity pattern is stable across calls, which is
    //what allows osqp_update_data_mat() to be used below when hotstarting.
    Solver::Impl::CSCMatrixData P_csc = Solver::Impl::dense_to_csc_upper_triangular(H);
    Solver::Impl::CSCMatrixData A_csc = Solver::Impl::dense_to_csc(A_extended);
    auto q_vec = impl.vectorxd_to_std_vector_double(f);
    auto l_vec = impl.vectorxd_to_std_vector_double(l_extended);
    auto u_vec = impl.vectorxd_to_std_vector_double(u_extended);

    const bool problem_shape_changed = (PROBLEM_SIZE != impl.problem_size_) || (TOTAL_CONSTRAINT_SIZE != impl.constraint_size_);

    if(impl.osqp_solve_first_time_ || problem_shape_changed || !detail::boolean_from(impl.configuration_, "use_hotstart"))
    {
        //(Re)create the solver from scratch. This is required the first time, whenever the
        //problem dimensions change, or whenever hotstarting is disabled in the configuration.
        impl.cleanup();

        OSQPSettings* settings = OSQPSettings_new();
        if(settings == nullptr)
            throw std::runtime_error("Solver::solve_quadratic_program(): unable to allocate OSQPSettings.");

        //Overwrite OSQP's defaults with the user's configuration.
        const OSQPSettings user_settings = detail::settings_from(impl.configuration_);
        *settings = user_settings;

        OSQPCscMatrix* P = OSQPCscMatrix_new(PROBLEM_SIZE, PROBLEM_SIZE, static_cast<OSQPInt>(P_csc.x.size()), P_csc.x.data(), P_csc.i.data(), P_csc.p.data());
        OSQPCscMatrix* A_mat = OSQPCscMatrix_new(TOTAL_CONSTRAINT_SIZE, PROBLEM_SIZE, static_cast<OSQPInt>(A_csc.x.size()), A_csc.x.data(), A_csc.i.data(), A_csc.p.data());

        const OSQPInt exitflag = osqp_setup(&impl.osqp_solver_, P, q_vec.data(), A_mat, l_vec.data(), u_vec.data(), TOTAL_CONSTRAINT_SIZE, PROBLEM_SIZE, settings);

        //osqp_setup() copies whatever it needs out of P, A_mat and settings, so these can be
        //freed right away (the underlying std::vector storage in P_csc/A_csc/q_vec/l_vec/u_vec
        //is only required to stay alive up to this point).
        OSQPCscMatrix_free(P);
        OSQPCscMatrix_free(A_mat);
        OSQPSettings_free(settings);

        detail::evaluate_osqp_exitflag(exitflag, "osqp_setup()");

        impl.problem_size_ = PROBLEM_SIZE;
        impl.constraint_size_ = TOTAL_CONSTRAINT_SIZE;
        impl.osqp_solve_first_time_ = false;
    }
    else
    {
        //Same problem shape as before: update the existing solver's data in place instead of
        //rebuilding it, analogous to qpOASES_Solver's hotstart() call.
        detail::evaluate_osqp_exitflag(osqp_update_data_vec(impl.osqp_solver_, q_vec.data(), l_vec.data(), u_vec.data()), "osqp_update_data_vec()");
        detail::evaluate_osqp_exitflag(osqp_update_data_mat(impl.osqp_solver_,
                                                             P_csc.x.data(), nullptr, static_cast<OSQPInt>(P_csc.x.size()),
                                                             A_csc.x.data(), nullptr, static_cast<OSQPInt>(A_csc.x.size())),
                                       "osqp_update_data_mat()");
    }

    //If the user provided a warm-start for x and/or y, forward it to OSQP. Either can be
    //provided independently; osqp_warm_start() accepts nullptr for whichever one is omitted.
    if(x0.size() != 0 || y0.size() != 0)
    {
        std::vector<double> x0_vec;
        std::vector<double> y0_vec;
        const OSQPFloat* x0_ptr = nullptr;
        const OSQPFloat* y0_ptr = nullptr;

        if(x0.size() != 0)
        {
            x0_vec = impl.vectorxd_to_std_vector_double(x0);
            x0_ptr = x0_vec.data();
        }
        if(y0.size() != 0)
        {
            y0_vec = impl.vectorxd_to_std_vector_double(y0);
            y0_ptr = y0_vec.data();
        }

        detail::evaluate_osqp_exitflag(osqp_warm_start(impl.osqp_solver_, x0_ptr, y0_ptr), "osqp_warm_start()");
    }

    detail::evaluate_osqp_exitflag(osqp_solve(impl.osqp_solver_), "osqp_solve()");

    const OSQPInt status = impl.osqp_solver_->info->status_val;
    if(status != OSQP_SOLVED && status != OSQP_SOLVED_INACCURATE)
        throw std::runtime_error("Solver::solve_quadratic_program(): unable to solve quadratic program. OSQP status: "+std::string(impl.osqp_solver_->info->status));

    std::vector<double> return_value_std(impl.osqp_solver_->solution->x, impl.osqp_solver_->solution->x + PROBLEM_SIZE);

    return impl.std_vector_double_to_vectorxd(return_value_std);
}

Solver::Info Solver::get_info() const
{
    const auto& impl = *impl_;
    if(impl.osqp_solver_ == nullptr || impl.osqp_solver_->info == nullptr)
        throw std::runtime_error("Solver::get_info(): no solution information available. solve_quadratic_program() must be called successfully first.");

    Info info;
    info.obj_val = impl.osqp_solver_->info->obj_val;
    info.dual_obj_val = impl.osqp_solver_->info->dual_obj_val;
    info.prim_res = impl.osqp_solver_->info->prim_res;
    info.dual_res = impl.osqp_solver_->info->dual_res;

    //The dual solution y (Lagrange multiplier associated with l <= Ax <= u) has
    //constraint_size_ entries, i.e. the total number of inequality/equality rows used
    //in the last successful solve_quadratic_program() call.
    if(impl.constraint_size_ > 0 && impl.osqp_solver_->solution != nullptr && impl.osqp_solver_->solution->y != nullptr)
    {
        std::vector<double> y_std(impl.osqp_solver_->solution->y, impl.osqp_solver_->solution->y + impl.constraint_size_);
        info.dual_solution = impl.std_vector_double_to_vectorxd(y_std);
    }

    return info;
}

// Helper functions to help evaluate the wrapper when needed.
Eigen::VectorXd Solver::test_vectorxd(const Eigen::VectorXd& v)
{
    return v;
}

Eigen::MatrixXd Solver::test_matrixxd(const Eigen::MatrixXd& m)
{
    return m;
}

} // namespace osqp

} // namespace solvers

} // namespace marinholab
