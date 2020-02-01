#ifndef APPMEDIAN_H
#define APPMEDIAN_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Class contains methods to median time series calculation 
 @Details:
   Process of calculation follows:
     - median year calculation
     - deseasonalised time series calculation

 @References:
   [1] Elias M., et al. AN ASSESSMENT OF METHOD FOR CHANGE-POINT DETECTION APPLIED IN TROPOSPHERIC
                        PARAMETER TIME-SERIES GIVEN  * FROM NUMERICAL WEATHER MODEL. Submitted
                        in XXX 2019
   [2] Rienzner M. and Gandolfi C. A procedure for the detection of undocumented multiple abrupt
                                   changes in the mean value of daily temperature time-series of a 
                                   regional network. In: International Journal of Climatology, Vol.
                                   33, No. 5, pp. 1107-1120, 2013.
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
#include "stat.h"

#include "coredata.h"
#include "setting.h"

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;

class t_appMedian
{
   
   typedef map<double, double>  m_dd;
   typedef map<string, double> m_td;
   
  public:

   ///   Constructor
   t_appMedian( t_setting  * setting,  
                t_coredata * coredata);
   ///   Destructor   
   virtual ~t_appMedian(){};
   
  protected:
   
   void _calculateMedianTimeSeries();
   void _transTimeToMJD();
   void _produceMedianYearSeries();
   void _plotTimeSeries();
   int  _getHourResolution();
   int  _addCoreData();
   int  _prepareDeseasonalisedSeries();
   int  _findEpoInInterestSeries(t_timeStamp EPO, int& mMon, int& mDay, int& mHour);
   
   vector<double> _toMedianStat;
   vector<string> _timeVec;
   
   t_coredata * _coredata;
   t_setting  * _setting;
   
   map< int, map<int, map<int, double>>> _MEDIAN;

   m_dd _MEDIAN_SERIES;
   m_dd _data;
   m_dd _dataDeseas;
   m_td _Data;
   m_td _DataDeseas;
   
   int _BEGY;
   int _ENDY;
   int _constHour;
   
   double _tsResolution;
   double _hourRes;
   
   bool _fixedHour;
   
   string _begy;
   string _endy;
   string _fmt;       // data format: dd, td
   string _out;
   string _gnp;   
   string _plot; 
   string _hst;
   
};
#endif