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

#include "appMedian.h"
#include "convTDtoDD.h"
#include "logger.hpp"
#include "plot.h"
#include "workingdir.h"

using namespace NEWMAT;

// contructor
// -----------
t_appMedian::t_appMedian(t_setting* setting,  t_coredata* coredata)
{
  _coredata = coredata;
  _setting = setting;
  
  /// Get setting & create log info
  vector<string> inpSett = setting->getLoadSetting(); _fmt = inpSett[1];
  string outputName = setting->getOutputName(); _out = outputName;
  string outputHist = setting->getOutputHist(); _hst = outputHist;   
  string plotOnOff = setting->getPlotOnOff(); _plot = plotOnOff;
  int constHour = setting->getConstHour(); _constHour = constHour;
  double tsResolution = setting->getInputResolution(); _tsResolution = tsResolution;
  bool fixedHour = setting->getFixedHour(); _fixedHour = fixedHour;
  
  /// Get actual working folder path
  t_workingDir workingDir; _gnp = workingDir.GetCurrentWorkingDir();
  
  LOG1(":...t_appMedian::......Loaded appMedian settings!");
     
  if (_fmt == "dd") {
    
    WARN1(":...t_appMedian::......For this data format, the median time series development is not supported!");
    return;
  }
  else {
    
    _Data = coredata -> getTimeData("origval");
    
#ifdef DEBUG // ok
    for(m_td::iterator it = _Data.begin(); it != _Data.end(); ++it)
       cout << it->first << " " << it->second << endl;
#endif
    
    if(!_Data.empty()) {
      
      LOG1(":...t_appMedian::......Loaded Original data container. Data size: ", _Data.size());
    }
    else {
      
      ERR(":...t_appMedian::......Data container is empty!"); return;
    }
  }
  
  /// Convert string into the mjd
  this -> _transTimeToMJD();

  /// Calculate median time series
  this->_calculateMedianTimeSeries();

  /// Prepare output
  this->_prepareDeseasonalisedSeries();

  /// Add deseasonalised data into the coredata
  this->_addCoreData();

  // If request -> plot data
  if(_plot == "on") this->_plotTimeSeries();
}

//-
int t_appMedian::_prepareDeseasonalisedSeries()
{

  for ( int iTime = 0; iTime < _timeVec.size(); ++iTime ) {
    
    t_timeStamp epo( _timeVec[iTime] );  double tmt = epo.mjd();
    
    m_dd::iterator iMedian = _MEDIAN_SERIES.find(tmt);
    m_dd::iterator iData = _data.find(tmt);
    
    if ( iMedian      != _MEDIAN_SERIES.end() &&
         iData        != _data.end()          &&
         iData->first == iMedian->first ) {
      
      _DataDeseas[ _timeVec[iTime] ] = ( iData->second - iMedian->second );
      _dataDeseas[ tmt ] = ( iData->second - iMedian->second );      
    }
  }
  
  return 0;
}

//-
int t_appMedian::_addCoreData()
{
 
  if ( _DataDeseas.empty() || _dataDeseas.empty() ) {
      
      ERR(":...t_appMedian::......No data in _addCoreData method!");
    
      return -1;
  } else {
    
    /// added in format: string, double
    _coredata -> addData("elimSeasonal", _DataDeseas);
    /// added in format: double, double
    _coredata -> addData("elimSeasonal", _dataDeseas);
    
#ifdef DEBUG
    for(m_dd::iterator i = _dataDeseas.begin(); i != _dataDeseas.end(); ++i)
       cout << i->first << " appMedian " << i->second << endl;
#endif
  }
  
  return 0;
}

//-
void t_appMedian::_plotTimeSeries()
{

  if ( !_MEDIAN_SERIES.empty() && !_data.empty() ) {

    LOG1(":...t_appMedian::......Request for plot!");
  
    t_plot mplot;
    
    ofstream mLine((_gnp + "/PROHO/gnuplot/medianDD"));
    
    for (m_dd::iterator i = _data.begin(); i != _data.end(); ++i) {
      
      m_dd::iterator j = _MEDIAN_SERIES.find(i->first);
      
      if( j != _MEDIAN_SERIES.end() ) {
        
        mLine << fixed << setprecision(5)
             << i->first  << " "
             << i->second << " "
             << j->second << " "
             << i->second - j->second << endl;
      }
    }
    
    mLine.close();
    
    /// double plot
    mplot.median();
    LOG1(":...t_appMedia::......Median plot is created (time-stamp::double)!");
  }
  else {
    
    ERR(":...t_appMedian::......Problem with data creating!");
  }
}

// -
void t_appMedian::_transTimeToMJD()
{
  
  for( map<string, double>::iterator I = _Data.begin(); I != _Data.end(); ++I ) {
    
    t_timeStamp epo(I->first); double tmt = epo.mjd();
    
    _timeVec.push_back(I->first);
    
    // data[MJD] = double
    _data[tmt] = (I->second);
  }
  
  if(!_data.empty()) {
    
#ifdef DEBUG //ok
    for(m_dd::iterator i = _data.begin(); i != _data.end(); ++i)
       cout << i->first << "  " << i->second << endl;
#endif
    
    LOG1(":...t_appMedian::......Provided data conversion!");
  }
}

// -
void t_appMedian::_calculateMedianTimeSeries()
{
  
  m_td::iterator I;         I = _Data.begin();  _begy = I->first; t_timeStamp beg(_begy); _BEGY = beg.year();
  m_td::reverse_iterator J; J = _Data.rbegin(); _endy = J->first; t_timeStamp end(_endy); _ENDY = end.year();
  
  vector<double> KK;
  
  int IDX = 1;
  
  if (_fixedHour == false) { _hourRes = this->_getHourResolution(); }
  
  /// loop over months
  for(int indexMonth = 1; indexMonth <= 12; indexMonth++) {
    
    /// loop over days
    for(int indexDay = 1; indexDay <= 31; indexDay++) {
      
      int MON = 999;
      int DAY = 999;
      int HOUR = 999;
      double actualMedian = 999.9;
      
      if( ( indexMonth == 2  && indexDay > 29 ) ||
          ( indexMonth == 4  && indexDay > 30 ) ||
          ( indexMonth == 6  && indexDay > 30 ) ||
          ( indexMonth == 9  && indexDay > 30 ) ||
          ( indexMonth == 11 && indexDay > 30 ) ) {
        
        break;
      } else {
        
        if ( _fixedHour ) { // fixed hour = true
          
          /// loop over years
          for(int indexYear = _BEGY; indexYear <= _ENDY; indexYear++) {
            
            t_timeStamp EPO(indexYear, indexMonth, indexDay, _constHour, 0, 0); 
            
            this -> _findEpoInInterestSeries( EPO, MON, DAY, HOUR );
          } /// END: loop over indexYear
          
          t_stat statMedian(_toMedianStat); statMedian.calcMedian(); actualMedian = statMedian.getMedian(); _toMedianStat.clear();
          
          _MEDIAN[MON][DAY][HOUR] = actualMedian;
          
        } else { // fixed hour = false
          
          /// loop over hours
          for( int indexHour = 0; indexHour < 24 ; indexHour = indexHour + _hourRes ) {
            
            /// loop over years
            for(int indexYear = _BEGY; indexYear <= _ENDY; indexYear++) {              
              
              t_timeStamp EPO(indexYear, indexMonth, indexDay, indexHour, 0, 0);
              
              this -> _findEpoInInterestSeries( EPO, MON, DAY, HOUR );
              
            } /// END: loop over indexYear
            
            t_stat statMedian( _toMedianStat ); statMedian.calcMedian(); actualMedian = statMedian.getMedian(); _toMedianStat.clear();
      
            _MEDIAN[MON][DAY][HOUR] = actualMedian;
            
#ifdef DEBUG
            cout
               << fixed << setprecision(4)
               << "IDX: "   <<setw(4)<< IDX
               << " MONTH: "<<setw(2)<< MON
               << " DAY: "  <<setw(2)<< DAY
               << " HOUR: " <<setw(2)<< HOUR
               << " MED: "  <<setw(6)<< actualMedian
               << endl;
#endif
            IDX++;
            
          } /// END:loop over indexHour
        } /// END:cond. over _fixedHour
      } /// END:loop over if cond.
    } /// End::loop over days
  } /// End::loop ever months
  
  /// Give me the "full median" series for total time span of original series of interest!
  this->_produceMedianYearSeries();
}

int t_appMedian::_getHourResolution()
{
  
  double dayInSec = 86400.0;
  double rat = dayInSec / _tsResolution;
  
  return 24.0 / rat;
}

// -
void t_appMedian::_produceMedianYearSeries()
{
  
  /// Loop over years
  for( int i = _BEGY; i <= _ENDY; i++) {
    
    if (!_MEDIAN.empty()) {
      
      for(map<int, map<int, map<int, double>>>::iterator ii = _MEDIAN.begin(); ii!=_MEDIAN.end(); ++ii) {
      
        for(map<int, map<int, double>>::iterator iii = ii->second.begin(); iii != ii->second.end(); ++iii) {
          
          for(map<int, double>::iterator iiii = iii->second.begin(); iiii != iii->second.end(); ++iiii) {
            
            int month = ii->first;
            int day = iii->first;
            int hour = iiii->first;
            double val = iiii->second;
            
            /// nebude mi to samozrejme fungovat. Point je, ze nemam triple. Potrebujem dostat nieco, ako mapu
            t_timeStamp time_ref(i, month, day, hour, 0, 0); //time_ref.from_ymdhms(i, month, day, hour, 0, 0, true);
            t_timeStamp beg(_begy);
            t_timeStamp end(_endy);
          
            if( ( time_ref.mjd() >= beg.mjd() )  &&
                ( time_ref.mjd() <= end.mjd() )) {
        
              _MEDIAN_SERIES[time_ref.mjd()] = val;

#ifdef DEBUG
              cout 
                 << " year " << i
                 << " mont " << month
                 << " day  " << day
                 << " tr   " << time_ref.timeStamp()
                 << " MJD  " << time_ref.mjd()
                 << " val  " << val
                 << endl;
#endif
            }
          } /// end of if loop
        } /// end of iii iter
      } /// end of ii iter
    } /// end of if loop
    else{
      
      break;
    }
  } /// end of i iter
}

// -
int t_appMedian:: _findEpoInInterestSeries(t_timeStamp EPO, int& mMon, int& mDay, int& mHour)
{
  
  m_dd::iterator j;  j = _data.find(EPO.mjd());
  
  if ( j != _data.end() ) {
    
#ifdef DEBUG
    cout << fixed << setprecision(5) << EPO.timeStamp() << "  " << EPO.mjd() << " values to median calculation: " << j->first << "  " << j->second << endl;
#endif
    
    mMon = EPO.month();
    mDay = EPO.day();
    mHour = EPO.hour();
    
    _toMedianStat.push_back( j->second );
    
  }
  
  return 0;
}                                                                                                                                         