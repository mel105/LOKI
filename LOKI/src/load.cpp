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

#include <stdlib.h>
#include <iostream>
#include <iomanip>

#include "logger.hpp"
#include "load.h"
#include "workingdir.h"
//#include <boost/algorithm/string.hpp>   

using namespace std;
  
// constructor
// --------------------
t_load::t_load( t_setting* setting )
{
   
  // get actual working dir
  t_workingDir workingDir; 
  string wDir = workingDir.GetCurrentWorkingDir();
  
  vector <string> loadSetting = setting->getLoadSetting();
  bool convTdDd = setting->getInputConvTdDd();
  
  _dataName = loadSetting[0];
  _dataFolderPath = (wDir+loadSetting[1]);
  _dataFormat = loadSetting[2];
  _convTdDd = convTdDd;
  _dataCol = setting->getInputDataCol();

  // .check data file name and return file names vector
  this->_checkDataNameString();
}


// .public functions
void t_load::setDataFileName(string actualName){

   _dataName = actualName;
}

// -
void t_load::readData(){
   
  if (_dataFormat == "d") {
    
    // will open the file with only the one column
  }
   else if (_dataFormat == "dd") {
      
    // key: int    val: double
    // key: double val: double
    this->_doubleFormat();
  }
  else if (_dataFormat == "td") {
    
    // key: t_timeStamp val: double
    this->_timeFormat();
  }
  else {
    
    ERR(":......t_load::......Requested input data format is not supported!");
  }
    
}

// -
map<double, double> t_load::getTestval() {
  
  return _data;
}

map<string, double> t_load::getTimeTestval() {
  
  return _dataTime;
}

vector<string> t_load::getDataFiles() {
   
  return _dataFileNamesVector;
}


// Protected functions
// -
void t_load::_checkDataNameString()
{

   replace(_dataName.begin(), _dataName.end(), ';', ' ');  // ';' replaced by ' '
   
   stringstream ss(_dataName);
   string temp;
   while (ss >> temp) {

       _dataFileNamesVector.push_back(temp);
   }
        
//   cout << _dataFileNamesVector.size() << endl;
}

// the main poit of the method is to check, if string contains symbol ". If so, then remove the symbol.
/*
string t_load::_control(const string& str, char mSym)
{

   return str.erase(remove(str.begin(), str.end(), '"'), str.end());
}
*/

// -
void t_load::_timeFormat()  
{
   
  _dataTime.clear();

  ifstream dir((_dataFolderPath+"/"+_dataName).c_str());
  
  string ymd, hms, line;
  double vals, sigma;
  
  if(dir){
    
    while ( getline(dir, line) ){
      
      istringstream istr(line);
      istr >> ymd >> hms >> vals >> sigma;
      
      t_timeStamp epoch(ymd+" "+hms);
      
#ifdef DEBUG
      cout << ymd+" <-ymd vs hms-> "+hms << " nasleduje mjd " << epoch.mjd() << " vals  " << vals << " a sigma " << sigma<< endl;
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

// Loads the data file, when the time stamp is given as double.
void t_load::_doubleFormat()
{  

  // No of cols in input data file
  int nCols = this->_noOfCols();
  
  ifstream dir((_dataFolderPath+"/"+_dataName).c_str());
  
#ifdef DEBUG
  
  cout << "nCols = " << nCols << endl;;
#endif
 
  // If req. col is > than nCols, then set default col.
  if (nCols <= _dataCol) {
    
     _dataCol = 1;
  }
  
  // Load the data!
  _data.clear();
  
  string line;
  
  while ( getline(dir, line) ) {
    
    istringstream iss (line);
    
    double timeKey;  iss >> timeKey;
    
    int idx = 1;
    
    for (int iCol = 1; iCol < nCols; iCol++) {
    
      double val;  iss >> val;
    
      if (idx == _dataCol) {
        
        _data[static_cast<double>(timeKey)] = val;        
      } // if
      
      idx ++;
    } // for
  } // while
  
#ifdef DEBUG
  for(map<double, double>::iterator i = _data.begin(); i!=_data.end(); ++i) {
    
     cout << fixed << setprecision(0) <<  i->first << "  " << setprecision(1) <<  i->second << endl;
  }
#endif

} // void
  
// -
int t_load::_noOfCols() 
{

  ifstream dir((_dataFolderPath+"/"+_dataName).c_str());
  
  string mLine, tLine;
  stringstream sStr;
  
  int nCols = 0;
    
  getline(dir, mLine);
  sStr.clear();
  sStr << mLine;

  while (sStr >> tLine) {
    
    nCols++;
  }
  
#ifdef DEBUG
  
  cout << "nCols = " << nCols << endl;;
#endif
 
  return nCols;
}

