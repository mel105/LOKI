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

#include <stdlib.h>
#include <iostream>
#include <iomanip>

#include "logger.hpp"
#include "load.h"
#include "workingdir.h"

using namespace std;
  
// constructor
// --------------------
//t_load::t_load(
//	       string dataName,
//	       string dataFormat
//	      )

t_load::t_load( t_setting* setting )
{
   
   // get actual working dir
   t_workingDir workingDir; 
   string wDir = workingDir.GetCurrentWorkingDir();
   
   vector <string> loadSetting = setting->getLoadSetting();
   bool convTdDd = setting->getInputConvTdDd();
  
   _dataName = loadSetting[0];
   _dataFormat = loadSetting[1];
   _dataFolderPath = (wDir+loadSetting[2]);
   _convTdDd = convTdDd;
   
   if (_dataFormat == "d") {
    
      // predpoklada sa, ze data format tvori len jeden stlpec
   }
   else if (_dataFormat == "dd") {
      
      // key: int    val: double
      // key: double val: double
      this->_twoColFormat();
   }
   else if (_dataFormat == "td") {
      
      // key: t_gtime val: double
      this->_timeFormat();
   }
   else {
      
      ERR(":......t_load::......Requested input data format is not supported!");
   }
}

map<double, double> t_load::getTestval(){ return _data;}

map<string, double> t_load::getTimeTestval(){ return _dataTime;}


// -
void t_load::_timeFormat()  
{
   
  _dataTime.clear();

  ifstream dir((_dataFolderPath+"/"+_dataName).c_str());
  
  string ymd, hms, line;
  double vals;
  
  if(dir){
    
    while ( getline(dir, line) ){
      
      istringstream istr(line);
      istr >> ymd >> hms >> vals;
      
      t_timeStamp epoch(ymd+" "+hms);
      
#ifdef DEBUG
      cout << ymd+" "+hms << "  " << epoch.mjd() << "  " << vals << endl;
#endif
      
      if (_convTdDd){
        
        _dataTime[ymd+" "+hms] = vals; // string = yyyy-mm-dd hh:mn:sc, double
      }
      else {
        
        _data[epoch.mjd()] = vals; // double = mjd, double
      }
    }
  }
}

// -
void t_load::_twoColFormat()
{  
  _data.clear();
  
  string line;
  
  ifstream dir((_dataFolderPath+"/"+_dataName).c_str());
  
  while ( getline(dir, line) ) {
    
    double timeKey, vals;
    
    istringstream istr(line);
    
    istr >> timeKey >> vals;
    
    _data[static_cast<double>(timeKey)] = vals;
  }
}