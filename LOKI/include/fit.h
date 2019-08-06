#ifndef FIT_H
#define FIT_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: LSQ method  
 @Details: Class contains algorithms for LSQ method implementation. Class also contains methods for 
           linear, polynomial and  harmonics model fitting.
  
 @Reference:
***************************************************************************************************/

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <math.h>

#include <nlohmann/json.hpp>

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;

//# define M_PI          3.141592653589793238462643383279502884L /* pi */

class t_fit
{
   
   typedef map<double, double> m_dd;
   
 public:
   
   t_fit(const int & model, const map<double, double> & data,  const int & order, const double & res);
   virtual ~t_fit(){};
   
   //- set
   void calcCoefDeter();
   void calcResVar();
   void calcTAlpha(); // TODO::toto by malo ist asi do stat, Rozmysli si to!!!!
   
   //- get
   ColumnVector   getCoef();
   double         getTAlpha();
   double         getSSE(); // Sc
   double         getRSS(); // Se
   double         getCD();  // koef deter
   double         getSR();  // varience of residuals
   vector<double> getSigmaFit();
   vector<double> getConfIntLow();
   vector<double> getConfIntUpp();
   vector<double> getTStat();
   vector<double> getPVals();
   
 protected:
   
   void _setPeriod();
   void _setModel();
   void _calcCoefDeter();
   void _calcResVar();
   
   int  _UP();
   int  _calcTAlpha();
   int  _sigmaFit();
   int  _confidenceInterval();
   int  _t_stat();
   int  _pValue();
   int _lsq(ColumnVector & fit);
   int _fitStat();
   
   //-
   int _vLinear( const ColumnVector& X, const ColumnVector& fit, ColumnVector& val );
   int _dLinear( const ColumnVector& X, Matrix& val );
   int _vPolynom( const ColumnVector& X, const ColumnVector& fit, ColumnVector& val );
   int _dPolynom( const ColumnVector& X, Matrix& val );
   int _vHarmFixPer( const ColumnVector& X, const ColumnVector& fit, ColumnVector& val );
   int _dHarmFixPer( const ColumnVector& X, const ColumnVector& fit, Matrix& val );     
   //-
   vector<double> _vX;
   vector<double> _vY;
   vector<double> _sigFit;
   vector<double> _confLow;
   vector<double> _confUpp;
   vector<double> _tStat;
   vector<double> _pVal;
   
   //- to fit stat
   double _Sc;  // *** total squares sum
   double _St;  // *** theoretical sum of squares: explained part of variances
   double _Se;  // *** residual sum of squares: unexplained part of variances
   double _SXX;   
   double _ym;  // *** an average value of fitted model
   double _sxx; // *** 
   double _cd;  // *** coefficient of determination
   double _sr;  // *** variance of residuals
   double _pSigma;
   double _up;
   double _tal;
   int    _order;
   
   // - to lsq
   double _eps;
   double _res;
   double _period;
   
   int _Niter;
   int _it;
   int _stop;
   
   //- Method
   ColumnVector   _fit;
   vector<double> _DIAG;
   int            _model;
   m_dd           _data;
   
};
#endif
