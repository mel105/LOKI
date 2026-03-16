#pragma once

#include <stdexcept>
#include <string>

namespace loki {  

/**
 * @brief Root exception for all LOKI errors.
 *
 * All exceptions thrown by LOKI libraries derive from this class.
 * Catch this type to handle any LOKI-specific error generically.
 */
class LOKIException : public std::exception {
public:
    explicit LOKIException(std::string message)
        : m_message(std::move(message)) {}

    /// @brief Returns the error message.
    [[nodiscard]] const char* what() const noexcept override {
        return m_message.c_str();
    }

private:
    std::string m_message;
};

// ─────────────────────────────────────────────
//  Data exceptions
// ─────────────────────────────────────────────

/**
 * @brief Thrown when input data is invalid, missing, or malformed.
 */
class DataException : public LOKIException {
public:
    explicit DataException(std::string message)
        : LOKIException(std::move(message)) {}
};

/**
 * @brief Thrown when a time series is too short for the requested operation.
 *
 * Typically raised when a series contains fewer points than the algorithm's
 * minimum required length (e.g. MIN_SERIES_LENGTH).
 */
class SeriesTooShortException : public DataException {
public:
    explicit SeriesTooShortException(std::string message)
        : DataException(std::move(message)) {}
};

/**
 * @brief Thrown when required values are absent from the input data.
 *
 * Raised when NaN, gaps, or explicitly missing entries prevent computation.
 */
class MissingValueException : public DataException {
public:
    explicit MissingValueException(std::string message)
        : DataException(std::move(message)) {}
};

// ─────────────────────────────────────────────
//  I/O exceptions
// ─────────────────────────────────────────────

/**
 * @brief Thrown when a file or stream I/O operation fails.
 */
class IoException : public LOKIException {
public:
    explicit IoException(std::string message)
        : LOKIException(std::move(message)) {}
};

/**
 * @brief Thrown when a required file cannot be located or opened.
 */
class FileNotFoundException : public IoException {
public:
    explicit FileNotFoundException(std::string message)
        : IoException(std::move(message)) {}
};

/**
 * @brief Thrown when input data cannot be parsed into the expected format.
 *
 * Covers malformed CSV/JSON, unexpected column counts, type mismatches, etc.
 */
class ParseException : public IoException {
public:
    explicit ParseException(std::string message)
        : IoException(std::move(message)) {}
};

// ─────────────────────────────────────────────
//  Configuration exceptions
// ─────────────────────────────────────────────

/**
 * @brief Thrown when configuration parameters are invalid or inconsistent.
 *
 * Raised for missing required keys, out-of-range values, or contradictory
 * settings in the application configuration.
 */
class ConfigException : public LOKIException {
public:
    explicit ConfigException(std::string message)
        : LOKIException(std::move(message)) {}
};

// ─────────────────────────────────────────────
//  Algorithm exceptions
// ─────────────────────────────────────────────

/**
 * @brief Thrown when a numerical or algorithmic failure occurs.
 */
class AlgorithmException : public LOKIException {
public:
    explicit AlgorithmException(std::string message)
        : LOKIException(std::move(message)) {}
};

/**
 * @brief Thrown when an iterative algorithm fails to converge within the allowed steps.
 */
class ConvergenceException : public AlgorithmException {
public:
    explicit ConvergenceException(std::string message)
        : AlgorithmException(std::move(message)) {}
};

/**
 * @brief Thrown when a matrix inversion or decomposition encounters a singular matrix.
 *
 * Indicates that the system is numerically underdetermined or ill-conditioned.
 */
class SingularMatrixException : public AlgorithmException {
public:
    explicit SingularMatrixException(std::string message)
        : AlgorithmException(std::move(message)) {}
};

}