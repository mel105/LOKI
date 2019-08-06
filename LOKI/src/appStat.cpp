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

#include "appStat.h"
#include "convTDtoDD.h"
#include "logger.hpp"
#include "plot.h"
#include "stat.h"
#include "workingdir.h"

// contructor
// -----------
t_appStat::t_appStat(t_setting* setting,  t_coredata* coredata)
{
  
  /// Get setting & create log info
  vector<string> inpSett = setting->getLoadSetting(); _fmt  = inpSett[1];
  string outputName = setting->getOutputName(); _out = outputName;
  string outputHist = setting->getOutputHist(); _hst = outputHist;
  string plotOnOff = setting->getPlotOnOff(); _plot = plotOnOff;
  double iqrCnfd = setting->getIqrCnfd(); _iqr = iqrCnfd;
  bool conv = setting->getInputConvTdDd(); _conv = conv;
  
  /// Get actual working folder path
  t_workingDir workingDir; _gnp = workingDir.GetCurrentWorkingDir();
  
  LOG1(":...t_appStat::......Loaded appStat settings!");

   /// Get data from coredata & create log info.
  if (_fmt == "dd") {

    _data = coredata -> getData("origval");
    
    if(!_data.empty()) {
      
      LOG1(":...t_appStat::......Loaded data container. Data size: ", _data.size());
    }
    else{
      
      ERR(":...t_appStat::......Data container is empty!"); return;
    }
  }
  else if (_fmt == "td" && conv == true ) {
    
    _Data = coredata -> getTimeData("origval");
    
    if(!_Data.empty()) {
      
      LOG1(":...t_appStat::......Loaded data container. Data size: ", _Data.size());
      
    }
    else{
      
      ERR(":...t_appStat::......Data container is empty!"); return;
    }
  }
  else if (_fmt == "td" && conv == false ) {
  
    _data = coredata -> getData("origval");

    if(!_data.empty()) {
      
      LOG1(":...t_appStat::......Loaded data container. Data size: ", _data.size());
      
    }
    else{
      
      ERR(":...t_appStat::......Data container is empty!"); return;
    }
  }  
  else {
    
    ERR(":...t_appStat::......Requested data format is not supported!");
  }
  
  /// Process stat
  this -> _procStat();
}

/// Protected methods

/// @detail
///    Process constains:
///     - Basic descriptive statistics;
///     - Quartiles estimation;
///     - Normality test (by kolmogorov-smirnov, etc.) estimation; [NOT YET->TODO]
///     - Box chart figure plotting;
///     - Quantile-Quantile figure creation; [NOT YET->TODO]
void t_appStat::_procStat()
{
  
  /// Output setting
  ofstream hfile(_hst.c_str(), ios::out | ios::app);
  ofstream ofile(_out.c_str(), ios::out | ios::app);
  ofile << "# ********************************************* \n";
  ofile << "#            Statistical description            \n";
  ofile << "# ********************************************* \n\n";
  
  vector<double> vecStat;
  if (_fmt == "dd") {
    
    vecStat.clear();
    for(map<double, double>::iterator it = _data.begin(); it != _data.end(); ++it) {
      
      vecStat.push_back(it->second);
    }
  }
  else if (_fmt == "td" && _conv == true) {
    
    t_fromTDtoDD fromTDtoDD;  m_dd convOut = fromTDtoDD.FromTDtoDD(_Data);
    
    vecStat.clear();
    for(m_dd::iterator it = convOut.begin(); it != convOut.end(); ++it) {
      
#ifdef DEBUG
      cout << it->first << "  " << it->second << endl;
#endif      
      vecStat.push_back(it->second);
    }
  }
  else if (_fmt == "td" && _conv == false) {
    
    // AK MAM TD A CONV FALSE, TAK LEN OD PRVEJ HODNTY ODCITAT PRVU HDNOTU.    
    // conv false == key val => mjd
    vecStat.clear();
    
    for(m_dd::iterator it = _data.begin(); it != _data.end(); ++it) {
      
#ifdef DEBUG
      cout << it->first << "  " << it->second << endl;
#endif
      vecStat.push_back(it->second);
    }
  }
  else { 
    
    ERR(":...t_appStat::......Requested data format is not supported!");
  }
  
  size_t N = vecStat.size();

  /// 1. Descriptive statistics
  t_stat stat(vecStat, _iqr);

   ofile << fixed << setprecision(3);
                                                        ofile << " # Data size: " << N       << "\n";
   stat.calcMean();   double mean   = stat.getMean();   ofile << " # Mean:      " << mean    << "\n";
   stat.calcMedian(); double median = stat.getMedian(); ofile << " # Median:    " << median  << "\n";
   stat.calc1Qt();    double Qt1    = stat.get1Qt();    ofile << " # Qt1:       " << Qt1     << "\n";
   stat.calc3Qt();    double Qt3    = stat.get3Qt();    ofile << " # Qt3:       " << Qt3     << "\n";
   stat.calcIQR();    double iqr    = stat.getIQR();    ofile << " # IQR:       " << iqr     << "\n";
                      double upp    = stat.getUPP();    ofile << " # UPPIQR:    " << upp     << "\n";
                      double low    = stat.getLOW();    ofile << " # LOWIQR:    " << low     << "\n";
   stat.calcSdev();   double sdev   = stat.getSdev();   ofile << " # Sdev:      " << sdev    << "\n";
   stat.calcVare();   double vare   = stat.getVare();   ofile << " # Vare:      " << vare    << "\n";
   stat.calcMinMax(); double min    = stat.getMin();    ofile << " # Min val:   " << min     << "\n";
                      double max    = stat.getMax();    ofile << " # Max val:   " << max     << "\n";

   /// Mode Estimation
   stat.calcMode();   vector<double> mode = stat.getMode();
   for (auto x : mode) {
      ofile << " # Mode:    " << x << " (freq) " << stat.getModeFreq()  << "\n";
   }
   LOG1(":...t_appStat::......Descriptive statistic::Done!");

   /// Quantile values estimation
   ofile << "\n\n # Quantile estimation: \n";
   vector<double> REQQ;
   REQQ.push_back(0.0);
   REQQ.push_back(5.0);
   REQQ.push_back(25.0);
   REQQ.push_back(50.0);
   REQQ.push_back(75.0);
   REQQ.push_back(95.0);
   REQQ.push_back(100.0);

  if (N > 100) {
    
    for ( auto x : REQQ) {
      
      double quantile = stat.calcQuantile(x);
      ofile << " # Q[" << x << "] " << quantile << "\n";
    }
  }
  else {
    
    ofile << (" # *** Small data size");
    WARN1(":...t_appStat:......Warning::Data size is less than 100. Quantile estimation is not allowed!");
  }
  LOG1(":...t_appStat::......Quantile values estimation::Done!");
  
  /// If requested, then plot figures
  if (_plot == "on") {
    
    LOG1(":...t_appStat::......Request for plot!");
    t_plot mplot;
    
    if(_fmt == "dd") {
      
      ofstream mLine((_gnp + "/LOKI/gnuplot/line"));
      for (m_dd::iterator i = _data.begin(); i != _data.end(); ++i) {
        
        mLine << i->first << "  " << i->second << endl;
      }
      
      mLine.close();
      
      /// double plot
      mplot.line();
      LOG1(":...t_appStat::......Line plot created (time-stamp::double)!");
    }
    else {
      
      ofstream mLine((_gnp + "/LOKI/gnuplot/line"));
      for (m_td::iterator i = _Data.begin(); i != _Data.end(); ++i) {
        
        t_timeStamp epo = i->first;
        mLine << epo.timeStamp() << "  " << i->second << endl;
      }
      
      mLine.close();
      
      // time-string plot
      mplot.tline();
      LOG1(":...t_appStat::......Line plot created (time-stamp::t_gtime)!");
    }
    
    // plot histogram
    mplot.histogram(min, max);
    LOG1(":...t_appStat::......Data histogram created!");
    
    // plot box chart
    mplot.boxplot();
    LOG1(":...t_appStat::......Box-plot created!");
  }
  
  /// Notes what to do?
  hfile << "[2019-03-02] [mel] -> [stat] Added t_appStat; added stat classes\n";
  hfile << " TODO::t_appStat::- Usposobit histogram a boxplot\n";
  hfile << " TODO::t_appStat::- Normality test (by kolmogorov-smirnov, etc.) estimation\n";
  hfile << " TODO::t_appStat::- Quantile-Quantile figure creation\n";
  hfile << " TODO::t_appStat::- Plots::1. histogram (time-stamp = t_gtime), xylabels??? titile??? as a config params?\n";
}
