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

#include "appDetection.h"
#include "adjustSeries.h"
#include "changePoint.h"
#include "convTDtoDD.h"
#include "deseas.h"
#include "fit.h"
#include "logger.hpp"
#include "plot.h"
#include "workingdir.h"

using namespace NEWMAT;

// contructor
// -----------
t_appDetection::t_appDetection(t_setting* setting,  t_coredata* coredata)
{
  _coredata = coredata;
  _setting = setting;
  
  /// default setting
  _counter = 0;
  _result = "stationary";
  
  /// Get setting & create log info
  vector<string> inpSett = setting->getLoadSetting(); _fmt = inpSett[1];
  string outputName = setting->getOutputName(); _out = outputName;
  string outputHist = setting->getOutputHist(); _hst = outputHist;   
  string plotOnOff = setting->getPlotOnOff(); _plot = plotOnOff;
  string regressOnOff = setting->getRegressOnOff(); _regressOnOff = regressOnOff;
  string medianOnOff = setting->getMedianOnOff(); _medianOnOff = medianOnOff;
  string referenceOnOff = setting->getReferenceOnOff(); _referenceOnOff = referenceOnOff;
  
  /// Get actual working folder path
  t_workingDir workingDir; _gnp = workingDir.GetCurrentWorkingDir();
  
  bool conv = setting->getInputConvTdDd(); _conv = conv;
  double res = setting->getInputResolution(); _res = res;
  double prob = setting->getProbCritVal(); _prob = prob;
   
  LOG1(":...t_appDetection::......Loaded appDetection settings!");
     
  if (_fmt == "dd") {
    
    _data = coredata -> getData("origval");

    if(!_data.empty()) {
      
      LOG1(":...t_appDetection::......Loaded original data container. Data size: ", _data.size());
    }
    else{
      
      ERR(":...t_appDetection::......Data container is empty!"); return;
    }
  }
  else {
    
    _Data = coredata -> getTimeData("origval");

#ifdef DEBUG  
    for(m_td::iterator it = _Data.begin(); it != _Data.end(); ++it)
       cout << it->first << "  " << it->second << endl;
#endif
    
    if(!_Data.empty()) {
      
      LOG1(":...t_appDetection::......Loaded Original data container. Data size: ", _Data.size());
      
      // Prepare & Transform data
      this->_prepareData();
    }
    else {
      
      ERR(":...t_appDetection::......Data container is empty!"); return;
    }
  }
  
  /// Process time series homogenization
  this->_processChangePointDetection();

  // If request -> plot data
  if(_plot == "on") {
    
    this->_plotTimeSeries();
  }
}

/// @Details
///   - Function prepares data for further using!
int t_appDetection::_prepareData()
{
  
  LOG1(":...t_appDetection::......Data preparation!");
  
  for(m_td::iterator it = _Data.begin(); it != _Data.end(); ++it) {
    
    //t_timeStamp epo; epo = it->first;
    _timeData.push_back(it->first);
  }

  // convert data from string, double to the double, double
  t_fromTDtoDD fromTDtoDD;  _data = fromTDtoDD.FromTDtoDD(_Data);
  
#ifdef DEBUG //ok: first epo is eliminated from rest of time epochs
  for(m_dd::iterator it = _data.begin(); it != _data.end(); ++it)
     cout << it->first << " prepared  " << it->second << endl;
#endif
  
  if(!_data.empty()) {
    
    LOG1(":...t_appDetection::......Data transformation was successful. Data size: ", _data.size());
    _coredata->addData("origval", _data);
  } else {
      
    ERR(":...t_appDetection::......Data container is empty!"); return -1;
  }
  
  return 0;
}

// -
int t_appDetection::_plotTimeSeries()
{
  
  LOG1(":...t_appDetection::......Request for plot!");
  
  t_plot mplot;
  
  // Prepare data for plot
  if ( _data.empty() ||
       _deseas.empty() ||
       _TK.empty() ) {
    
    ERR(":...t_appDetection()::......Problem with plot creating!");
  }
  
#ifdef DEBUG //ok: 
  for(m_dd::iterator it = _data.begin(); it != _data.end(); ++it)
     cout << it->first << " plot  " << it->second << endl;
#endif
  
  t_fromTDtoDD fromTDtoDD;
  m_dd mData = fromTDtoDD.FromTDtoDD(_data);
  m_dd mDeseas = fromTDtoDD.FromTDtoDD(_deseas);
  m_dd mTk = fromTDtoDD.FromTDtoDD(_TK);  
  
  ofstream mLine((_gnp + "/LOKI/gnuplot/homogenDD"));
  
  for(m_dd::iterator it = mData.begin(); it != mData.end(); ++it) {
    
    m_dd::iterator itD = mDeseas.find(it->first);
    
    if (itD != mDeseas.end())	{
      
      m_dd::iterator itT = mTk.find(it->first);
      
      if (itT != mTk.end()) {
        
        mLine << fixed << setprecision(5)
           << it->first   << " "
           << it->second  << " "
           << itD->second << " "
           << itT->second << " "
           << _CV         << endl;
      }
    }
  }
  
  mLine.close();
  
  /// Plot
  mplot.hTime();
  mplot.hDeseas();
  mplot.hTK();
  
  LOG1(":...t_appDetection::......Plots are created (time-stamp::double)!");
  
  return 0;
}

/// -
int t_appDetection::_processChangePointDetection()
{
/*  
  /// Output setting
  ofstream ofile(_out.c_str(), ios::out | ios::app);
  ofile << "\n# ********************************************* \n";
  ofile << "#   Change point detection and Detectionization   \n";
  ofile << "# ********************************************* \n\n";
  ofile.close();
*/  
  // Pointers definition
  t_changePoint * changePoint = 0;
  t_adjustSeries * adjustSeries = 0;
  
  // Get deseasonalised data to change point detection process
  if ( _regressOnOff == "on" || _medianOnOff == "on") {
    
    _deseas = _coredata -> getData("elimSeasonal");
    
#ifdef DEBUG
    for(m_dd::iterator i = _deseas.begin(); i != _deseas.end(); ++i)
       cout << i->first << " appDetection " << i->second << endl;
#endif
    
    this -> _checkDeseasData(); _adjustString = "elimSeasonal";
  }
  else {
    
    WARN1(":...t_appDetection::......Seasonal model is not removed from the original data!");

    _deseas = _data;
    
    this -> _checkDeseasData();  _adjustString = "origval";
  } 
  
  _N = _deseas.size();
  
  /// Part A: Change point detection.
  /// ---------------------------------------------------------------------------------------------
  /// Provide process of change point detection
  changePoint = new t_changePoint( _setting, _coredata );

  // Get index of suspected ch.p.
  _suspectedChangeIdx = changePoint->getIdxMaxPOS();

  // Get shift
  _shift = changePoint->getShift();

  // Get critical value
  _CV = changePoint->getCritVal();

  // Get autocorrelation fun. value estimation (lag = 1)
  _acf = changePoint->getACF();

  // Get p value
  _pVal = changePoint->getPValue();

  // Get max TK statistic
  _maxTk = changePoint->getMaxTK();

  // Get result about the stationarity
  _result = changePoint->getResult();

  // Get vector of all TK statistics
  _TK = changePoint->getTK();

#ifdef DEBUG
  cout << "\n# Suspected time No# " << _counter
       << "\n# Detection's result " << _result
       << "\n# [epoch-idx]        " << _suspectedChangeIdx
       << "\n# [epoch-timestamp]  " << ( _timeData[ _suspectedChangeIdx -1 ] ).timeStamp()
       << "\n# [shift]            " << fixed << setprecision(2) << _shift
       << "\n# [Critical value]   " << _CV
       << "\n# [Max Tk value]     " << _maxTk
       << "\n# [ACF[1]]           " << _acf
       << "\n# [P-Value]          " << _pVal << endl;
#endif
  
  if ( _result == "non-stationary" ) {
    
    /*
    // add change point into the container
    _listOfChpsString.push_back( (_timeData[_suspectedChangeIdx -1]).timeStamp() );
     t_timeStamp convMjd((_timeData[_suspectedChangeIdx -1]).timeStamp());
    _listOfChpsMJD.push_back( convMjd.mjd() );
     */
    
    /// Part B: If non-stationary, define itnervals for multi change-point detection
    /// -------------------------------------------------------------------------------------------
    vector<int> idxVec;
    idxVec.push_back(0);
    idxVec.push_back(_suspectedChangeIdx);
    idxVec.push_back(_N);

    m_ii inter; 
    inter = this->_prepareIntervals( idxVec );

    /// Part C: If the original time series is non-stationary, provide multi change-point detection
    /// -------------------------------------------------------------------------------------------
    int iNo = 1;  
    
//    while (!inter.empty()) {
       
//#ifdef DEBUG
       cout << "\n\n Loop: " << iNo << endl;
//#endif
       
       int idxIt = 1;
      
       for ( m_ii::iterator it = inter.begin(); it != inter.end(); ++it) {
        
//        int iBeg = it->first; // Analysed series begin
//        int iEnd = it->second; // Analysed series end

         int iBeg = 0; 
         int iEnd = 777;
                     
        changePoint = new t_changePoint( _setting, _coredata, iBeg, iEnd );

        int suspectedPointIdx = changePoint->getIdxMaxPOS();
        
        suspectedPointIdx = suspectedPointIdx + iBeg;
       
//#ifdef DEBUG
        cout << "\n\nAnalysed time series [from/to/result]: " << iBeg << "  " << iEnd << "  " << changePoint->getResult() << "\n";
//#endif
	  
         if ( changePoint->getResult() == "stationary" && idxIt % 2 != 0 ) {
           cout << "db14" << endl;
           cout << "idxVec size " << idxVec.size() << endl;
          for (int j = 0; j < idxVec.size(); j++){
            cout << j << " idx " << idxVec[j] << " iBeg " << iBeg << endl;
          }
         
          idxVec.erase( remove( idxVec.begin(), idxVec.end(), iBeg) );
          cout << "db14a" << endl;
        }else if ( changePoint->getResult() == "stationary" && idxIt % 2 == 0 ) {
          cout << "som tu?" << endl;
          cout << "db15" << endl;
          idxVec.erase( remove( idxVec.begin(), idxVec.end(), iEnd) );
          cout << "db15a" << endl;
        }
        
        if ( changePoint->getResult() == "non-stationary" ) {
          
          idxVec.push_back( suspectedPointIdx );
	   
          /*
          // add change point into the container
          _listOfChpsString.push_back( (_timeData[suspectedPointIdx -1]).timeStamp() );
          t_timeStamp convMjd((_timeData[suspectedPointIdx -1]).timeStamp());
          _listOfChpsMJD.push_back( convMjd.mjd() );
           */ 
        }
         
#ifdef DEBUG
        cout << "\n# Detection's result " << changePoint->getResult()
             << "\n# TS beg idx         " << iBeg
             << "\n# TS end idx         " << iEnd
             << "\n# [epoch-idx]        " << suspectedPointIdx
             << "\n# [epoch-timestamp]  " << ( _timeData[ suspectedPointIdx -1 ] ).timeStamp()
             << "\n# [shift]            " << fixed << setprecision(2) << changePoint->getShift()
             << "\n# [Critical value]   " << changePoint->getCritVal()
             << "\n# [Max Tk value]     " << changePoint->getMaxTK()
             << "\n# [ACF[1]]           " << changePoint->getACF()  
             << "\n# [P-Value]          " << changePoint->getPValue()
             << endl;
#endif
        
         /*
         cout << "\n";
         for(map<int,int>::iterator i = inter.begin(); i!=inter.end(); i++)
            cout << i->first << " PRED VYMAZANIM " << i->second << endl;
          */
         
//         idxIt++;
//         inter.erase(iBeg);
         
         /*
         cout << "\n";
         for(map<int,int>::iterator i = inter.begin(); i!=inter.end(); i++)
            cout << i->first << " PO VYMAZANI  " << i->second << endl;
          */ 
                  
       } // it iterator (loop via inter container)
      
//      inter = this->_prepareIntervals( idxVec );
      
//      iNo++;
//    } // while inter != empty
  } // if _result == non-stationary
  else {
    
    _result == "stationary";
    cout << "zde";
  }
  
#ifdef DEBUG
  cout << "List of detected change point(s) [time stamp/mjd]:\n";
  for (int i = 0; i<_listOfChpsString.size(); i++) {
    
    cout <<  _listOfChpsString[i] << "  " << _listOfChpsMJD[i] << endl;
  }
#endif

  // delete pointers
  if ( changePoint  ) delete changePoint;
  if ( adjustSeries  ) delete adjustSeries;
  
  return 0;
} // _process

// --- 
int t_appDetection::_checkDeseasData() 
{

  if ( _deseas.empty() ) {
    
    ERR(":......t_appDetection......No cleaned data to change-point detection!");
  }
  else {
    
#ifdef DEBUG
    for(m_dd::iterator i = _deseas.begin(); i != _deseas.end(); ++i)
       cout << i->first << " _checkDeseasData " << i->second << endl;
#endif
    
    LOG1(":......t_appDetection......Loaded cleaned data to change-point detection!");
  }
  
  return 0;
}

// ---
map<int, int> t_appDetection::_prepareIntervals( vector<int>& idxVec )
{
  
  // sort data in vector first
  sort(idxVec.begin(), idxVec.end());
//#ifdef DEBUG
  for (int i = 0; i < idxVec.size()-1; i++) {
    
    cout << "Analysed time series [from/to]: " << idxVec[i] << "  " <<  idxVec[i+1] << "\n" << endl;
  }
//#endif
  
  m_ii intervals;
  
  if ( idxVec.size() < 3) {
    
//#ifdef DEBUG
    cout << " Warning::t_appDetection::_prepareIntervals::IdxVec<3! Change point detection is interrupted!\n" << endl;
//#endif
  }
  else {
    
    for ( int i = 0; i < idxVec.size()-1; i++ ) {
    
      int beg = idxVec[i];
      int end = idxVec[i+1];
      
      intervals[beg] = end;
    }
  }
  
  return intervals;
}