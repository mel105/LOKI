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

#include "logger.hpp"
#include "loki.h"
#include "setting.h"
#include "workingdir.h"

// constructor
// --------------------
t_setting::t_setting()
{
  /// Get address of current working dir.
  t_workingDir workingDir;  string current_working_dir = workingDir.GetCurrentWorkingDir();
  
  /// LOKI Declaration
  t_loki loki;
  
  /// Open configure file loki.json
  loki.OpenFile((current_working_dir + "/LOKI/res/loki.json").c_str());
  
  /// LOKI::general
  string plotOnOff {loki.getValue<string>({ "general", "plotOnOff"})}; _plotOnOff = plotOnOff;
  
  /// LOKI::input
  string inputName {loki.getValue<string>({"input", "inputName"})}; _loadSetting.push_back(inputName);
  string inputFolder {loki.getValue<string>({"input", "inputFolder"})}; _loadSetting.push_back(inputFolder);
  string inputFormat {loki.getValue<string>({"input", "inputFormat"})}; _loadSetting.push_back(inputFormat);
  int inputDataCol {loki.getValue<int>({"input", "inputDataCol"})}; _inputDataCol = inputDataCol;
  bool inputConvTdDd {loki.getValue<bool>({"input", "inputConvTdDd"})}; _inputConvTdDd = inputConvTdDd;
  double inputResolution {loki.getValue<double>({"input", "inputResolution"})}; _inputResolution = inputResolution;
  
  /// LOKI::output
  string outputName {loki.getValue<string>({"output", "outputName"})}; _outputName = outputName;
  string outputHist {loki.getValue<string>({"output", "outputHist"})}; _outputHist = outputHist;
  string outputList {loki.getValue<string>({"output", "outputList"})}; _outputList = outputList;
  
  /// LOKI::stat
  string statOnOff {loki.getValue<string>({"stat", "statOnOff"})};  _statOnOff = statOnOff;
  double iqrCnfd   {loki.getValue<double>({"stat", "iqrCnfd"})};    _iqrCnfd   = iqrCnfd;
  
  /// LOKI::regression
  string regressOnOff {loki.getValue<string>({"regression", "regressOnOff"})}; _regressOnOff = regressOnOff;
  int regModel        {loki.getValue<int>({"regression", "regModel"})};        _regModel     = regModel;
  int regOrder        {loki.getValue<int>({"regression", "regOrder"})};        _regOrder     = regOrder;   
  bool elimTrend      {loki.getValue<bool>({"regression", "elimTrend"})};      _elimTrend    = elimTrend;
  bool elimSeas       {loki.getValue<bool>({"regression", "elimSeas"})};       _elimSeas     = elimSeas;
  
  /// LOKI::median
  string medianOnOff {loki.getValue<string>({"median", "medianOnOff"})}; _medianOnOff = medianOnOff;
  bool fixedHour     {loki.getValue<bool>({"median", "fixedHour"})};     _fixedHour   = fixedHour;
  int constHour      {loki.getValue<int>({"median", "constHour"})};      _constHour   = constHour;
  
  /// LOKI::reference
  string referenceOnOff {loki.getValue<string>({"reference", "referenceOnOff"})}; _referenceOnOff = referenceOnOff;
  
  /// LOKI::detectionization
  string detectionOnOff {loki.getValue<string>({"detection", "detectionOnOff"})}; _detectionOnOff = detectionOnOff;
  double probCritVal {loki.getValue<double>({"detection", "probCritVal"})}; _probCritVal = probCritVal;  
  double limitDepedence {loki.getValue<double>({"detection", "limitDepedence"})}; _limitDepedence = limitDepedence;

  /// Log
  LOG2(":...t_setting::......Folder: ", inputFolder);
  LOG2(":...t_setting::......File: ", inputName);
  LOG2(":...t_setting::......Format: ", inputFormat);
  LOG2(":...t_setting::......Column: ", inputDataCol);   
  LOG2(":...t_setting::......Plot: ", plotOnOff);   
}

/// SET ACTUAL STATION
void t_setting::setActualStation(string st){ _st = st; }


// *** GET FUNCTIONS *** //

// general
string t_setting::getPlotOnOff() { return _plotOnOff; }
string t_setting::getActualStation(){ return _st; }


// input
vector <string> t_setting::getLoadSetting() { return _loadSetting; }
int t_setting::getInputDataCol() { return _inputDataCol; }
bool t_setting::getInputConvTdDd() { return _inputConvTdDd; }
double t_setting::getInputResolution() { return _inputResolution; }

// output   
string t_setting::getOutputName() { return _outputName; }
string t_setting::getOutputHist() { return _outputHist; }
string t_setting::getOutputList() { return _outputList; }

// statistics
string t_setting::getStatOnOff() { return _statOnOff; }
double t_setting::getIqrCnfd() { return _iqrCnfd; }

// regression
string t_setting::getRegressOnOff() { return _regressOnOff; }
int    t_setting::getRegModel() { return _regModel; }
int    t_setting::getRegOrder() { return _regOrder; }
bool   t_setting::getElimTrend() { return _elimTrend; } 
bool   t_setting::getElimSeas() { return _elimSeas; }

// median
string t_setting::getMedianOnOff() { return _medianOnOff; }
int    t_setting::getConstHour() { return _constHour; }
bool   t_setting::getFixedHour() { return _fixedHour; }

// reference
string t_setting::getReferenceOnOff() { return _referenceOnOff; }

// detectionization
string t_setting::getDetectionOnOff() { return _detectionOnOff; }
double t_setting::getProbCritVal() { return _probCritVal; }
double t_setting::getLimitDependence() { return _limitDepedence; }
