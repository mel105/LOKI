#ifndef APPCHANGE_H
#define APPCHANGE_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Algorithms for change point detection.
 @Details:
  

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

#include "timeStamp.h"
#include "coredata.h"
#include "setting.h"

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

#define _PI 3.141592653589793238462643383279502884197169399375105820974944592307816406286

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;


class t_changePoint
{
   
   typedef map<double, double>  m_dd;
   typedef map<string, double> m_td;
   
 public:
   
   ///   Constructor
   t_changePoint( t_setting* setting,  
                  t_coredata* coredata);
   
   t_changePoint( t_setting* setting,  
                  t_coredata* coredata,
                  int& iBeg,
                  int& iEnd);
   
   ///   Destructor   
   virtual ~t_changePoint(){};
   
   double getCritVal();
   double getACF();
   double getPValue();
   double getMaxTK();
   double getShift();
   
   int getIdxMaxPOS();
   
   string getResult();
   
   map<double, double> getTK();
   
   // get confidence interval.
    
 protected:
   
   void _detectChangePoint();
   void _estimateDependency();
   void _estimateTStatistics(); 
   void _estimateCritValue();
   void _estimateMeans();
   void _estimateShift();
   void _estimateSigmaStarL();
   void _estimatePValue();
   void _testHypothesis();
   
   int    _idxMaxPOS;
   
   double _maxTK;
   double _acf;
   double _mcoef;
   double _prob;
   double _criticalVal;
   double _meanBefore;
   double _meanAfter;
   double _shift;
   double _sigmaStar;
   double _PValue;
   double _limitDependence;

   string _resultOfStationarity;
   
   vector<double> _TK;
   vector<double> _dataVec;
   
   m_dd _data;
};
#endif