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

#include "appRegress.h"
#include "convTDtoDD.h"
#include "detrend.h"
#include "deseas.h"
#include "fit.h"
#include "logger.hpp"
#include "plot.h"
#include "stat.h"
#include "workingdir.h"

using namespace NEWMAT;

// contructor
// -----------
t_appRegress::t_appRegress(t_setting* setting,  t_coredata* coredata)
{
  _coredata = coredata;
   
  /// Get setting & create log info
  vector<string> inpSett = setting->getLoadSetting(); _fmt = inpSett[2];
  string outputName = setting->getOutputName(); _out = outputName;
  string outputHist = setting->getOutputHist(); _hst = outputHist;   
  string plotOnOff = setting->getPlotOnOff(); _plot = plotOnOff;
  bool elimTrend = setting->getElimTrend(); _elimTrend = elimTrend;
  bool elimSeas = setting->getElimSeas(); _elimSeas = elimSeas;
  int regModel = setting->getRegModel(); _regModel = regModel;
  int regOrder = setting->getRegOrder(); _regOrder = regOrder;
  bool conv = setting->getInputConvTdDd(); _conv = conv;
  double res = setting->getInputResolution(); _res = res;
  
  /// Get actual working folder path
  t_workingDir workingDir; _gnp = workingDir.GetCurrentWorkingDir();
  
  LOG1(":...t_appRegress::......Loaded appRegress settings!");
  
  /// Get data from coredata & create log info.
  /// TODO> Ale s akymi datami chcem pracovat. Zatial default original. Ale kludne mozem s datami ocestene o outliers, skok a podobne.
  
  if (_fmt == "dd") {
    
    _data = coredata -> getData("origval");
    if(!_data.empty()){
      
      LOG1(":...t_appRegress::......Loaded data container. Data size: ", _data.size());
    }
    else{
      
      ERR(":...t_appStat::......Data container is empty!"); return;
    }
    
  }
  else {
    
    _Data = coredata -> getTimeData("origval");
    if(!_Data.empty()) {
      
      LOG1(":...t_appRegress::......Loaded data container. Data size: ", _Data.size());
    }
    else {
      
      ERR(":...t_appStat::......Data container is empty!"); return;
    }
  }
  
  // Prepare data
  this->_prepareData();
  
  // Call application
  this->_procRegress();
  
  // Eliminate regression from the input ts
  this->_eliminateRegressionModel();
  
  // Add eliminated ts into the coredata
  this->_addTimeSeries();
  
  // If request -> plot data
  if(_plot == "on") this->_plotTimeSeries();
  
}

/// @Details
///   - Function prepares data for further processing
int t_appRegress::_prepareData()
{
  
  if (_fmt == "dd") {
    
    //data::double, double
    _data = _coredata -> getData("origval");
    
    if(!_data.empty()) {
      
      LOG1(":...t_appRegress::......Loaded data container. Data size: ", _data.size());
    }
    else{
      
      ERR(":...t_appRegress::......Data container is empty!"); return -1;
    }
  }
  else if (_fmt == "td" && _conv == true ) {
    
    //data::string, double
    _Data = _coredata -> getTimeData("origval");
    
    // convert data from string, double to the double, double
    t_fromTDtoDD fromTDtoDD;  _data = fromTDtoDD.FromTDtoDD(_Data);
    
    if(!_data.empty()) {
            
      LOG1(":...t_appRegress::......Loaded data container. Data size: ", _data.size());
            
    }
    else{
      
      ERR(":...t_appRegress::......Data container is empty!"); return -1;
    }
  }
  else if (_fmt == "td" && _conv == false ) {
    
    //data::double, double
    _data = _coredata -> getData("origval");
    
    if(!_data.empty()) {
      
      LOG1(":...t_appStat::......Loaded data container. Data size: ", _data.size());
      
    }
    else{
      
      ERR(":...t_appStat::......Data container is empty!"); return -1;
    }
  }
  
  return 0;
}

/// @Details
///   -
int t_appRegress::_procRegress()
{
   
  /// Output
  ofstream hfile(_hst.c_str(), ios::out | ios::app);
  ofstream ofile(_out.c_str(), ios::out | ios::app);
  ofile << "\n# ********************************************* \n";
  ofile << "#                   Regression                  \n";
  ofile << "# ********************************************* \n\n";   
  
  // call regression process
  t_fit * fit = 0;
  
  switch (_regModel) {
    
  case 1:
    
    LOG1(":...t_appRegress::......Request for Linear regression model!");
    if (_regOrder != 1) {
      
      _regOrder = 1; 
    }
    
    break;
    
  case 2:
      
    LOG1(":...t_appRegress::......Request for Polynomial regression model!");
    if ( _regOrder <= 0 || _regOrder >= 10 ) {
      
      ERR(":...t_appRegress::......Degree of a polynomial is allowed in rage of <1:10>!");
    }
    
    break;
    
  case 3:
    
    LOG1(":...t_appRegress::......Request for Harmonic regression model!");
    
    break;
      
  default:
    
    ERR(":...t_appRegress::......Requested model is not supported!");
    
    return -1;
    break;
  }
  
  fit = new t_fit(_regModel, _data, _regOrder, _res);
   
  hfile << "TODO::t_appRegress: ked mam _regOrder v harmonickom modele 1+1, ako to, ze mi funguje _est = 4? A mozno to nefunguje. Over!!!!!" << endl;
  ColumnVector est(_regOrder+1); est = fit->getCoef(); _est = est;
  
   if ( _est(1) != 0.0 ) { // should be non-zero
     
     LOG1(":...t_appRegress::......Regression done!");
   }
  
  /// Results of regression model fitting
  double tal = fit->getTAlpha();
  double sse = fit->getSSE();
  double rss = fit->getRSS();
  double cd = fit->getCD();
  double rv = fit->getSR();
  vector<double> sigma = fit->getSigmaFit();
  vector<double> low = fit->getConfIntLow();
  vector<double> upp = fit->getConfIntUpp();
  vector<double> tstat = fit->getTStat();
  vector<double> pvals = fit->getPVals();
  
  size_t N = _data.size();
  
  hfile << "TODO::appRegress::Nepozdavaju sa mi statistiky v pripade cd - Over si to podla diplomovky!!!" << endl;
  
  ofile << fixed << setprecision(5)
     << "\n# Model:                        "       << _regModel
     << "\n# Order:                        "       << _regOrder
     << "\n# Parameters to estimation:     "       << _regOrder+1 // ako je to s harmonickym model, to este premysli   
     << "\n# N:                            "       << N
     << "\n# Sum square error:             "       << sse
     << "\n# Residuals sum square:         "       << rss
     << "\n# Coefficient of determination: "       << cd * 100.0 << "[%]"
     << "\n# Residual variance:            "       << rv
     << "\n# Critical value [t(alpha)]:    "       << tal
     << "\n# ------------------------------------------------------------------------------------------------------------ "
     << "\n#        Coefficient   | Standard deviation |  Confidence Interval Estimation  |   t-Statistic   |    P-value  "
     << "\n#         Estimation   |  Error Estimation  |       Lower 95%     Upper 95%    |                 |             "
     << "\n# ------------------------------------------------------------------------------------------------------------ ";
  for ( int i = 0; i < sigma.size(); i++) {
    
    ofile  
       << "\n#   A("<< i+1 << ") " 
       <<setw(11)<< est(i+1) << "      "
       <<setw(11)<< sigma[i] << "             "
       <<setw(11)<< low[i]   << "    " 
       <<setw(11)<< upp[i]   << "        " 
       <<setw(11)<< tstat[i] << "   " 
       <<setw(11)<< pvals[i];
  }
  ofile << "\n# ------------------------------------------------------------------------------------------------------------ " << endl;
  
  /// Notes what to do?
  hfile << "[2019-03-29] [mel] -> [regress] Added t_appRegress; added appRegress obj.\n";
  hfile << "[2019-03-30] [mel] -> [regress] td format will be converted into the dd format.\n";
  hfile << "[2019-03-31] [mel] -> [regress] Plotting the dd format ( and td converted into the dd).\n";
  hfile << "[2019-03-31] [mel] -> [regress] Linear/polynomial regression method implemented.\n";
  hfile << "[2019-03-31] [mel] -> [regress] Data eliminated by regression model and added into the coredata obj.\n";
  hfile << "[2019-04-08] [mel] -> [regress] Harmonic regression model implemented.\n";
  hfile << "[2019-05-03] [mel] -> [regress] Cleaned input data formats.\n";
  
  /// Close/Delete
  if ( fit ) delete fit;
  
  return 0;
}

/// @Details
///   The main method's idea is to eliminate the regression model from the input data.
int t_appRegress::_eliminateRegressionModel()
{
  
  if(_elimTrend) {
    
    // linear trend constants
    double a = _est(1);
    double b = _est(2);
    
    t_detrend * detrend = new t_detrend(_data, a, b);
    
    _ts_trend = detrend->getTrend();   // stright line represnets linear trend
    _ts_detrend = detrend->getDetrend(); // de-trended time series.
    
    // log
    if(!_ts_detrend.empty()) {
      
      LOG1(":...t_appRegress::......De-trended time series::Done!");
    }
    
    // delete pointer
    if ( detrend ) delete detrend;
    
  }
  else if(_elimSeas) {
    
    /// Create de-seasonalised time series
    t_deseas * deseas = new t_deseas(_data, _est, _res);
    
    _ts_seas = deseas->getSeas();   // seasonal harmonic model
    _ts_deseas = deseas->getDeseas(); // de-seasonalised time series.
    if(!_ts_deseas.empty()) { 
      
      LOG1(":...t_appRegress::......De-seasonalised time series::Done!");
    }
    else {
      
      ERR(":...t_appRegress::......Something wrong in de-seasonalising process!");
    }
    
    // TBD
    // v deseas triede:1. zmenim td na dd; potom podla metody deseas. Metoda moze byt aj median time series.
    
    // delete pointer
    if ( deseas ) delete deseas;
  }
  
  return 0;
}

/// @Detail
///   The main idea is to put the detrended time series into the coredata obj.
int t_appRegress::_addTimeSeries()
{
  
  if (_regModel == 1 || _regModel == 2) {
    
    if ( _ts_detrend.empty()) {     
      
      ERR(":...t_appRegress::......No data in _addTimeSeries method (case:trend)!");
      return -1;
    }
    
    _coredata -> addData("elimTrend", _ts_detrend);
  }
  else if (_regModel == 3) {
    
    if ( _ts_deseas.empty()) {
      
      ERR(":...t_appRegress::......No data in _addTimeSeries method (case:deseas)!");
      return -1;
    }
    
    _coredata -> addData("elimSeasonal", _ts_deseas);
  }
  
  return 0;
}

/// @Detail
///   Plotting function
int t_appRegress::_plotTimeSeries()
{
  
  if(_regModel == 1 || _regModel == 2) {
    
    LOG1(":...t_appRegress::......Request for plot!");
    
    t_plot mplot;
    
    if (_ts_trend.empty() || _ts_detrend.empty()) {     
      
      ERR(":...t_appRegress::......No data in _plotTimeSeries method!");
      return -1;
    }
    
    if (_ts_trend.size() != _ts_detrend.size()) {     
      
      ERR(":...t_appRegress::...... _plotTimeSeries method can not be applied!");
      return -1;
    }   
    
    ofstream mLine((_gnp + "/LOKI/gnuplot/trendDD"));
    
    for (m_dd::iterator i = _data.begin(); i != _data.end(); ++i) {
      
      m_dd::iterator j = _ts_trend.find(i->first);
      if( j != _ts_trend.end()) {
        
        m_dd::iterator k = _ts_detrend.find(i->first);
        
        if( k != _ts_detrend.end()) {
          
          mLine << fixed << setprecision(5)
             << i->first  << " "
             << i->second << " "
             << j->second << " "
             << k->second << endl;
        }
      }
    }
    
    mLine.close();
    
    /// double plot
    mplot.trend();
    LOG1(":...t_appRegress::......Trend plot is created (time-stamp::double)!");
  }
  else if (_regModel == 3) { // plot harmonic reg. model
    
    LOG1(":...t_appRegress::......Request for (deseas) plot!");
    
    t_plot mplot;
    
    if (_ts_seas.empty() || _ts_deseas.empty()) {     
      
      ERR(":...t_appRegress::......No data in _plotTimeSeries method (case: deseas)!");
      return -1;
    }
    
    if (_ts_seas.size() != _ts_deseas.size()) {     
      
      ERR(":...t_appRegress::...... _plotTimeSeries method can not be applied!");
      return -1;
    }   
    
    ofstream mLine((_gnp + "/LOKI/gnuplot/seasDD"));
    
    for (m_dd::iterator i = _data.begin(); i != _data.end(); ++i) {
      
      m_dd::iterator j = _ts_seas.find(i->first);
      if( j != _ts_seas.end()) {
        
        m_dd::iterator k = _ts_deseas.find(i->first);
        
        if( k != _ts_deseas.end()) {
          
          mLine << fixed << setprecision(5)
             << i->first  << " "
             << i->second << " "
             << j->second << " "
             << k->second << endl;
        }
      }
    }
    
    mLine.close();
    
    /// double plot
    mplot.seas(); // urob metodu seas
    
    LOG1(":...t_appRegress::......Seas plot is created (time-stamp::double)!");
  }
  
  return 0;
}