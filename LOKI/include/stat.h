#ifndef STAT_H
#define STAT_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Basic statistics calculation
 @Details: Apart from basic statistics calculation, class also contains method for quantiles 
           estimation.
  
 @Reference:
***************************************************************************************************/

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#include <nlohmann/json.hpp>

#define IQR_CNFD_DEF 2.0

using namespace std;

class t_stat
{
 public:
   
   t_stat(const vector<double> & data);
   t_stat(const vector<double> & data, const double & iqrCnfd);
   virtual ~t_stat(){};
   
   void calcMean();
   void calcMode();
   void calcMedian();
   void calc1Qt();
   void calc3Qt();
   void calcIQR();
   void calcSdev();
   void calcVare();
   void calcMinMax();
   double calcQuantile(const double reqQ);
   
   double getMean();
   double getMedian();
   double get1Qt();
   double get3Qt();
   double getIQR();
   double getUPP();
   double getLOW();
   double getSdev();
   double getVare();
   double getMin();
   double getMax();
   double getModeFreq();
   vector<double> getMode();
   
 protected:
   
   void _setData( const vector<double> & data);
   void _calcMean();
   void _calcMode();
   void _calcMedian();
   void _calc1Qt();
   void _calc3Qt();
   void _calcIQR();
   void _calcSdev();
   void _calcVare();
   void _calcMinMax();
   void _calcQuantile(const double reqQ);
   
   vector<double> _data;
   vector<double> _minmax;
   
   double _iqrCnfd;
   double _mean;
   double _median;
   double _1Qt;
   double _3Qt;
   double _iqr;
   double _upp;
   double _low;
   double _sdev;
   double _vare;
   double _min;
   double _max;
   double _quantile;
   double _modeF;
   vector<double> _mode;
   
};

#endif