#ifndef APPREGRESS_H
#define APPREGRESS_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Provides regression analysis
 @Details:
   Process includes:
     - Linear regression calculation;
     - Polynomial regression calculation;
     - Cosine-Harmonics regression calculation.  
 
 @Reference:
   [1] Bloomfield P.: Fourier Analysis of time series (2000) 
***************************************************************************************************/

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "timeStamp.h"
#include "coredata.h"
#include "setting.h"

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

#define ORDER_DEF 2

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;



class t_appRegress
{
   
   typedef map<double, double>  m_dd;
   typedef map<string, double> m_td;
   
  public:

   ///   Constructor
   t_appRegress( t_setting  * setting,  
		 t_coredata * coredata);
   ///   Destructor   
   virtual ~t_appRegress(){};
   
   /// @detail

   // void procRegress(const string & fmt);
   
  protected:
   
   t_coredata * _coredata;
   
   m_dd _ts_trend;
   m_dd _ts_detrend;
   m_dd _ts_seas;
   m_dd _ts_deseas;
   m_dd _data;
   m_td _Data;
   
   int _prepareData();
   int _procRegress();
   int _eliminateRegressionModel();
   int _addTimeSeries();
   int _plotTimeSeries();
   
   int    _regOrder;
   int    _regModel; // regression model: 1 = linear, 2 = polynomial, 3 = harmonics
   
   bool   _elimTrend;
   bool   _elimSeas;
   bool   _conv;
   
   double _res;
   
   string _fmt;      // data format: dd, td
   string _out;
   string _gnp;   
   string _plot; 
   string _hst;
   ColumnVector _est;
   
};
#endif