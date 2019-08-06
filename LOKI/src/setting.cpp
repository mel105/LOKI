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

#include "logger.hpp"
#include "proho.h"
#include "setting.h"
#include "workingdir.h"

// constructor
// --------------------
t_setting::t_setting()
{
  /// Get address of current working dir.
  t_workingDir workingDir;  string current_working_dir = workingDir.GetCurrentWorkingDir();
  
  /// PROHO Declaration
  t_proho proho;
  
  /// Open configure file proho.json
  proho.OpenFile((current_working_dir + "/PROHO/res/proho.json").c_str());
  
  /// PROHO::general
  string plotOnOff {proho.getValue<string>({ "general", "plotOnOff"})}; _plotOnOff = plotOnOff;
  
  /// PROHO::input
  string inputName {proho.getValue<string>({"input", "inputName"})}; _loadSetting.push_back(inputName);
  string inputFormat {proho.getValue<string>({"input", "inputFormat"})}; _loadSetting.push_back(inputFormat);
  string inputFolder {proho.getValue<string>({"input", "inputFolder"})}; _loadSetting.push_back(inputFolder);
  bool inputConvTdDd {proho.getValue<bool>({"input", "inputConvTdDd"})}; _inputConvTdDd = inputConvTdDd;
  double inputResolution {proho.getValue<double>({"input", "inputResolution"})}; _inputResolution = inputResolution;
  
  /// PROHO::output
  string outputName {proho.getValue<string>({"output", "outputName"})}; _outputName = outputName;
  string outputHist {proho.getValue<string>({"output", "outputHist"})}; _outputHist = outputHist;
  
  /// PROHO::stat
  string statOnOff {proho.getValue<string>({"stat", "statOnOff"})};  _statOnOff = statOnOff;
  double iqrCnfd   {proho.getValue<double>({"stat", "iqrCnfd"})};    _iqrCnfd   = iqrCnfd;
  
  /// PROHO::regression
  string regressOnOff {proho.getValue<string>({"regression", "regressOnOff"})}; _regressOnOff = regressOnOff;
  int regModel        {proho.getValue<int>({"regression", "regModel"})};        _regModel     = regModel;
  int regOrder        {proho.getValue<int>({"regression", "regOrder"})};        _regOrder     = regOrder;   
  bool elimTrend      {proho.getValue<bool>({"regression", "elimTrend"})};      _elimTrend    = elimTrend;
  bool elimSeas       {proho.getValue<bool>({"regression", "elimSeas"})};       _elimSeas     = elimSeas;
  
  /// PROHO::median
  string medianOnOff {proho.getValue<string>({"median", "medianOnOff"})}; _medianOnOff = medianOnOff;
  bool fixedHour     {proho.getValue<bool>({"median", "fixedHour"})};     _fixedHour   = fixedHour;
  int constHour      {proho.getValue<int>({"median", "constHour"})};      _constHour   = constHour;
  
  /// PROHO::reference
  string referenceOnOff {proho.getValue<string>({"reference", "referenceOnOff"})}; _referenceOnOff = referenceOnOff;
  
  /// PROHO::homogenization
  string homogenOnOff {proho.getValue<string>({"homogen", "homogenOnOff"})}; _homogenOnOff = homogenOnOff;
  double probCritVal {proho.getValue<double>({"homogen", "probCritVal"})}; _probCritVal = probCritVal;  
  double limitDepedence {proho.getValue<double>({"homogen", "limitDepedence"})}; _limitDepedence = limitDepedence;
  
  
  /// Log
  LOG2(":...t_setting::......Folder: ", inputFolder);
  LOG2(":...t_setting::......File: ",   inputName);
  LOG2(":...t_setting::......Format: ", inputFormat);
  LOG2(":...t_setting::......Plot: ",   plotOnOff);   
}

// general
string t_setting::getPlotOnOff() { return _plotOnOff; }
// input
vector <string> t_setting::getLoadSetting() { return _loadSetting; }
bool t_setting::getInputConvTdDd() { return _inputConvTdDd; }
double t_setting::getInputResolution() { return _inputResolution; }
// output   
string t_setting::getOutputName() { return _outputName; }
string t_setting::getOutputHist() { return _outputHist; }
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
// homogenization
string t_setting::getHomogenOnOff() { return _homogenOnOff; }
double t_setting::getProbCritVal() { return _probCritVal; }
double t_setting::getLimitDependence() { return _limitDepedence; }
