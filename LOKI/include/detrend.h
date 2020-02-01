#ifndef DETREND_H
#define DETREND_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Method provides time series that is cleaned by the estimated trend.
 @Details: It is valid only for the regression.

 @Reference:
***************************************************************************************************/

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;

class t_detrend
{
   
   typedef map<double, double> m_dd;
   
 public:
   
   t_detrend(const map<double, double> & data,  const double & a, const double & b);
   virtual ~t_detrend(){};
   
   map<double, double> getTrend();
   map<double, double> getDetrend();
   
 protected:
   
   void _elimTrend();
   
   double _a;
   double _b;
   
   m_dd _data;
   m_dd _trend;
   m_dd _detrend;
   
};
#endif
