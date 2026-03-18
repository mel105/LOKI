#include <catch2/catch_test_macros.hpp>

#include "loki/core/exceptions.hpp"

// =============================================================================
// test_exceptions.cpp
//
// Tests for the LOKI exception hierarchy defined in exceptions.hpp.
// Verifies that:
//   - each exception type carries the correct message
//   - the inheritance chain is intact (catch base catches derived)
//   - exceptions are throwable and catchable as std::exception
// =============================================================================

// -----------------------------------------------------------------------------
// LOKIException -- base
// -----------------------------------------------------------------------------

TEST_CASE("LOKIException carries its message", "[exceptions][base]")
{
    const std::string msg = "something went wrong";

    REQUIRE_THROWS_AS(
        []{ throw loki::LOKIException("something went wrong"); }(),
        loki::LOKIException
    );

    try {
        throw loki::LOKIException(msg);
    } catch (const loki::LOKIException& ex) {
        REQUIRE(std::string(ex.what()) == msg);
    }
}

TEST_CASE("LOKIException is catchable as std::exception", "[exceptions][base]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::LOKIException("base"); }(),
        std::exception
    );
}

// -----------------------------------------------------------------------------
// DataException hierarchy
// -----------------------------------------------------------------------------

TEST_CASE("DataException is catchable as LOKIException", "[exceptions][data]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::DataException("bad data"); }(),
        loki::LOKIException
    );
}

TEST_CASE("SeriesTooShortException is catchable as DataException", "[exceptions][data]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::SeriesTooShortException("too short"); }(),
        loki::DataException
    );
}

TEST_CASE("SeriesTooShortException is catchable as LOKIException", "[exceptions][data]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::SeriesTooShortException("too short"); }(),
        loki::LOKIException
    );
}

TEST_CASE("MissingValueException is catchable as DataException", "[exceptions][data]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::MissingValueException("missing"); }(),
        loki::DataException
    );
}

// -----------------------------------------------------------------------------
// IoException hierarchy
// -----------------------------------------------------------------------------

TEST_CASE("FileNotFoundException is catchable as IoException", "[exceptions][io]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::FileNotFoundException("/no/such/file"); }(),
        loki::IoException
    );
}

TEST_CASE("FileNotFoundException is catchable as LOKIException", "[exceptions][io]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::FileNotFoundException("/no/such/file"); }(),
        loki::LOKIException
    );
}

TEST_CASE("ParseException is catchable as IoException", "[exceptions][io]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::ParseException("line 42: bad token"); }(),
        loki::IoException
    );
}

// -----------------------------------------------------------------------------
// ConfigException
// -----------------------------------------------------------------------------

TEST_CASE("ConfigException is catchable as LOKIException", "[exceptions][config]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::ConfigException("missing key"); }(),
        loki::LOKIException
    );
}

// -----------------------------------------------------------------------------
// AlgorithmException hierarchy
// -----------------------------------------------------------------------------

TEST_CASE("AlgorithmException is catchable as LOKIException", "[exceptions][algorithm]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::AlgorithmException("bad algo"); }(),
        loki::LOKIException
    );
}

TEST_CASE("ConvergenceException is catchable as AlgorithmException", "[exceptions][algorithm]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::ConvergenceException("did not converge"); }(),
        loki::AlgorithmException
    );
}

TEST_CASE("SingularMatrixException is catchable as AlgorithmException", "[exceptions][algorithm]")
{
    REQUIRE_THROWS_AS(
        []{ throw loki::SingularMatrixException("singular"); }(),
        loki::AlgorithmException
    );
}

// -----------------------------------------------------------------------------
// Message round-trip
// -----------------------------------------------------------------------------

TEST_CASE("Exception message survives catch by base type", "[exceptions][message]")
{
    // Throw a leaf, catch the root -- what() must still return the original text.
    const std::string expected = "matrix is singular at row 7";

    try {
        throw loki::SingularMatrixException(expected);
    } catch (const loki::LOKIException& ex) {
        REQUIRE(std::string(ex.what()) == expected);
    }
}
