/***************************************************************************************************
 LOKI - software development library

 (c) 2025 Michal Elias

 This file is part of the LOKI C++ library.

 This library is free software; you can redistribute it and/or modify it under the terms of the
 GNU General Public License as published by the Free Software Foundation; either version 3 of the
 License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#include "loki/io/gnuplot.hpp"

#include <utility>

using namespace loki;

// ── Construction / destruction ────────────────────────────────────────────────

Gnuplot::Gnuplot()
{
#ifdef _WIN32
    m_pipe = _popen("gnuplot", "w");
#else
    m_pipe = popen("gnuplot", "w");
#endif

    if (!m_pipe) {
        throw IoException(
            "Gnuplot::Gnuplot(): failed to open gnuplot pipe. "
            "Is gnuplot installed and available in PATH?");
    }
}

Gnuplot::~Gnuplot() noexcept
{
    if (m_pipe) {
        // Ask gnuplot to exit cleanly before closing the pipe.
        std::fprintf(m_pipe, "exit\n");
        std::fflush(m_pipe);

#ifdef _WIN32
        _pclose(m_pipe);
#else
        pclose(m_pipe);
#endif
        m_pipe = nullptr;
    }
}

Gnuplot::Gnuplot(Gnuplot&& other) noexcept
    : m_pipe(other.m_pipe)
{
    other.m_pipe = nullptr;
}

Gnuplot& Gnuplot::operator=(Gnuplot&& other) noexcept
{
    if (this != &other) {
        // Release current resource first.
        if (m_pipe) {
            std::fprintf(m_pipe, "exit\n");
            std::fflush(m_pipe);
#ifdef _WIN32
            _pclose(m_pipe);
#else
            pclose(m_pipe);
#endif
        }
        m_pipe       = other.m_pipe;
        other.m_pipe = nullptr;
    }
    return *this;
}

// ── Command interface ─────────────────────────────────────────────────────────

void Gnuplot::send(const std::string& cmd)
{
    if (!m_pipe) {
        throw IoException("Gnuplot::send(): pipe is not open.");
    }

    if (std::fprintf(m_pipe, "%s\n", cmd.c_str()) < 0) {
        throw IoException(
            "Gnuplot::send(): failed to write command to gnuplot pipe: " + cmd);
    }

    std::fflush(m_pipe);
}

void Gnuplot::operator()(const std::string& cmd)
{
    send(cmd);
}

// ── State ─────────────────────────────────────────────────────────────────────

bool Gnuplot::isOpen() const noexcept
{
    return m_pipe != nullptr;
}