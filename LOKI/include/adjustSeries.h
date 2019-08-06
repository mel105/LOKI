#ifndef ADJUST_H
#define ADJUST_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Method returns adjusted series. 
 @Details: -
  
 @Reference: -
***************************************************************************************************/

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "coredata.h"
#include "changePoint.h"

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;

class t_adjustSeries
{
   
   typedef map<double, double>  m_dd;
   
 public:
   
   ///@Constructor
   t_adjustSeries( t_coredata * coredata, t_changePoint * changePoint );
   t_adjustSeries( t_coredata * coredata, const int& suspectedChpIdx, const double& shift, const string& dataString );
   
   ///@Destructor   
   virtual ~t_adjustSeries(){};
   
   /// Get adjusted series
   map<double, double> getAdjustedSeries();
   
 protected:
   
   /// Estimates the shift
   void _estimateShift();
   
   /// Adjust time series
   void _adjustSeries();
   
   /// Adjust time series
   void _adjustSeries( const int& epoIdx, const double& shift );
   
   /// Processed data container
   m_dd _data;
   
   /// Adjusted data container
   m_dd _adjusted;    
   
   /// Processed data vector
   vector<double> _dataVec;
   
   /// Index of suspected change point epoch
   int _idxMax;
   
   /// Shift given from changePoint method
   double _shift;
   
};
#endif