#include <loki/gnss/broadcastOrbit.hpp>
#include <loki/gnss/keplerOrbit.hpp>

using namespace loki::gnss;

SatState BroadcastOrbit::compute(const NavFile&  nav,
                                   GnssSystem      system,
                                   int             prn,
                                   const GpsTime&  t) const
{
    return KeplerOrbit::compute(nav, system, prn, t);
}
