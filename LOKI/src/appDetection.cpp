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


// .contructor
t_appDetection::t_appDetection(t_setting* setting,  t_coredata* coredata)
{
   _coredata = coredata;
   _setting = setting;
   
   // .default setting
   _counter = 0;
   _result = "stationary";
   
   // .get setting & create log info
   _inpSett = setting->getLoadSetting(); _fmt = _inpSett[2]; //_res = _inpSett[4];
   //
   string outputName = setting->getOutputName(); _out = outputName;
   string outputList = setting->getOutputList(); _lst = outputList;
   string outputHist = setting->getOutputHist(); _hst = outputHist;   

   string plotOnOff = setting->getPlotOnOff(); _plot = plotOnOff;
   
   string regressOnOff = setting->getRegressOnOff(); _regressOnOff = regressOnOff;
   string medianOnOff = setting->getMedianOnOff(); _medianOnOff = medianOnOff;
   string referenceOnOff = setting->getReferenceOnOff(); _referenceOnOff = referenceOnOff;
   
   // .get actual working folder path
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
      
      if(!_Data.empty()) {
	 
	 LOG1(":...t_appDetection::......Loaded Original data container. Data size: ", _Data.size());
	 
	 // .prepare & Transform data
	 this->_prepareData();
      }
      else {
	 
	 ERR(":...t_appDetection::......Data container is empty!"); return;
      }
   }
   
   // .get basic info about the series
   this->_basicInfo();
   
   // .process time series homogenization
   this->_processChangePointDetection();
   
   // .if request -> plot data
   if(_plot == "on") {
      
      this->_plotTimeSeries();
   }
}

/*
 *  .private functions
 */

// .function sumarizes basis info about the analysed time series
void t_appDetection::_basicInfo()
{
   if (_fmt == "td" && !_Data.empty()) {
      
      _firstRecord = _Data.begin()->first;
      _lastRecord = _Data.rbegin()->first;
      _originalSize = _Data.size();
   }
   else if (_fmt == "dd" && !_data.empty()) {
      
   }
   else {
      // warning
   }
   
}

//  .function prepares data for further using!
int t_appDetection::_prepareData()
{
   
   LOG1(":...t_appDetection::......Data preparation!");
   
   for(m_td::iterator it = _Data.begin(); it != _Data.end(); ++it) {
      
      _timeData.push_back(it->first);
   }
   
   // .convert data from string, double to the double, double
   t_fromTDtoDD fromTDtoDD; _data = fromTDtoDD.FromTDtoDD(_Data);
      
   if(!_data.empty()) {
      
      LOG1(":...t_appDetection::......Data transformation was successful. Data size: ", _data.size());
      _coredata->addData("origval", _data);
   } else {
      
      ERR(":...t_appDetection::......Data container is empty!"); return -1;
   }
   
   return 0;
}

// .manages the figures ploting
int t_appDetection::_plotTimeSeries()
{
   
   LOG1(":...t_appDetection::......Request for plot!");
   
   t_plot mplot;
   
   // .prepare data for plots creation
   if ( _data.empty() ||
	_deseas.empty() ||
	_TK.empty() ) {
      
      ERR(":...t_appDetection()::......Problem with plot creating!");
   }
   
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
   
   // .plot
   mplot.hTime();
   mplot.hDeseas();
   mplot.hTK();
   
   LOG1(":...t_appDetection::......Plots are created (time-stamp::double)!");
   
   return 0;
}


// .function manages the change point detection process
int t_appDetection::_processChangePointDetection()
{

   // . change point protocol;
   ofstream cfile(_lst.c_str(), ios::out | ios::app);
   // cfile << "\n# Station  Index  Median  Date  Shift\n";
   
   // .output setting
   ofstream ofile(_out.c_str(), ios::out | ios::app);
   ofile << "\n";
   ofile << "# ******************************** \n";
   ofile << "#   Change point detection results \n";
   ofile << "# ******************************** \n";
   ofile << "\n# First record:        " << _firstRecord  << endl;
   ofile << "# Last record:         " << _lastRecord   << endl;   
   ofile << "# File folder:         " << _inpSett [1]  << endl;
   ofile << "# File format:         " << _inpSett [2]  << endl;
   ofile << "# Data size:           " << _originalSize << endl;
   ofile << "# Time resolution [s]: " << _res          << "\n" << endl;
   
   // .results on screen
   cout << "# ******************************** \n";
   cout << "#   Change point detection results \n";
   cout << "# ******************************** \n";
   cout << "\n# First record:        " << _firstRecord  << endl;
   cout << "# Last record:         " << _lastRecord   << endl;
   cout << "# Data size:           " << _originalSize << endl;
   cout << "# Time resolution [s]: " << _res          << endl;


   // first epo, last epo, no of years
   // .pointers to change point 
   t_changePoint * changePoint = 0;
   t_adjustSeries * adjustSeries = 0;
   
   // .if requested, get deseasonalised data to change point detection process
   if ( _regressOnOff == "on" || _medianOnOff == "on") {
      
      _deseas = _coredata -> getData("elimSeasonal");
      
      this -> _checkDeseasData(); _adjustString = "elimSeasonal";
   } else {
      
      WARN1(":...t_appDetection::......Seasonal model is not removed from the original data!");
      
      _deseas = _data;
      
      this -> _checkDeseasData();  _adjustString = "origval";
   } 
   
   /*
    * .change point detection object declaration.
    */
   
   // .functions returns results of change point detection procedure
   changePoint = new t_changePoint( _setting, _coredata );
   
   // .get results
   _suspectedChangeIdx = changePoint->getIdxMaxPOS(); // .estimated index of change point
   _shift = changePoint->getShift(); // .estimated value of shigt
   _CV = changePoint->getCritVal(); // .estimated critical value
   _acf = changePoint->getACF(); // .estimated autocorrelation coefficient (lag = 1)
   _pVal = changePoint->getPValue(); // .estimated p-value
   _maxTk = changePoint->getMaxTK(); // .estimated max vlaue of Tk statistic
   _result = changePoint->getResult(); // .get result - stationary or non-stationary
   _TK = changePoint->getTK(); // .returns vector of Tk statistics
   
   LOG1(":...t_appDetection::......Loop: 0");
   
   if ( _result == "non-stationary" ) {
      
      // .store change points into the specific containers
      //   .lsit of shifts
      _listOfShifts.push_back(changePoint->getShift());
	
      //   .list of potential change points epochs/possitions given as indexes
      _listOfChpsIdx.push_back(_suspectedChangeIdx -1);
      _listOfChpsString.push_back( (_timeData[_suspectedChangeIdx -1]).timeStamp() );
      t_timeStamp convMjd((_timeData[_suspectedChangeIdx -1]).timeStamp());

      //   .list of potential change points epochs/positions given as MJD (Modified Julian Date)
      _listOfChpsMJD.push_back( convMjd.mjd() );
      
      t_timeStamp convLow(( _timeData[ changePoint->getLowConfInterIdx() -1 ] ).timeStamp());
      t_timeStamp convUpp(( _timeData[ changePoint->getUppConfInterIdx() -1 ] ).timeStamp());          
      ofile
      	<< "\n# TS beg [idx]       " << 0
	<< "\n# TS end [idx]       " << _timeData.size()
	<< "\n# Chp [idx]          " << _suspectedChangeIdx
	<< "\n# Chp [Stamp/MJD]    " << ( _timeData[ _suspectedChangeIdx -1 ] ).timeStamp() << "  "  << convMjd.mjd()
	<< "\n# Shift [-]          " << fixed << setprecision(2) << changePoint->getShift()
	<< "\n# Critical value     " << changePoint->getCritVal()
	<< "\n# Max Tk             " << changePoint->getMaxTK()
	<< "\n# ACF[1]             " << changePoint->getACF()  
	<< "\n# P-Value            " << changePoint->getPValue()
	<< endl;
      
      
      /*
       * .multi-change point detection procedure
       */
      
      vector<int> idxVec;
      idxVec.push_back(0);
      idxVec.push_back(_suspectedChangeIdx);
      idxVec.push_back(_originalSize);
      
      m_ii inter; 
      inter = this->_prepareIntervals(idxVec);
      
      int iNo = 1;  
      while (!inter.empty()) {
	 
	 LOG1(":...t_appDetection::......Loop: ", iNo);
	 
	 for ( m_ii::iterator it = inter.begin(); it != inter.end(); ++it) {
      
	    int iBeg = it->first; // .sub-series begin
	    int iEnd = it->second; // .sub-series end
	    
	    // .if subseries dimension is less than 300 records, then skip the change point detection process
	    // Constant may be configurated in the future
	    if ((iEnd - iBeg)<300) {
	    
	       _statInter[iBeg] = iEnd;
	       
	       // .is subseries is stationary, then remove the first index of stationary interval.
	       inter.erase(iBeg);
	       idxVec.erase(remove(idxVec.begin(), idxVec.end(), iBeg), idxVec.end()); //?
	    }else{
	       
	       // .change point detection in defined sub-series that is defined by the idx interval <iBeg, iEnd>
	       changePoint = new t_changePoint( _setting, _coredata, iBeg, iEnd );
	       
	       // .if subseries is stationary, save the results and remove first index of actually analysed subseries interval
	       if ( changePoint->getResult() == "stationary" ) {
		  
		  _statInter[iBeg] = iEnd;
//		  inter.erase(iBeg);
		  idxVec.erase(remove(idxVec.begin(), idxVec.end(), iBeg), idxVec.end()); //?

	       }else{
		     
		  // .suspected change point detected within the interval <iBeg, iEnd>
		  int suspectedPointIdx = changePoint->getIdxMaxPOS();
		  suspectedPointIdx = suspectedPointIdx + iBeg;
		  
		  // .seve new change point into the specific containers
		  if (find(idxVec.begin(), idxVec.end(), suspectedPointIdx) != idxVec.end()) { continue; }
		  else {idxVec.push_back(suspectedPointIdx);}

		  if (find(_listOfChpsIdx.begin(), _listOfChpsIdx.end(), suspectedPointIdx) != _listOfChpsIdx.end()) { continue; }
		  else {_listOfChpsIdx.push_back(suspectedPointIdx -1);}

		  if (find(_listOfChpsString.begin(), _listOfChpsString.end(), (_timeData[suspectedPointIdx -1]).timeStamp()) != _listOfChpsString.end()) { continue; }
		  else {_listOfChpsString.push_back( (_timeData[suspectedPointIdx -1]).timeStamp() );}
		  
		  t_timeStamp convMjd((_timeData[suspectedPointIdx -1]).timeStamp());
		  if (find(_listOfChpsMJD.begin(), _listOfChpsMJD.end(),  convMjd.mjd()) != _listOfChpsMJD.end()) { continue; }
		  else {  _listOfChpsMJD.push_back( convMjd.mjd() );  }
		  

//		  _listOfChpsIdx.push_back(suspectedPointIdx-1);
//		  _listOfChpsString.push_back( (_timeData[suspectedPointIdx -1]).timeStamp() );
//		  t_timeStamp convMjd((_timeData[suspectedPointIdx -1]).timeStamp());
//		  _listOfChpsMJD.push_back( convMjd.mjd() );
		  
		  // ToDo: confidence intervals estimation must be improved
		  ofile
		    << "\n# TS beg [idx]    " << iBeg
		    << "\n# TS end [idx]    " << iEnd
		    << "\n# Chp [idx]       " << suspectedPointIdx
		    << "\n# Chp [Stamp/MJD] " << ( _timeData[ suspectedPointIdx -1 ] ).timeStamp()                        << "  " << convMjd.mjd()
		    << "\n# Shift [-]       " << fixed << setprecision(2) << changePoint->getShift()
		    << "\n# Critical value  " << changePoint->getCritVal()
		    << "\n# Max Tk          " << changePoint->getMaxTK()
		    << "\n# ACF[1]          " << changePoint->getACF()  
		    << "\n# P-Value         " << changePoint->getPValue()
		    << endl;
		  
		  LOG1(":...t_appDetection::......Analysed time series [from: ", iBeg, " to: ", iEnd, " with result of detection: ", changePoint->getResult(), "]");
	       }
	    }
	 }
	 
	 // .prepare updated intervals for sub-time series.
	 inter = this->_prepareIntervals( idxVec );
	 
	 iNo++;
      } 
   } 
   else {
      
      _result == "stationary";
   }

   // .list of change points
   // .output, list of change points. Setting
   
   // .general protocol
   ofile << "\n# List of detected change point(s) [idx/mjd/time stamp]:\n";
   cout  << "\n# List of detected change point(s) [idx/mjd/time stamp]:\n";
   for (int i = 0; i<_listOfChpsString.size(); i++) {

      // .chp protocol
      cout <<  _setting->getActualStation() << "  " << _listOfChpsIdx[i] << "  " << _listOfChpsMJD[i] << "  " << _listOfChpsString[i] << "  " << _listOfShifts[i] << endl;
      cfile <<  _setting->getActualStation() << "  " << _listOfChpsIdx[i] << "  " << _listOfChpsMJD[i] << "  " << _listOfChpsString[i] << "  " << _listOfShifts[i] << endl;


      // .general protocol
      ofile <<  _listOfChpsIdx[i] << "  " << _listOfChpsMJD[i] << "  " << _listOfChpsString[i] << endl;
      cout  << setw(5) << _listOfChpsIdx[i] << "  " 
	    << setw(5) << _listOfChpsMJD[i] << "  " 
	    << setw(5) << _listOfChpsString[i] 
	<< endl;
   }
   
   // .delete pointers
   if ( changePoint  ) delete changePoint;
   if ( adjustSeries  ) delete adjustSeries;
   
   return 0;
} 

// .check deseasonalised data. 
// ToDo. Momentalne len skontroluje, ze ci nie je prazdna. Over este predpokladanu velkost v zavislosti na time resolution
int t_appDetection::_checkDeseasData() 
{
   
   if ( _deseas.empty() ) {
      
      ERR(":......t_appDetection......No cleaned data to change-point detection!");
   } else {
      
      LOG1(":......t_appDetection......Loaded cleaned data to change-point detection!");
   }
   
   return 0;
}


// .function returns intervals of subseries
map<int, int> t_appDetection::_prepareIntervals( vector<int>& idxVec )
{
   
   sort(idxVec.begin(), idxVec.end());

   m_ii intervals;
     
   for (int i = 0; i<idxVec.size()-1; ++i) {

      int beg = idxVec[i];
      int end = idxVec[i+1];

      // .container of non-stationary intervals
      intervals[beg] = end;
   }
 
   return intervals;
}