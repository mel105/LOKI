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

#include "adjustSeries.h"
#include "logger.hpp"
#include "plot.h"
#include "stat.h"

using namespace NEWMAT;

// contructor
// -----------
t_adjustSeries::t_adjustSeries( t_coredata * coredata, t_changePoint * changePoint)
{
  
  /// Get homogenized data
  _data = coredata->getData("homogenTS");  
   
  /// Get suspected change point position index
  _idxMax = changePoint->getIdxMaxPOS();
  
  LOG1(":......t_adjustSeries::......Called adjusting method!");
  
  if(!_data.empty()){
    
    LOG1(":......t_adjustSeries::......Data size: ", _data.size());
    
    for(m_dd::iterator i = _data.begin(); i != _data.end(); i++) {
      
      _dataVec.push_back( i->second );
      
    }
  }
  else{
    
    ERR(":......t_adjustSeries::......Data container is empty!"); return;
    
  }
  
  /// Estimate shift
  this->_estimateShift();
  
  /// Adjust processed time series
  this->_adjustSeries();

  /// Add adjusted time series into the coredata
  coredata->addData("homogenTS", _adjusted);
}

// contructor
// -----------
t_adjustSeries::t_adjustSeries( t_coredata* coredata, 
                                const int& suspectedChpIdx, 
                                const double& shift, 
                                const string& dataString )
{
  
  /// Get data 
  _data = coredata->getData(dataString);  
  
  LOG1(":......t_adjustSeries::......Called adjusting method [",dataString,"]!");
  
  if(!_data.empty()){
    
    LOG1(":......t_adjustSeries::......Data size: ", _data.size());
    
    for(m_dd::iterator i = _data.begin(); i != _data.end(); i++) {
      
      _dataVec.push_back( i->second );
    }
  }
  else{
    
    ERR(":......t_adjustSeries::......Data container is empty!"); return;
  }
  
  
  /// Adjust time series
  this->_adjustSeries(suspectedChpIdx, shift);
  
  /// Add adjusted time series into the coredata
  coredata->addData(dataString, _adjusted);
}

// Public function that returns adjusted series. The series is also added into the coredata.
map<double, double> t_adjustSeries::getAdjustedSeries() 
{
  if ( !_adjusted.empty() ) {
	
    return _adjusted;
  }
  else {
    
    ERR(":......t_adjustedSeries::......Unsuccessful process of time series homogenization");
  }
}
 
/// Method processes the data
void t_adjustSeries::_adjustSeries( const int& epoIdx, const double& shift )
{

  int idx = 0;
  for (map<double, double>::iterator it = _data.begin(); it != _data.end(); ++it) {

    if (idx >= epoIdx)	{
      
      _adjusted[it->first] = it->second - shift;
    }
    else {
      
      _adjusted[it->first] = it->second;
    }
    
    idx ++;
  }
   
}

/// Method processes the data
void t_adjustSeries::_adjustSeries()
{
  
  int idxCounter = 0; 
  
  for(m_dd::iterator I = _data.begin(); I != _data.end(); I++) {

    if ( idxCounter >= _idxMax ) {
      
      _adjusted[I->first] = I->second - _shift;
    }
    else {
      
      _adjusted[I->first] = I->second;
    }
    
    idxCounter++;
  }
}

/// Method estimates the shift
void t_adjustSeries::_estimateShift()
{
  
  vector<double> data_to_k;
  vector<double> data_af_k;
  
  for(unsigned int i = 0; i < _idxMax-1; i++) {
    
    data_to_k.push_back(_dataVec[i]);
  }
  
  for(unsigned int j = _idxMax; j < _dataVec.size(); j++) {
    
    data_af_k.push_back(_dataVec[j]);
  }
  
  t_stat before(data_to_k); before.calcMean(); double meanBefore = before.getMean();
  t_stat  after(data_af_k);  after.calcMean(); double meanAfter  = after.getMean();
  
  _shift = (meanAfter - meanBefore);
}