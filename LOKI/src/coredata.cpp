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

#include  "coredata.h"

// constructor
// --------------------
t_coredata::t_coredata(){}

// constructor
// --------------------
t_coredata::t_coredata(const m_dd & origval)
{
  _origval = origval;
  this->_checkAdd("origval");
}

// constructor
// --------------------
t_coredata::t_coredata(const m_td & Origval)
{
  _Origval = Origval;
  this->_CheckAdd("origval");
}


/// @Details
///    -
void t_coredata::addData( const string & datatype, m_dd & data )
{
   
   if      (datatype == "elimTrend")     { _elimTrend    = data; this->_checkAdd(datatype); }
   else if (datatype == "elimSeasonal" ) { _elimSeasonal = data; this->_checkAdd(datatype); }   
   else if (datatype == "elimOutliers" ) { _elimOutliers = data; this->_checkAdd(datatype); }
   else if (datatype == "homogenTS" )    { _homogenTS    = data; this->_checkAdd(datatype); }   
   else if (datatype == "origval" )      { _origval      = data; this->_checkAdd(datatype); }     
   else                                  { /* */                                            }
   
}

/// @Details
///    -
void t_coredata::addData( const string & datatype, m_td & data )
{
  
  if      (datatype == "elimTrend")     { _ElimTrend    = data; this->_CheckAdd(datatype); }
  else if (datatype == "elimSeasonal" ) { _ElimSeasonal = data; this->_CheckAdd(datatype); }   
  else if (datatype == "elimOutliers" ) { _ElimOutliers = data; this->_CheckAdd(datatype); }
  else if (datatype == "homogenTS" )    { _HomogenTS    = data; this->_CheckAdd(datatype); }      
  else if (datatype == "origval" )      { _Origval      = data; this->_CheckAdd(datatype); }        
  else                                  { /* */                                            }
  
}

void t_coredata::_checkAdd(const string & datatype)
{
  
  if(datatype == "origval" && !_origval.empty()) {
    
    LOG1(":...t_coredata[0]::......Original time series is added. Data size: ", _origval.size());
  }
  else if(datatype == "elimTrend" && !_elimTrend.empty()) {
    
    LOG1(":...t_coredata[0]::......De-trended time series is added. Data size: ", _elimTrend.size());
  }
  else if(datatype == "elimSeasonal" && !_elimSeasonal.empty()) {
    
    LOG1(":...t_coredata[0]::......De-seasonalised time series is added. Data size: ", _elimSeasonal.size());
  }
  else if(datatype == "elimOutliers" && !_elimOutliers.empty()) {
    
    LOG1(":...t_coredata[0]::......Time series eliminated by outliers is added. Data size: ", _elimOutliers.size());
  }   
  else if(datatype == "homogenTS" && !_homogenTS.empty()) {
    
    LOG1(":...t_coredata[0]::......Homogenized time series is added. Data size: ", _homogenTS.size());
  }      
  else {
    
    WARN1(":...t_coredata[0]::......Problem with adding the data in coredada obj!");
    return (void) 0;
  }
}

void t_coredata::_CheckAdd(const string & datatype)
{
   
  if(datatype == "origval" && !_Origval.empty()) {
    
    LOG1(":...t_coredata[1]::......Original time series is added. Data size: ", _Origval.size());
  }
  else if(datatype == "elimTrend" && !_ElimTrend.empty()) {
    
    LOG1(":...t_coredata[1]::......De-trended time series is added. Data size: ", _ElimTrend.size());
  }
  else if(datatype == "elimSeasonal" && !_ElimSeasonal.empty()) {
    
    LOG1(":...t_coredata[1]::......De-seasonalised time series is added. Data size: ", _ElimSeasonal.size());
  }
  else if(datatype == "elimOutliers" && !_ElimOutliers.empty()) {
    
    LOG1(":...t_coredata[1]::......Time series eliminated by outliers is added. Data size: ", _ElimOutliers.size());
  }
  else if(datatype == "homogenTS" && !_HomogenTS.empty()) {
    
    LOG1(":...t_coredata[1]::......Homogenized time series is added. Data size: ", _HomogenTS.size());
  }
  else {
    
    WARN1(":...t_coredata[1]::......Problem with adding the data in coredada obj!");
    return (void) 0;
  }
}

/// @Details
///    -
map<double, double> t_coredata::getData( const string & datatype )
{
   
   m_dd data;
   
   if      (datatype == "origval"      ) { data = _origval;      }
   else if (datatype == "elimTrend"    ) { data = _elimTrend;    }   
   else if (datatype == "elimSeasonal" ) { data = _elimSeasonal; }
   else if (datatype == "elimOutliers" ) { data = _elimOutliers; }   
   else if (datatype == "homogenTS"    ) { data = _homogenTS;    }      
   else { /* */ }
   
   return data;
}

/// @Details
///    -
map<string, double> t_coredata::getTimeData( const string & datatype )
{
   
   m_td data;
   
   if      (datatype == "origval"      ) { data = _Origval;      }
   else if (datatype == "elimTrend"    ) { data = _ElimTrend;    }   
   else if (datatype == "elimSeasonal" ) { data = _ElimSeasonal; }
   else if (datatype == "elimOutliers" ) { data = _ElimOutliers; }
   else if (datatype == "homogenTS"    ) { data = _HomogenTS;    }         
   else {  }
   
   return data;
}
