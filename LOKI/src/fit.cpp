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

#include "fit.h"
#include "stat.h"
#include "logger.hpp"


/// Constructor
t_fit::t_fit(const int & model, const map<double, double> & data, const int & order, const double & res)
{
  
  _model = model;
  _data = data;
  _order = order;
  _res = res;
  
  // - to lsq
  _eps   = 1e-2;      // convergence criterium for gradient
  _Niter = 100;       // max iterations
  _it    = 0;         // ini. iteration
  _stop  = 0;         // stop criterium   
  
  // - to fit stat
  _Sc  = 0.0;   
  _St  = 0.0; 
  _Se  = 0.0; 
  _SXX = 0.0;   
  _ym  = 0.0;  
  _sxx = 0.0;  
  _cd  = 999.99;
  _sr  = 999.99;
  _pSigma = 97.5; // konfig parameter?
  _up = 999.99;
  _tal = 999.99;
  
  // set and estimate coef.
  this->_setModel();
  
  // estimate stat.
  this->_fitStat();
}

// public
ColumnVector t_fit::getCoef(){ return _fit; }

/// @detail t_alpha
double t_fit::getTAlpha() { return _tal; }

/// @detail Sum square error
double t_fit::getSSE(){ return _Sc; }

/// @detail Residual sum square
double t_fit::getRSS(){ return _Se; }

/// @detail Coefficient of determiantion
double t_fit::getCD(){ return _cd; }

/// @detail Variance of residuals
double t_fit::getSR(){ return _sr; }

/// @detail Standard deviation of parameters estimation
vector<double> t_fit::getSigmaFit() { return _sigFit; }

/// @detail Confidence interval: low case
vector<double> t_fit::getConfIntLow() { return _confLow; }

/// @detail Confidence interval: upp case
vector<double> t_fit::getConfIntUpp() { return _confUpp; }

/// @detail T-Statistic
vector<double> t_fit::getTStat() { return _tStat; }

/// @detail P-Value
vector<double> t_fit::getPVals() { return _pVal; }

// protected
// ---------
void t_fit::_setPeriod()
{
  _period = 365.25;
  /*
  if(_res <= 0.0 || _res == 86400.0) {
    
    _period = 365.25;
  }
  else {
   
    double rat = 86400.0 / _res;
    // ToDo: Decide, how to setup the period.
    //_period = 365.25 * rat;
    _period = 365.25 ;
    
  }
   */ 
}

// 
void t_fit::_setModel()
{
  
  _fit.CleanUp();
  
  switch (_model) {
    
  case 1: {  
    
    // Linear regression model
    // Inicialization of fitter parameters.
    ColumnVector fit(_order+1);  for(int i = 1; i <= (_order+1); i++) { fit(i) = 1.0; } 
    
    // Call LSQ method
    this->_lsq(fit);
    
    // final set of fitted parameters
    _fit = fit;
    
    break;
  }
  case 2: {
    
    // Polynomial regression
    // Inicialization of fitter parameters.
    ColumnVector fit(_order+1);  for(int i = 1; i <= (_order+1); i++) { fit(i) = 1.0; } 
    
    // Call LSQ method
    this->_lsq(fit);
    
    // final set of fitted parameters
    _fit = fit;
    
    break;
  }
  case 3: {
    
    // Harmonic regression
    // y = A + B*t + C*sin(2*M_PI*t/P + Phi): P = fixed = 365.25
    
    // set time series period
    this->_setPeriod();

    // Inicialization of fitter parameters.
    ColumnVector fit(4);  for(int i = 1; i <= 4; i++) { fit(i) = 1.0; } 
    
    // Call LSQ method
    this->_lsq(fit);
    
    // final set of fitted parameters
    _fit = fit;

    break;
  }
  default: {      
    
    ERR(":......t_fit::......Requested regression model is not supported!");
    break;
    return (void) -1;
  }
  }
}

/// @Detials
///   -
int t_fit::_fitStat()
{
  
  t_stat statTime(_vX); statTime.calcMean(); double xmean = statTime.getMean(); /*cout << xmean << endl;*/
  t_stat statVals(_vY); statVals.calcMean(); double ymean = statVals.getMean(); /*cout << ymean << endl;*/
  
  size_t N = _vX.size();
  
  if (_model == 1 || _model == 2) {
    //-
    for ( int i = 0; i < N; i++) {
      
      double temp = _fit(1);
      
      for(int j = 1; j <= _order; j++) {
        
        temp += _fit(j+1) * pow(_vX[i], j);
      }
      
      _ym += temp;
    }
    
    _ym = _ym / N;
    
    //-
    for (m_dd::iterator I = _data.begin(); I != _data.end(); ++I) {
      
      double temp = _fit(1);
      
      for(int j = 1; j <= _order;  j++) {
        
        temp += _fit(j+1) * pow(I->first, j);
      }
      
      _St  += pow((temp - ymean), 2.0);
      _Sc  += pow((I->second - ymean), 2.0);
      _Se  += pow((I->second - temp),  2.0);
      
      // statistics
      _sxx += pow(I->first, 2.0);
      _SXX += pow((I->first - xmean), 2.0);
      
#ifdef DEBUG
      cout
         << fixed 
         << setprecision(4)
         << I->first
         << " ORIG " << I->second
         << " POLY " << temp
         << endl;
#endif
    }
  }
  else if (_model == 3) {
    
    // Harmonic regression
    // y = A + B*t + C*sin(2*M_PI*t/P + Phi): P = fixed = 365.25
    for ( int i = 0; i < N; i++) {
      
      double temp = _fit(1) + _fit(2) * _vX[i];
      
      for( int j = 3; j <= _fit.Nrows(); j=j+2) {
        
        temp +=  _fit(j) * sin((j-1) * M_PI * _vX[i] / _period + _fit(j+1));
      }
      
      _ym += temp;
    }
    
    _ym = _ym / N;
    
    //-
    for (m_dd::iterator I = _data.begin(); I != _data.end(); ++I) {
      
      double temp = _fit(1) + _fit(2) * I->first;
      
      for(int j = 3; j <= _fit.Nrows();  j=j+2) {
        
        temp += _fit(j) * sin((j-1) * M_PI * I->first / _period + _fit(j+1));
      }
      
      _St  += pow((temp - ymean), 2.0);
      _Sc  += pow((I->second - ymean), 2.0);
      _Se  += pow((I->second - temp),  2.0);
      
      // statistics
      _sxx += pow(I->first, 2.0);
      _SXX += pow((I->first - xmean), 2.0);
      
#ifdef DEBUG
      cout
         << fixed 
         << setprecision(4)
         << I->first
         << " ORIG " << I->second
         << " POLY " << temp
         << endl;
#endif
    }
  }
  else {
    ERR(":...t_fit::......Unknown regression model!");
  }
  
  // estimate coef. of deter
  this-> _calcCoefDeter();
  // estimate residual variance
  this-> _calcResVar();
  // estimate _up parameter
  this-> _UP();
  // t-stat critical value calculation
  this-> _calcTAlpha();
  // Fitted parameters standard deviation calculation
  this-> _sigmaFit();
  // Confidence intervals of fitted parameters estimation
  this-> _confidenceInterval();
  // t-stat estimation
  this-> _t_stat();
  // p-value estimation
  this-> _pValue();
  
  return 0;
}

//-
int t_fit::_UP()
{
  
  // Likes&Laga, str 4 tab 1B
  if( _pSigma == 68.3) {
    
    _up = 0.476104; // approx 1*sigma
  }
  else if( _pSigma == 95.0) {
    
    _up = 1.644854; //
  }
  else if( _pSigma == 95.5) {
    
    _up = 1.695398; // approx 2*sigma
  }
  else if( _pSigma == 97.5) {
    
    _up = 1.959964; //
  }
  else if( _pSigma == 99.0) {
    
    _up = 2.326348; // 
  }
  else if( _pSigma == 99.5) {
    
    _up = 2.575829; // 
  }
  else if( _pSigma == 99.7) {
    
    _up = 2.747781; // approx 3*sigma
  }
  else if( _pSigma == 99.9) {
    
    _up = 3.090232; //
  }
  else {
    
    ERR(":...t_fit::......Unknown 'up' value (100P% quantile of distribution)!");
    return -1;
  }
  
  return 0;
}

//- 
int t_fit::_sigmaFit()
{
  
  size_t N = _data.size();
  
  switch (_model) {
    
  case 1:
    
    // fit 1
    _sigFit.push_back( sqrt(_sr) * (sqrt(_sxx) / sqrt(N * _SXX)) );
    // fit 2
    _sigFit.push_back( sqrt(_sr) * ( 1.0 / sqrt( _SXX)) );
    break;
    
  case 2:
    
    for(auto i = 0; i< _DIAG.size(); i++) {
      
      _sigFit.push_back( sqrt(_sr) * sqrt(_DIAG[i]));
    }
    break;
    
  case 3:
    
    for(auto i = 0; i< _DIAG.size(); i++) {
      
      _sigFit.push_back( sqrt(_sr) * sqrt(_DIAG[i]));
    }
    break;
    
  default:
    
    return -1;
    break;
  }
  
  return 0;
}

//-
int t_fit::_calcTAlpha()
{
  
  if (_up == 999.99) {
    
    ERR("Error::fit.cpp::_calcTAlpha::Unknown _up");
    return -1;
  }
  else {
    
    // *** see Likes&Laga[1978], pp.17: t(alpha/2[N-2])
    size_t n = _data.size() - 2;
    double N = static_cast<double>(n);
    
    _tal = _up * ( 1.0 + (1.0 / (4.0 * N)) * (1.0 + _up * _up) +
                 ( 1.0 / (96.0 * N * N)) * (3.0 + (16.0 * _up * _up) + (5.0 * _up * _up * _up * _up)));
  }
  
  return 0;
}

//-
void t_fit::_calcCoefDeter()
{
  
  _cd = _St / _Sc;
}


//-
void t_fit::_calcResVar()
{
  
  size_t N = _data.size();
  
  _sr = _Se / (N - _order -1);
}

//-
int t_fit::_confidenceInterval()
{
  
  int m = _fit.Nrows();
  int n = _sigFit.size();
  
  if ( m != n) {
    
    ERR("Error::fit.cpp::_confidenceInterval::_fit vector is not the same as _sigmaFit");
    return -1;
  }
  else if(_tal == 999.99) {
    
    ERR("Error::fit.cpp::_confidenceInterval::Unknown _tal parameter");
    return -1;
  }
  else {
    
    for( int i = 0; i < n; i++) {
      
      _confLow.push_back( _fit(i+1) - (_tal * _sigFit[i]) );
      _confUpp.push_back( _fit(i+1) + (_tal * _sigFit[i]) );             
    }
  }
  
  return 0;
}


//-
int t_fit::_t_stat()
{
  
  int m = _fit.Nrows();
  int n = _sigFit.size();
  
  if ( m != n) {
    
    ERR("Error::fit.cpp::_t_stat::_fit vector is not the same as _sigmaFit");
    return -1;
  }
  else {
    
    for( int i = 0; i < n; i++) {
      
      _tStat.push_back( _fit(i+1) / _sigFit[i] );
    }
  }
  
  return 0;
}

//-
int t_fit::_pValue()
{
  
  size_t n = _data.size();
  double N = static_cast<double>(n-1);
  
  
  if ( n == 0) {
    
    ERR("Error::fit.cpp::_pValue::No t-statistics");
    return -1;
  }
  else {
    
    for( int i = 0; i < _tStat.size(); i++) {
      
      //*** p-value: reference: Abramowitz et al. pp 936
      double X = ( abs(_tStat[i]) * ( 1.0 - ( 1.0 / (4.0 * N)))) / ( sqrt( 1.0 + pow( abs(_tStat[i]), 2.0) / (2.0 * N)));
      double p = (2.0 * (1.0 - 0.5 * pow(( 1.0 + (0.196854 * X) + (0.115194 * pow(X, 2.0)) + (0.000344 * pow(X, 3.0)) + (0.019527 * pow(X, 4.0))), -4.0 )) - 1.0);
      
      _pVal.push_back( 1.0 - p );
    }
  }
  
  return 0;
}

/// @Details
///   - Least square method process.
int t_fit::_lsq(ColumnVector & fit)
{
  
  int Npar = fit.Nrows();
  int n    = _data.size();
  
  ColumnVector X(n);
  ColumnVector Y(n);
  
  map<double, double>::iterator J = _data.begin();
  
  for(map<double, double>::iterator I = _data.begin(); I!= _data.end(); ++I) {
    
    _vX.push_back(I->first);
    _vY.push_back(I->second);
  }
  
  for( int i = 1; i<=n; ++i) {
    
    X(i) = _vX[i-1];
    Y(i) = _vY[i-1];
  }
  
  ColumnVector fit0(Npar);
  for(int i = 1; i<=Npar; i++) {
    
    fit0(i) = 1.0;
  }
  
  // Set procedure
  Matrix      A(n, Npar);      // design matrix
  Matrix   M(Npar, Npar);      // covariance matrix
  Matrix COV(Npar, Npar);
  
  ColumnVector   N(Npar);      // right side vector
  ColumnVector  dp(Npar);      // fitted parameters
  ColumnVector    val(n);
  ColumnVector    Res(n);
  ColumnVector fitest(n);
  ColumnVector fitres(n);
  
  while(_stop == 0 && _it <= _Niter) {
    
    ++_it;
    
    if ( _model == 1 ) {
      
      this->_vLinear(X, fit0, val);
    }
    else if ( _model == 2) {
      
      this->_vPolynom(X, fit0, val);
    }
    else if ( _model == 3) {
      
      this->_vHarmFixPer(X, fit0, val);
    }
    
    Res = Y - val;
    
    // A: Designe matrix
    if ( _model == 1 ) {
      
      this->_dLinear(X, A);
    }
    else if ( _model == 2 ) {
      
      this->_dPolynom(X, A);
    }
    else if ( _model == 3 ) {
      
      this->_dHarmFixPer(X, fit0, A);
    }
    
    
    M   = A.t() * A;
    COV = M.i();
    dp  = COV * A.t() * Res;
    fit = fit0 + dp;
    
    if ( _model == 1 ) {
      
      this->_vLinear(X, fit, fitest);
    }
    else if ( _model == 2 ) {
      
      this->_vPolynom(X, fit, fitest);
    }
    else if ( _model == 3 ) {
      
      this->_vHarmFixPer(X, fit, fitest);
    }	
    
    fitres = Y-fitest;
    
    // tests of convergence
    for(int i = 1; i <= Npar; ++i) {
      
      N(i) = dp(i) / fit0(i);
    }
    
    fit0 = fit;
    
    if(N.MaximumAbsoluteValue() <= _eps && _it > 2) {
      
      _stop = 1;
    }
    
    if(_it == _Niter) {
      
      _stop = 1;
    }
  }
  
  for(unsigned int i = 1; i <= Npar; i++) {
    
    _DIAG.push_back( COV(i,i) );
  }
  
  return 0;
}

// -
int t_fit::_vLinear( const ColumnVector& X, const ColumnVector& fit, ColumnVector& val )
{
  
  for(int i=1; i<=X.Nrows(); ++i) {
    
    val(i) = fit(1) + (fit(2) * X(i));
  }
  
  return 0;
}

// -
int t_fit::_dLinear( const ColumnVector& X, Matrix& val )
{
  
  for(int i=1; i<=X.Nrows(); ++i ) {
    
    val(i,1) = 1.0;
    val(i,2) = X(i);
  }
  
  return 0;
}

// -
int t_fit::_vPolynom( const ColumnVector& X, const ColumnVector& fit, ColumnVector& val )
{
  
  for(int i=1; i<=X.Nrows(); ++i) {
    
    val(i) = fit(1);
    for(int j=1; j<=_order; ++j ) {
      
      val(i) += fit(j+1)*pow(X(i),j);
    }
  }
  
  return 0;
}

int t_fit::_dPolynom( const ColumnVector& X, Matrix& val )
{
  
  for(int i=1; i<=X.Nrows(); ++i ) {
    
    val(i,1) = 1.0;
    for(int j=1; j<=_order; ++j ) {
      
      val(i,j+1) = pow(X(i),j);
    }
  }
  
  return 0;
}

// -
int t_fit::_vHarmFixPer( const ColumnVector& X, const ColumnVector& fit, ColumnVector& val )
{
  
  for( int i=1; i<=X.Nrows(); ++i) {
    
    val(i) = fit(1) + fit(2)*X(i);
    
    for( int j=3; j<fit.Nrows(); j=j+2 ) {
      
      val(i) += fit(j) * sin((j-1) * M_PI * X(i) / _period + fit(j+1));
    }
  }
  
  return 0;
}

int t_fit::_dHarmFixPer( const ColumnVector& X, const ColumnVector& fit, Matrix& val )
{
  
  for( int i=1; i<=X.Nrows(); ++i ) {
    
    val(i,1) = 1.0;
    val(i,2) = X(i);
    for( int j=3; j<fit.Nrows(); j=j+2 ) {
      
      val(i,j)   =        sin((j-1) * M_PI * X(i) / _period + fit(j+1));
      val(i,j+1) = fit(j)*cos((j-1) * M_PI * X(i) / _period + fit(j+1));
    }
  }
  
  return 0;
}