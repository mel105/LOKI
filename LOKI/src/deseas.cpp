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

#include "deseas.h"
#include "logger.hpp"

/// Constructor
t_deseas::t_deseas( const map<double, double> & data, const ColumnVector & fit, const double & res)
{
  // @input
  //  - data time series
  //  - fit:  coefficients given as result of LSQ application
  if(!data.empty()) {
    
    _data = data;
    _fit = fit;
    _res = res;
    
    // set period
    this->_setPeriod();
    
    // elim. method
    this->_elimSeas();
  }
  else {
    
    //ERR();
  }
}

// -
map<double, double> t_deseas::getSeas()
{
  // ak nie je prazdna, posli, inak err
  return _seas;
}

map<double, double> t_deseas::getDeseas()
{
  // ak nie je prazdna, posli, inak err
  return _deseas;
}

// -
void t_deseas::_setPeriod()
{        
  
  if(_res <= 0.0 || _res == 86400.0) {
    
    _per = 365.25;
  }
  else {
    
    double rat = 86400.0 / _res;
    _per = 365.25 * rat;
  }
}


// -
void t_deseas::_elimSeas()
{
  
#ifdef DEBUG
  cout << _fit << "  " << _data.size() << endl;
#endif
  
  size_t N = _data.size();
  
  // y = A + B*t + C*sin(2*M_PI*t/P + Phi): P = fixed = _per
  for ( map<double, double>:: iterator i = _data.begin(); i != _data.end(); i++) {
    
    double temp = _fit(1) + _fit(2) * i->first;
    
    for( int j = 3; j <= _fit.Nrows(); j=j+2) {
      
      temp +=  _fit(j) * sin((j-1) * M_PI * i->first / _per + _fit(j+1));
    }
    
    
#ifdef DEBUG
    cout
       << fixed << setprecision(4)
       << " TIM:    " << i->first
       << " VAL:    " << i->second
       << " Seas:   " << temp
       << " Deseas: " << i->second - temp
       << endl;
#endif
    
    _seas[i->first]   = temp;
    _deseas[i->first] = i->second - temp;
  }
}