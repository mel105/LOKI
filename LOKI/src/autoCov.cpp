/************************************************************************************************** 
 LOKI - software development library
  
 (c) 2019 Michal Elias
  
 This file is part of the LOKI C++ library.
  
 This library is free software; you can redistribute it and/or  modify it under the terms of the GNU
 General Public License as  published by the Free Software Foundation; either version 3 of the 
 License, or (at your option) any later version.
  
 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.
  
 You should have received a copy of the GNU General Public License  along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#include "autoCov.h"
#include "logger.hpp"
#include "plot.h"
#include "stat.h"

using namespace NEWMAT;

// contructor
// -----------
t_autoCov::t_autoCov( const vector <double> & data)
{

   LOG1(":......t_autoCov::......Called autocovariance method!");

   if(!data.empty()){

      _dataVec = data;
      LOG1(":......t_autoCov::......Data size: ", _dataVec.size());
   }
   else{

      ERR(":......t_autoCov::......Data container is empty!"); return;
   }

   // process change point detection method
   this->_autoCov();
}

// contructor
// -----------
t_autoCov::t_autoCov( const map<double, double>& data)
{

   LOG1(":......t_autoCov::......Called autocovariance method!");

   if(!data.empty()){

      for(m_dd::const_iterator i = data.begin(); i != data.end(); i++) {
	 _dataVec.push_back(i->second);
      }

      LOG1(":......t_autoCov::......Data size: ", _dataVec.size());
   }
   else{

      ERR(":......t_autoCov::......Data container is empty!"); return;
   }

   // process change point detection method
   this->_autoCov();
}

// -
vector <double> t_autoCov::getAutoCov()
{
   if( !_xcov.empty() ) {

      LOG1(":......t_autoCov::......Get xcov: ", _xcov.size());

      return _xcov;
   }
   else{

      ERR(":......t_autoCov::......xcov vector is empty!");
   }
}


/// @Details
///   -
void t_autoCov::_autoCov()
{
   int n = _dataVec.size();

   vector<double> C;

   // get mean
   t_stat stat(_dataVec); stat.calcMean(); double mean = stat.getMean();

   // get C
   for(int k = 0; k < n; ++k) {

      double suma = 0.000;
      for(int t = 1; t < n; ++t) {

	 if( (t + k) < n ) {

	    suma += ((_dataVec[t] - mean) * (_dataVec[t + k] - mean)) / n;
	 }
      }

      C.push_back(suma);
   }


#ifdef DEBUG
   cout << "C[ 0 ] " << C[0] << " RHO[ 0 ]  " << C[0]/C[0] << endl;
#endif

   // get xcov
   for(unsigned int k = 0; k<C.size(); ++k) {

      _xcov.push_back(C[k] / C[0]);

#ifdef DEBUG
      cout << "C[ " << k <<" ] " << C[k] << " RHO[ " << k <<" ]  " << C[k]/C[0] << endl;
#endif
   }

   C.clear();
}
