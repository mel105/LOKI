#ifndef LOAD_H
#define LOAD_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Method loads data.
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <set>
#include <map>
#include <list>
#include <vector>

#include <string>

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "setting.h"
#include "timeStamp.h"

using namespace std;
   
// ----------
class t_load 
{
   
 public:
   
   // Construnctor(s)
   //t_load(string name, string type );
   //t_load(vector<string> load);
   t_load( t_setting* setting);
   
   // Destructor
   virtual ~t_load(){};
   
   map<double, double> getTestval();
   map<string, double> getTimeTestval();
   vector<string> getDataFiles();
   void setDataFileName(string actualName);
   void readData();
   
 protected:
   
   int _noOfCols();
   
   string _dataFolderPath; 
   string _dataName; 
   string _dataFormat;
   
   int _dataCol;
   bool _convTdDd;
   
   map<double, double> _data;
   map<string, double> _dataTime;
   vector<string> _dataFileNamesVector;
   
   void _doubleFormat();
   void _timeFormat();
   void _checkDataNameString();

   //string _control(const string&, char);
   
};

#endif