#ifndef APPHOMGEN_H
#define APPHOMOGEN_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2019-06-14 22:43:00 Author: Michal Elias  

 @Brief: Class contains methods used for change point detection 
 @Details:
   Process contains:
     - If requested: Time series De-seasonalisation. 
       Implemented methods:
         1. seasonal regression, 
         2. median year series, 
         3. if exist, then by reference time series TBD.
     - Change point detection. 
     - Time series adjustment (Change point elimination)
 
    Change point detection steps:
      1. Use deseasonalised time series.
      2. Detect suspected epoch of change point in des. t.s.
      3. Test H0 hypothesis and decide the stationrity.
      4. If the t.s. is stationary -> end the process. If the t.s. is non-stationary, then make
         the verification of the suspected epoch.
      5. Create/add list of epochs
      6. Adjust deseasonalised t.s. and repeat steps 2-5.
      7. TBD: Adjust original t.s. depending on created list of suspected epochs.
 
 @References:
   [1] Antoch et al.: Effect of dependence on statistics for determination of change. In: Journal
                      of Statistical Planning and Inference. Vol. 60, pp. 291-310, 1997.
   [2] Antoch et al.: Off-Line Statistical Process Control. In: Lauro C., Antoch J., Vinzi V.E.,
                      Saporta G. (eds) Multivariate Total Quality Control. Contributions to 
                      Statistics. Physica-Verlag HD, 2002.
   [3] Csorgo and Horvath: Limit Theorems of Change-Point Analysis, Wiley, 1997.
   [4] Elias et al.: AN ASSESSMENT OF METHOD FOR CHANGE-POINT DETECTION APPLIED IN TROPOSPHERIC
                     PARAMETER TIME-SERIES GIVEN FROM NUMERICAL WEATHER MODEL. Submitted in X (2019)
   [5] Jaruskova: Change-point detection in meteorological measurement In: Monthly Weather Review, 
                  Vol. 124, No. 7, pp. 1535-1543, 1996.
   [6] Jaruskova: Some Problems with Application of Change-Point Detection Methods to Environmental 
                  Dta In: Environmetrics, Vol. 8, No. 5, pp. 469-483, 1997.
   [7] Yao and Davis: The Asymptotic Behavior of the Likelihood Ratio Statistic for Testing a Shift 
                      in Mean in a Sequence of Independent Normal Variates. In: Sankhya: The Indian 
                      Journal of Statistics, Series A (1961-2002), 48(3), 339-353, 1986.
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

#define CONF_INTER_DEF 2.5

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;

class t_appDetection
{
   
   typedef map<double, double> m_dd;
   typedef map<string, double> m_td;
   typedef map<int, int> m_ii;
   
  public:

   ///   Constructor
   t_appDetection( t_setting  * setting,  
		  t_coredata * coredata);
   
   ///   Destructor   
   virtual ~t_appDetection(){};
    
 protected:
   
   //   int _addTimeSeries();
   //bool _verifyChangePointRepeatability();
   //void _verifiedProtocol();
   //void _nonVerifiedProtocol();
   
   int _prepareData();
   int _plotTimeSeries();
   int _processChangePointDetection();
   int _checkDeseasData();
   map<int, int> _prepareIntervals( vector<int>& idxVec );
   
   //vector<string> _timeData;
   vector<t_timeStamp> _timeData;
   
   t_coredata* _coredata;
   t_setting* _setting;
   
   m_td _Data;
      
   m_dd _data;
   m_dd _TK;
   m_dd _deseas;
   
   int _N; // number of original data
   int _counter;
   int _suspectedChangeIdx;
   
   double _CV;
   double _res;
   double _prob;
   double _suspectChp;
   double _shift;
   double _maxTk;
   double _acf;
   double _pVal;
   
   bool _conv;
   bool _verifiedChP;
   
   string _fmt;       // data format: dd, td
   string _out;
   string _gnp;   
   string _plot; 
   string _hst;
   string _result;
   string _regressOnOff;
   string _medianOnOff;
   string _referenceOnOff;
   string _adjustString;
   
   vector<double> _containerOfChps;
   vector<double> _containerOfSfts;
};
#endif