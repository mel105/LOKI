/************************************************************************************************** 
 PROHO - software development library
  
 (c) 2019 Michal Elias
  
 This file is part of the PROHO C++ library.
  
 This library is free software; you can redistribute it and/or  modify it under the terms of the GNU
 General Public License as  published by the Free Software Foundation; either version 3 of the 
 License, or (at your option) any later version.
  
 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.
  
 You should have received a copy of the GNU General Public License  along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#include "detrend.h"
#include "logger.hpp"

/// Constructor
t_detrend::t_detrend( const map<double, double> & data, const double & a, const double & b)
{
   // @input
   //  - data: time series
   //  - a, b coefficients given as result of LSQ application
   if(!data.empty()) {
      
      _data = data;
      _a    = a;
      _b    = b;
   
      // trned elim. method
      this->_elimTrend();
   }
   else {
      
      //ERR();
   }
}

// -
map<double, double> t_detrend::getTrend()
{
   // ak niej e prazdna, posli, inak err
   return _trend;
}

map<double, double> t_detrend::getDetrend()
{
   // ak niej e prazdna, posli, inak err
   return _detrend;
}


// -
void t_detrend::_elimTrend()
{
   
#ifdef DEBUG
   cout << _a << "  " << _b << "  " << _data.size() << endl;
#endif
   
   for (m_dd:: iterator J = _data.begin(); J != _data.end(); ++J) {
            
#ifdef DEBUG
      cout
	<< fixed << setprecision(4)
	<< " TIM: " << J->first
	<< " VAL: " << J->second
	<< " TRD: " << _a + _b * J->second
	<< " ELM: " << _a + (J->second - (_a + _b * J->second))
	<< endl;
#endif
      
      _trend[J->first]   = ( _a + _b * J->first);
      _detrend[J->first] = ( J->second - (_a + _b * J->first));
   }
}