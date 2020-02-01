#ifndef SETTING_H
#define SETTING_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Setting
 @Details: Methods read settings from json configuration.
  
 @Reference:
***************************************************************************************************/

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#include <nlohmann/json.hpp>

using namespace std;

class t_setting
{
 public:
   
   t_setting();
   virtual ~t_setting(){};
   
   vector <string> getLoadSetting();
   
   string getOutputHist();   
   string getOutputName();
   string getPlotOnOff();
   string getRegressOnOff();
   string getMedianOnOff();
   string getReferenceOnOff();
   string getStatOnOff();
   string getDetectionOnOff();
   
   double getIqrCnfd();
   double getConfInter();
   double getProbCritVal();
   double getLimitDependence();
   double getInputResolution();
   
   int    getRegModel();
   int    getRegOrder();
   int    getOutMethod();
   int    getConstHour();
   int    getInputDataCol();
   
   bool   getElimTrend();
   bool   getElimSeas();
   bool   getInputConvTdDd();
   bool   getRemSeasModel();
   bool   getRemMedianModel();
   bool   getRemReferenceModel();
   bool   getFixedHour();
   
 protected:
   
   vector <string> _loadSetting;
   
   string _outputName;
   string _outputHist;
   string _statOnOff;
   string _regressOnOff;
   string _medianOnOff;
   string _referenceOnOff;
   string _plotOnOff;
   string _detectionOnOff;
   
   double _iqrCnfd;   
   double _probCritVal;
   double _limitDepedence;
   double _inputResolution;
   
   int    _regModel;
   int    _regOrder;
   int    _constHour;
   int    _inputDataCol;
   
   bool   _elimTrend;
   bool   _elimSeas;
   bool   _inputConvTdDd;
   bool   _fixedHour;
   
};

#endif