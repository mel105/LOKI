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

#include "autoCov.h"
#include "changePoint.h"
#include "convTDtoDD.h"
#include "logger.hpp"
#include "plot.h"
#include "stat.h"

using namespace NEWMAT;

// contructor
// -----------
t_changePoint::t_changePoint(t_setting* setting,  t_coredata* coredata)
{
  
  // Default setting
  _resultOfStationarity = "non-stationary";
  _shift = 999.999;
  
  /// Get setting & create log info
  double probCritVal = setting->getProbCritVal(); _prob = probCritVal;
  double limitDependence = setting->getLimitDependence(); _limitDependence = limitDependence;
  string regressOnOff = setting->getRegressOnOff(); 
  string medianOnOff = setting->getMedianOnOff(); 
  string referenceOnOff = setting->getReferenceOnOff(); 
  
  LOG1(":......t_changePoint::......Loaded homogen settings!");
  
  if ( regressOnOff == "on" || medianOnOff == "on") {
    
    // request to process the data without seasonal model.
    _data = coredata -> getData("elimSeasonal");
  }
  else if ( referenceOnOff == "on" ) {
    
    // request to process the data without seasonal model.
    //_data = coredata -> getData("elimSeasonal");
    // ked budem mat hotove, tak hornu podmienku len rozsirim.
  }
  else {
    
    // request to process the original data (even with the seasonal model).
    _data = coredata -> getData("origval");
  }
  
 
  if(!_data.empty()){
    
    LOG1(":......t_changePoint::......Loaded data container. Data size: ", _data.size());
  }
  else{
    
    ERR(":......t_changePoint::......Data container is empty!"); return;
  }

  // process change point detection method
  this->_detectChangePoint();
}

t_changePoint::t_changePoint(t_setting* setting,  t_coredata* coredata, int& iBeg, int& iEnd)
{
  
  // Default setting
  _resultOfStationarity = "non-stationary";
  _shift = 999.999;
  
  /// Get setting & create log info
  double probCritVal = setting->getProbCritVal(); _prob = probCritVal;
  double limitDependence = setting->getLimitDependence(); _limitDependence = limitDependence;
  
  string regressOnOff = setting->getRegressOnOff(); 
  string medianOnOff = setting->getMedianOnOff(); 
  string referenceOnOff = setting->getReferenceOnOff(); 
  
  if ( regressOnOff == "on" || medianOnOff == "on") {
    
    // request to process the data without seasonal model.
    _data = coredata -> getData("elimSeasonal");
  }
  else if ( referenceOnOff == "on" ) {
    
    // request to process the data without seasonal model.
    //_data = coredata -> getData("elimSeasonal");
    // ked budem mat hotove, tak hornu podmienku len rozsirim.
  }
  else {
    
    // request to process the original data (even with the seasonal model).
    _data = coredata -> getData("origval");
  }
  
  // get only the data within the interval <iBeg, iEnd>;
  m_dd tData;
  int idx = 0;
  for (m_dd::iterator iter = _data.begin(); iter != _data.end(); ++iter) {
    
    if ( idx > iBeg && idx < iEnd) {
      
      tData[iter->first] = iter->second;
    }
    idx++;
  }
  
  _data.clear();
  _data = tData;
  tData.clear();
  
  if(!_data.empty()){

    LOG1(":......t_changePoint::......Loaded data container. Data size: ", _data.size());
  }
  else{
    
    ERR(":......t_changePoint::......Data container is empty!"); return;
  }
  
//  cout << "am I here? " << _data.size() << endl;
  
  this->_detectChangePoint();
  /*
  if (_data.size() > 300 ) {

    // process change point detection method
    this->_detectChangePoint();
  } else {
    
    cout << " asi nema cenu pocitat change point, ak je rozmer rady maly." << endl;
  }
   */ 
}

// === PUBLIC FUNCTIONS ===
double t_changePoint::getShift()
{

  if ( _shift != 999.999) {
     
     return _shift;
  }
  else {
    
    ERR(":......t_changePoint::......No shift was estimated!");
    return -1;
  }
}

// -
double t_changePoint::getCritVal()
{
  
  if (_criticalVal <= 0 ) {
    
    ERR(":......t_changePoint()::......CriticalVal is not estimated!");
    return -1;
  }
  else {
    
    return _criticalVal;
  }
}

// -
double t_changePoint::getMaxTK() {
  
  return _maxTK;
}

// -
map<double, double> t_changePoint::getTK()
{
  
  m_dd TK;
  
  if(_TK.size() != _data.size()) { 
    
    WARN1(":......t_changePoint()::......TK size is not the same as data size!");
    _TK[_data.size()] = 0.0;
  }
  
  int idx = 0;
  
  for(m_dd::iterator i = _data.begin(); i != _data.end(); ++i ) {
    
    TK[i->first] = _TK[idx];

    idx++;
  }
  
  if (TK.empty()) { ERR(":......t_changePoint()::......TK is empty!"); }
  
  return TK;
}

// -
double t_changePoint::getACF()
{
  
  if (_acf < -1.0 ||
      _acf >  1.0 ) {
    
    ERR(":......t_changePoint()::......ACF is out of interval<-1,1>!");
    return -1;
  }
  else {
    
    return _acf;
  }
}

// -
double t_changePoint::getPValue()
{
  
  return _PValue;
}

// -
int t_changePoint::getIdxMaxPOS()
{
  
  if (_idxMaxPOS <= 0 ||
      _idxMaxPOS >= _dataVec.size()) {
    
    ERR(":......t_changePoint()::......IdxMaxPOS is not explored!");
    return -1;
  }
  else {
    
    return _idxMaxPOS;
  }
}

// -
string t_changePoint::getResult()
{
  
  return _resultOfStationarity;
}

double t_changePoint::getLowConfInterIdx()
{

  double confIdx = 0;
  
  if (_lowConfIdx > 0) {
    
    confIdx = _lowConfIdx;
  }
       
  return confIdx;
}

double t_changePoint::getUppConfInterIdx()
{

  double confIdx = _data.size();
  
  if (_uppConfIdx < _data.size()) {
    
    confIdx = _uppConfIdx;
  }
         
  return confIdx;
}

// === PROTECTED FUNCTIONS ===
// -
void t_changePoint::_detectChangePoint()
{

//  cout << "DB 0" << endl;
  /// The dependecy estimation: full time series
  this->_estimateDependency();
//  cout << "DB 1" << endl;
  /// Estimate T_k statistics!
  this->_estimateTStatistics();
//  cout << "DB 2" << endl;
  /// Critical values estimation: calculated by the asymptotic distribution:  
  this->_estimateCritValue();
//  cout << "DB 3" << endl;
  /// Estimate Means; before & after
  this->_estimateMeans();
//  cout << "DB 4" << endl;
  /// Estimate shift
  this->_estimateShift();
//  cout << "DB 5" << endl;
  /// Estimate Sigma*L (see Antoch et al. (1995))
  //  Warning: After the estimateSignaStarL method calling, we'll get new vector _dataVal, 
  //           that contains vals repaired by shift and centered to zero.
  this->_estimateSigmaStarL();
//  cout << "DB 6" << endl;
  /// Estimate p-Value
  this->_estimatePValue();
//  cout << "DB 7" << endl;
  /// Test H0 hypothesis
  this->_testHypothesis();
//  cout << "DB 8" << endl;
  if(_resultOfStationarity == "non-stationary") {
    
    this->_estimateConfidenceInterval();
  }
}

// -
void t_changePoint::_testHypothesis()
{
  
//#ifdef DEBUG
  cout << fixed << setprecision(5)
     << " _testHypothesis\n"
     << " \n_maxTK" << _maxTK
     << " \n_criticalVal " << _criticalVal << endl;
//#endif
  
  if(_maxTK > _criticalVal) {
    
    _resultOfStationarity = "non-stationary";                
  }
  else {
    
    _resultOfStationarity = "stationary";
  }
}


// -
void t_changePoint::_estimateConfidenceInterval()
{
  
  double alphaCrit = 999.0;
  
  if (_prob ==  0.9) {
    
    alphaCrit = 4.696;
  }
  else if (_prob ==  0.95) {
    
    alphaCrit = 7.687;
  }
  else if (_prob ==  0.975) {
    
    alphaCrit = 11.033;
  }
  
  else if (_prob ==  0.99) {
    
    alphaCrit = 15.868;
  }
  
  else if (_prob ==  0.995) {
    
    alphaCrit = 19.767;
  }
  else {
    
    alphaCrit = 4.0;
  }
  
  //double mValue = ( ( alphaCrit * pow( _SK[_idxMaxPOS], 2.0)) /
  //                  pow(_shift, 2.0 ) ) + _idxMaxPOS;
  
  double mValue = ( ( alphaCrit * pow( _SK[_idxMaxPOS], 2.0)) /
                    pow(_shift, 2.0 ) );
  
  _uppConfIdx = _idxMaxPOS + floor(mValue);
  _lowConfIdx = _idxMaxPOS - floor(mValue);
  
  cout << "changePoint " << _lowConfIdx << "  " << _idxMaxPOS << "  " << _uppConfIdx << endl;
}
// -
void t_changePoint::_estimatePValue()
{
  
  int n = _dataVec.size(); double N = static_cast<double>(n);
  
  double Tn2norm = _maxTK / _sigmaStar;
  
  double an = sqrt(2.0 * log(log(N)));
  double bn = 2.0 * log(log(N)) + 0.5 * log(log(log(n))) - 0.5 * log(_PI);
  
  double  y = (an * Tn2norm) - bn; //double Y = -1.0*y;
  
  _PValue = 1.0 - exp(-2.0 * exp(-y));
  
#ifdef DEBUG
  cout << fixed << setprecision(5)
     << "\n\n_estimatePValue\n" 
     << "\n n " << n
     << "\n_maxTK " << _maxTK
     << "\n_sigmaStar " << _sigmaStar
     << "\nTn2norm " << Tn2norm
     << "\nan " << an
     << "\nan " << bn
     << "\ny " << y 
     << "\nPVAL " << _PValue << endl;
#endif
  
}

// -
void t_changePoint::_estimateSigmaStarL()
{
  
  vector<double> beforeVals;
  vector<double> VALSIGMA;
  vector<double> ACF;
  
  for(unsigned int i = 0; i < _idxMaxPOS-1; i++) {
    
    VALSIGMA.push_back(_dataVec[i]);
    
    double actualVal = VALSIGMA[i] - _meanBefore;
    replace(VALSIGMA.begin(), VALSIGMA.end(), VALSIGMA[i], actualVal);
  }
  
  for(unsigned int j = _idxMaxPOS; j < _dataVec.size(); j++) {
    
    VALSIGMA.push_back(_dataVec[j]);
    
    double actualVal = VALSIGMA[j] - _meanAfter;
    replace(VALSIGMA.begin(), VALSIGMA.end(), VALSIGMA[j], actualVal);
  }
  
  int    n = VALSIGMA.size();
  double L = pow(static_cast<double>(n), (1.0 / 3.0)); L = ceil(L);
  
  // get autocorrelation functions of reduced orig. vals.
  t_autoCov autoCov(VALSIGMA); ACF = autoCov.getAutoCov(); 
  
  // get sigma*           new _acf (after the reduction)
  double f0est = ACF[1]; _acf = f0est;
  
  for(size_t i = 0; i < L; i++) {
      
    double wght = 1.0 - ( ( i + 1 ) / L );
    f0est = f0est + ( 2.0 * wght * ACF[i+1] * ACF[0] );
  }
  
  _sigmaStar = sqrt(abs(f0est));
  
#ifdef DEBUG
  /// Note: 
  /// a1 & a2   are estimates of Yule - Walker equations.  
  /// f0esttrue is theoretical value of spectral density in lag 0
  double a1 = -1.0 * (ACF[1]) * (ACF[0] - ACF[2]) / ( pow(ACF[0], 2.0) - pow(ACF[1], 2.0) );
  double a2 = -1.0 * (ACF[0] * ACF[2] - pow(ACF[1], 2.0)) / ( pow(ACF[0], 2.0) - pow(ACF[1], 2.0) );
  double f0esttrue = 1.0 / pow( (1.0 - a1 - a2), 2.0 );
  
  cout 
     << " Yule-Walker  " << a1 << "  "  << a2 
     << " f0est        " << f0est 
     << " f0esttrue    " << f0esttrue 
     << " sigma*       " << _sigmaStar <<  endl;
#endif
  
  // get updated critical value, if _acf > _limidDepedence

  if(_acf > _limitDependence) {
    
    _criticalVal = _criticalVal * _sigmaStar;
    
#ifdef DEBUG
    cout 
       << " _limit       " << _limitDependence
       << "\n _acf       " << _acf
       << "\n _mcoef     " << _mcoef
       << "\n _sigmaStar " << _sigmaStar
       << "\n _critVal   " << _criticalVal << endl;
#endif
  }
  
}

// -
void t_changePoint::_estimateShift()
{
   
  //_estimateMeans(); // v orig mam nezakomentovany. Overi si, ze preco najprv v main processe a potom aj tu
  _shift = (_meanAfter - _meanBefore);
  
#ifdef DEBUG
  cout << " ESTIMATED SHIFT " << _shift << endl;
#endif
}

// -
void t_changePoint::_estimateMeans()
{
  
  vector<double> data_to_k;
  vector<double> data_af_k;
  
  //cout << _idxMaxPOS << " <<== Hodnota idxMaxPOS | _dataVec.size ==>  " << _dataVec.size() << endl;
  
  for(unsigned int i = 0; i < _idxMaxPOS-1; i++) {
    
    data_to_k.push_back(_dataVec[i]);
  }
  
  for(unsigned int j = _idxMaxPOS; j < _dataVec.size(); j++) {
    
    data_af_k.push_back(_dataVec[j]);
  }
  

  t_stat before(data_to_k); before.calcMean(); _meanBefore = before.getMean();
  t_stat  after(data_af_k);  after.calcMean(); _meanAfter  = after.getMean();
  
#ifdef DEBUG
  cout << "Mean bf: " << _meanBefore << " Mean af: " << _meanAfter << endl;
#endif 
  
}

// -
void t_changePoint::_estimateCritValue()
{
  
  if(_prob <= 0.0 || _prob > 1.0) {
    
    WARN1(":......t_changePoint::......Parameter _prob is out of interval <0:1>. It was used default value: _prob = 0.95");
    _prob = 0.95;
  }
  
  double a_n = 1.0 / sqrt( 2.0 * log(log(_data.size())) );
  double b_n = 1.0 / a_n + ( a_n / 2.0 ) * log(log(log(_data.size())));
  double crvl = -log(-(sqrt(_PI) / 2.0) * log(_prob));
  
  _criticalVal = (crvl*a_n) + b_n;
  
#ifdef DEBUG
  cout << "cv: " << _criticalVal << " at " << _prob << endl;
#endif
}


// -
void t_changePoint::_estimateTStatistics()
{
  
  int N = _data.size(); double NN = static_cast<double>(N);
  int k = 1;
  
  for ( m_dd::iterator i = _data.begin(); i != _data.end(); i++) {
    
    _dataVec.push_back(i->second); 
  }
  
  t_stat stat(_dataVec); stat.calcMean(); double actualMean = stat.getMean();
  
  map<double, double> TK;
  
  while(k <= N-1) {
        
    double kk = static_cast<double>(k);
    
    double sum_k = 0.0;  for ( int i = 0; i < k; ++i ) { sum_k += _dataVec[i]; }
    
    double x_k  = sum_k / k;
    
    double sumK = 0.0; for ( int iDat = 0; iDat<N; ++iDat ) { sumK += pow(_dataVec[iDat] - x_k, 2.0); }
    
    double sum_n = 0.0;  for ( int j = k; j < N; ++j) { sum_n += _dataVec[j]; }
    
    double x_n = sum_n / ( N - k );
    
    double sumN = 0.0; for ( int iDat = 0; iDat<N; ++iDat ) { sumN += pow(_dataVec[iDat] - x_n, 2.0); }
    
    double sk = sqrt( (sumK + sumN) / (N-2) );  _SK.push_back(sk);
    
    double actualTK = sqrt( ((NN - kk) * kk) / NN ) * abs(x_k - x_n) * (1 / sk);
    
    _TK.push_back(actualTK);
    TK[actualTK] = k;
    
    k++;
  }
  
  if(TK.rbegin() != TK.rend()) {
    
    _maxTK     = TK.rbegin()->first;
    _idxMaxPOS = TK.rbegin()->second + 1;
    
#ifdef DEBUG
    cout << "Max tk " << _maxTK << " at " << _idxMaxPOS << endl;
#endif
  }
}



/*  
// Implementacia podla prof. 
void t_changePoint::_estimateTStatistics()
{
  
  int N = _data.size(); double NN = static_cast<double>(N);
  int k = 0;
  
  // get mean
  //vector<double> dataVec;
  for ( m_dd::iterator i = _data.begin(); i != _data.end(); i++) {
    
    _dataVec.push_back(i->second); 
  }
  
  t_stat stat(_dataVec); stat.calcMean(); double actualMean = stat.getMean();
  
  // Needed for TK sorting
  map<double, double> TK;
  
  while(k <= N-2) {
    
    double kk = static_cast<double>(k); kk = kk + 1.0;
    
    //double constant = 0.0; constant = sqrt( ( kk * ( NN - kk ) ) / NN ); /// pomaha mi v pripade JB time series
    double constant = 0.0; constant = sqrt( NN /  ( kk * ( NN - kk ) ) ); /// standardne tropo series
    //double constant = 0.0; constant = sqrt( ( NN - kk ) * kk / NN ); /// standardne tropo series
    
    double cumsum = 0.0; // _VAL[0]; // from left to right
    
    for(int i = 0; i <= k; ++i) {
      
      cumsum = cumsum + _dataVec[i];
    }
    
    double actualSK = cumsum - (kk * actualMean); 
    
    double actualTK = constant * abs(actualSK);
    //double actualTK = constant * actualSK;
    //double actualTK = constant * abs(actualSK) / _ACF[0];
    
    _TK.push_back(actualTK);
    
    TK[actualTK] = k;
    
    k++;
  }
  
  if(TK.rbegin() != TK.rend()) {
    
    _maxTK     = TK.rbegin()->first;
    _idxMaxPOS = TK.rbegin()->second + 1;
    
#ifdef DEBUG
    cout << "Max tk " << _maxTK << " at " << _idxMaxPOS << endl;
#endif
    
  }
}


*/


// -
void t_changePoint::_estimateDependency()
{
  
  // autocorrelation functions
  vector<double> xcov; 
  
  t_autoCov autoCov(_data); xcov = autoCov.getAutoCov(); 
  
#ifdef DEBUG   
  for(int i = 0; i < floor(xcov.size()/100.0); i++)
     cout << i << "  " << xcov[i] << endl;
#endif
  
  _acf   = xcov[1];
  _mcoef = sqrt( (1.0 + _acf) / (1.0 - _acf) );
  
#ifdef DEBUG
  cout << _acf << "  " << _mcoef << endl;
#endif
}