#pragma once

#include "loki/core/exceptions.hpp"

#include <cstdio>
#include <string>

namespace loki {

/**
 * @brief RAII wrapper around a gnuplot pipe.
 *
 * Opens a persistent gnuplot process via popen() on construction and closes
 * it cleanly on destruction. All gnuplot commands are sent through send() or
 * the equivalent operator().
 *
 * Usage example:
 * @code
 *   loki::Gnuplot gp;
 *   gp("set terminal png");
 *   gp("set output 'out.png'");
 *   gp("plot 'data.dat' u 1:2 w l");
 * @endcode
 *
 * @throws loki::IoException if gnuplot cannot be opened on construction.
 * @throws loki::IoException if a command cannot be sent (pipe closed).
 */
class Gnuplot {
public:

    // ── Construction / destruction ────────────────────────────────────────────

    /**
     * @brief Opens a gnuplot pipe.
     *
     * Launches gnuplot with the -persist flag so the window stays open after
     * the pipe is closed.
     *
     * @throws IoException if popen() fails (gnuplot not found or not in PATH).
     */
    Gnuplot();

    /**
     * @brief Closes the gnuplot pipe.
     *
     * Sends the "exit" command to gnuplot before closing. Never throws.
     */
    ~Gnuplot() noexcept;

    // Non-copyable, movable
    Gnuplot(const Gnuplot&)            = delete;
    Gnuplot& operator=(const Gnuplot&) = delete;
    Gnuplot(Gnuplot&&)                 noexcept;
    Gnuplot& operator=(Gnuplot&&)      noexcept;

    // ── Command interface ─────────────────────────────────────────────────────

    /**
     * @brief Sends a single gnuplot command followed by a newline.
     *
     * @param cmd Gnuplot command string (without trailing newline).
     * @throws IoException if the pipe has been closed or writing fails.
     */
    void send(const std::string& cmd);

    /**
     * @brief Syntactic sugar for send().
     *
     * Allows the object to be called like a function, preserving the usage
     * pattern of the original t_gnuplot implementation.
     *
     * @param cmd Gnuplot command string.
     * @throws IoException if the pipe has been closed or writing fails.
     */
    void operator()(const std::string& cmd);

    // ── State ─────────────────────────────────────────────────────────────────

    /**
     * @brief Returns true if the gnuplot pipe is open and ready.
     */
    [[nodiscard]] bool isOpen() const noexcept;

private:

    FILE* m_pipe{nullptr};  ///< Pipe to the gnuplot process.
};

} // namespace loki
