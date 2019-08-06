/************************************************************************************************** 
 PROHO - software development library
  
 (c) 2019 Michal Elias
  
 This file is part of the PROHO C++ library.
  
 This library is free software; you can redistribute it and/or  modify it under the terms of the GNU
 General Public License as  published by the Free Software Foundation; either version 3 of the 
 License, or (at your option) any later version.
  
 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.
  
 You should have received a copy of the GNU General Public License  along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#include "appHomogen.h"
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
t_appHomogen::t_appHomogen(t_setting* setting,  t_coredata* coredata)
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
   
  LOG1(":...t_appHomogen::......Loaded appHomogen settings!");
     
  if (_fmt == "dd") {
    
    _data = coredata -> getData("origval");

    if(!_data.empty()) {
      
      LOG1(":...t_appHomogen::......Loaded original data container. Data size: ", _data.size());
    }
    else{
      
      ERR(":...t_appHomogen::......Data container is empty!"); return;
    }
  }
  else {
    
    _Data = coredata -> getTimeData("origval");

#ifdef DEBUG  
    for(m_td::iterator it = _Data.begin(); it != _Data.end(); ++it)
       cout << it->first << "  " << it->second << endl;
#endif
    
    if(!_Data.empty()) {
      
      LOG1(":...t_appHomogen::......Loaded Original data container. Data size: ", _Data.size());
      
      // Prepare & Transform data
      this->_prepareData();
    }
    else {
      
      ERR(":...t_appHomogen::......Data container is empty!"); return;
    }
  }
  
  /// Process time series homogenization
  this->_processHomogenization();

  // If request -> plot data
  if(_plot == "on") {
    
    this->_plotTimeSeries();
  }
}

/// @Details
///   - Function prepares data for further using!
int t_appHomogen::_prepareData()
{
  
  LOG1(":...t_appHomogen::......Data preparation!");
  
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
    
    LOG1(":...t_appHomogen::......Data transformation was successful. Data size: ", _data.size());
    _coredata->addData("origval", _data);
  } else {
      
    ERR(":...t_appHomogen::......Data container is empty!"); return -1;
  }
  
  return 0;
}

// -
int t_appHomogen::_plotTimeSeries()
{
  
  LOG1(":...t_appHomogen::......Request for plot!");
  
  t_plot mplot;
  
  // Prepare data for plot
  if ( _data.empty() ||
       _deseas.empty() ||
       _TK.empty() ) {
    
    ERR(":...t_appHomogen()::......Problem with plot creating!");
  }
  
#ifdef DEBUG //ok: 
  for(m_dd::iterator it = _data.begin(); it != _data.end(); ++it)
     cout << it->first << " plot  " << it->second << endl;
#endif
  
  t_fromTDtoDD fromTDtoDD;
  m_dd mData = fromTDtoDD.FromTDtoDD(_data);
  m_dd mDeseas = fromTDtoDD.FromTDtoDD(_deseas);
  m_dd mTk = fromTDtoDD.FromTDtoDD(_TK);  
  
  ofstream mLine((_gnp + "/PROHO/gnuplot/homogenDD"));
  
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
  
  LOG1(":...t_appHomogen::......Plots are created (time-stamp::double)!");
  
  return 0;
}

/// -
int t_appHomogen::_processHomogenization()
{
  
  /// Output setting
  ofstream hfile(_hst.c_str(), ios::out | ios::app);
  hfile << "[2019-05-18] [mel] -> [appHomogen] First version of change-point detection method implemented@" << endl;
  hfile << "[2019-05-21] [mel] -> [appHomogen] Added plots!" << endl;
  hfile << "[2019-06-06] [mel] -> [appHomogen] Median series created [fst vers]!" << endl;
  hfile.close();
  
  ofstream ofile(_out.c_str(), ios::out | ios::app);
  ofile << "\n# ********************************************* \n";
  ofile << "#   Change point detection and Homogenization   \n";
  ofile << "# ********************************************* \n\n";
  ofile.close();
  
  // Pointers definition
  t_changePoint * changePoint = 0;
  t_adjustSeries * adjustSeries = 0;
  
  // Get deseasonalised data to change point detection process
  if ( _regressOnOff == "on" || _medianOnOff == "on") {
    
    _deseas = _coredata -> getData("elimSeasonal");
    
#ifdef DEBUG
    for(m_dd::iterator i = _deseas.begin(); i != _deseas.end(); ++i)
       cout << i->first << " appHomogen " << i->second << endl;
#endif
    
    this -> _checkDeseasData(); _adjustString = "elimSeasonal";
  }
  else {
    
    WARN1(":...t_appHomogen::......Seasonal model is not removed from the original data!");

    _deseas = _data;
    
    this -> _checkDeseasData();  _adjustString = "origval";
  } 
  
  /// Stop criterion::if stopProcess == true, then stop running the process of detection
/*  bool stopProcess  = false;
  
  while (stopProcess == false) {
 */   
#ifdef DEBUGa
    for(m_dd::iterator i = _data.begin(); i!= _data.end(); ++i) {
      
      cout << "Processed: " << i->first << "  " << i->second << endl;
    }
#endif
   
    /// Part A: Change point detection.
  
    /// Provide process of change point detection
    changePoint = new t_changePoint( _setting, _coredata );
        
    /// Get results
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
         << "\n# [epoch-idx]        " << _suspectedChangeIdx
         << "\n# [epoch-timestamp]  " << ( _timeData[ _suspectedChangeIdx -1 ] ).timeStamp()
         << "\n# [epoch-mjd]        " << fixed << setprecision(1) << _suspectChp
         << "\n# [shift]            " << fixed << setprecision(2) << _shift
         << "\n# [Critical value]   " << _CV
         << "\n# [Max Tk value]     " << _maxTk
         << "\n# [ACF[1]]           " << _acf
         << "\n# [P-Value]          " << _pVal << endl;

#endif
  
  /// Part B: Time series (reduced by seasonal/median year) homogenization.
  
  // POINTA TEJTO CASTI JE CASOVU RADU VYROVNAT O SKOK A OPAT JU ANALYZOVAT V PART A.
  
  // Mam detekovany change point, teraz casovu radu adjustuj!
  // Casova rada pred adjustom
#ifdef DEBUG
  for(m_dd::iterator it = _data.begin(); it != _data.end(); ++it)
     cout << fixed << setprecision(5) << "predAdjust: " << it->first << "  " << it->second << endl;
#endif
  // Adjust deseasonalised time series
  adjustSeries = new t_adjustSeries( _coredata, _suspectedChangeIdx, _shift, _adjustString );
  
  // Data cleaning
  _data.clear();
  _TK.clear();
  
  // Get adjusted time series & alebo si mam vybrat data z coredata. V adjusted radu ulozim ako mjd, nie ako poradove cislo. Oprav tam.
  _data = adjustSeries->getAdjustedSeries();
  t_fromTDtoDD fromTDtoDD; _data = fromTDtoDD.FromTDtoDD(_data);
  
  // Casova rada ocistena o sezonu zlosku po adjuste
#ifdef DEBUG
  for(m_dd::iterator it = _data.begin(); it != _data.end(); ++it)
     cout << "poAdjust: " << it->first << "  " << it->second << endl;
#endif

  // UISTI SA, ZE CI V COREDATA JE ADJUSTOVANA TS.
  
  /// Part C: Change point repeatibility verification & stop process setting!

  // POINTA TEJTO CASTI JE STANOVIT STOP PROCESS KRITERIUM.
  // 1. AK CHANGE POINT JE UNIKAT (NEMAM HO), STOP PROCESS == FALSE
  // 2. AK JE CHANGE POINT V INTERVALE +/- STANOVENA HRANICA, STOP PROCESS == FALSE
  // 3. INAK STOP PROCESS == TRUE A POJDEM NA CAST D.
  _counter++; bool verifiedChP = _verifyChangePointRepeatability();
  
  /// Part D: Original time series homogenization
  
  /// Part E: Create protocol
  
  
  
/*    if (_result == "non-stationary") {
      
      if (!_timeData.empty()) {
        
        t_timeStamp epo = _timeData[ _suspectedChangeIdx -1 ];
        _suspectChp = epo.mjd();
      } else {
        
        ERR(":...t_appHomogen()......No _timeData. Problem with change-point processing!");
      }
      
      // Verification of change point repeatibility
      _counter++;  bool verifiedChP = _verifyChangePointRepeatability();
      
      /// If the chp is verified (true) then adjust deseasonalised time series.
      if ( verifiedChP ) {
        
        // Make protocol
        this -> _verifiedProtocol();

        // Adjust deseasonalised time series
        adjustSeries = new t_adjustSeries( _coredata, _suspectedChangeIdx, _shift, _adjustString );
        
        // Data cleaning
        _data.clear();
        _TK.clear();
                
        // Get adjusted time series & alebo si mam vybrat data z coredata.
        _data = adjustSeries->getAdjustedSeries();
      } 
      else { 
        // Chp was not verified. Stop running the process
        // Make protocol
        this -> _nonVerifiedProtocol();
        
        stopProcess = true;
      } // END:: chp verification
    } 
    else {
      
      // TS is supposted to be stationary
      // Make protocol
      this -> _nonVerifiedProtocol();

      stopProcess = true;
    } // END::test results
  } // end of while loop

  // delete pointers
  if ( changePoint  ) delete changePoint;
  if ( adjustSeries ) delete adjustSeries;
*/ 
  return 0;
}

void t_appHomogen::_verifiedProtocol()
{
  
  ofstream ofile(_out.c_str(), ios::out | ios::app);
  
  ofile << "\n# Suspected time No# " << _counter;  
  ofile << "\n# [epoch-idx]        " << _suspectedChangeIdx;  
  ofile << "\n# [epoch-timestamp]  " << ( _timeData[ _suspectedChangeIdx -1 ] ).timeStamp();
  ofile << "\n# [epoch-mjd]        " << fixed << setprecision(1) << _suspectChp;
  ofile << "\n# [shift]            " << fixed << setprecision(2) << _shift;
  ofile << "\n# [Critical value]   " << _CV;
  ofile << "\n# [Max Tk value]     " << _maxTk;
  ofile << "\n# [ACF[1]]           " << _acf;
  ofile << "\n# [P-Value]          " << _pVal << endl;
      
  ofile.close();
}

void t_appHomogen::_nonVerifiedProtocol()
{

  ofstream ofile(_out.c_str(), ios::out | ios::app);
  
  ofile << "\n# Suspected time No# " << _counter;
  ofile << "\n# [epoch-idx]        " << _suspectedChangeIdx;
  ofile << "\n# [epoch-timestamp]  " << ( _timeData[ _suspectedChangeIdx -1 ] ).timeStamp();
  ofile << "\n# [shift]            " << fixed << setprecision(2) << _shift;
  ofile << "\n# [Critical value]   " << _CV;
  ofile << "\n# [Max Tk value]     " << _maxTk;
  ofile << "\n# [ACF[1]]           " << _acf;
  ofile << "\n# [P-Value]          " << _pVal << endl;
  ofile << "\n# Analysed time series is " << _result << " with probability of ";
  ofile << _prob * 100.0 <<"[%]"<< endl;
  
  ofile.close();
}

// --- 
int t_appHomogen::_checkDeseasData() 
{

  if ( _deseas.empty() ) {
    
    ERR(":......t_appHomogen......No cleaned data to change-point detection!");
  }
  else {
    
#ifdef DEBUG
    for(m_dd::iterator i = _deseas.begin(); i != _deseas.end(); ++i)
       cout << i->first << " _checkDeseasData " << i->second << endl;
#endif
    
    LOG1(":......t_appHomogen......Loaded cleaned data to change-point detection!");
  }
  
  return 0;
}

// --- 
bool t_appHomogen::_verifyChangePointRepeatability()
{

  bool verifiedChP;
  
  if( _result=="non-stationary" ) {
    
    if( _counter == 1 ) {
      
      // Detected first suspected epoch
      verifiedChP = true;
      
      _containerOfChps.push_back( _suspectChp );
      _containerOfSfts.push_back( _shift );
    } else {
      
      size_t ID = 0;
      bool nas = false;
      
      while( ID < _containerOfChps.size()) {
        
        if( ( _suspectChp >= ( _containerOfChps[ID] - 15.0 ) ) &&
            ( _suspectChp <= ( _containerOfChps[ID] + 15.0 ) ) ) {
          
          double new_val = _containerOfSfts[ID] + _shift;
          replace(_containerOfSfts.begin(), _containerOfSfts.end(), _containerOfSfts[ID], new_val);
	   
          verifiedChP = false;
          nas = true;
        }
        
        ID++;
      }
      
      if( nas == false ) {
        
        _containerOfChps.push_back( _suspectChp );
        _containerOfSfts.push_back( _shift );
        verifiedChP = true;
      }
    }
  }
  
  return verifiedChP;
}