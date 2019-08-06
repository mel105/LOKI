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

#include "timeStamp.h"
#include "logger.hpp"

/// Constructor(s)
// convert from time stamp string, e.g. "2000-01-01 12:14:22.25" to the MJD+(HMS fractional)
t_timeStamp::t_timeStamp(const string& timeStamp){
   
  _timeStamp =  timeStamp;
   
  // set default vals
  _Y = _M = _D = _h = _m = 0; _s = _mjd = 0.0;
   
  // split time stamp string into the date parameters.
  this->_timeStamp2Date();
  
  // calculate mjd
  if(_Y != 0 &&
     _M != 0 &&
     _D != 0){
    
    this->_date2Mjd();
  }
}

// convert from Y, M, D, h, m s, e.g. "2000,1,1,12,14,22.25" to the MJD+(HMS fractional)
//t_timeStamp::t_timeStamp(const int& Y, const int& M, const int& D, const int& h, const int& m, const double& s, const bool& conv){
t_timeStamp::t_timeStamp(const int& Y, const int& M, const int& D, const int& h, const int& m, const double& s){
  
  if(Y != 0 &&
     M != 0 &&
     D != 0){
    
    _Y = Y; _h = h;
    _M = M; _m = m;
    _D = D; _s = s;
    
    // _conv = conv;
    
    // create time stemp
    this->_date2TimeStamp();
    
    // calculate mjd
    this->_date2Mjd();
  }
}
 
// convert from MJD to date and timestamp
t_timeStamp::t_timeStamp(const double& mjd){
  
   _mjd = 999.9; _mjd = mjd;
  
   // estimate date
   if (_mjd != FIRST_VALID_MJD || _mjd != LAST_VALID_MJD) {
      
     this->_mjd2Date();
   }
  
   // estimate timeStamp: sec are given without dsecs
   if(_Y != 0 &&
      _M != 0 &&
      _D != 0){
      
      // create time stemp
      this->_date2TimeStamp();
   }
}

/// Destructor
t_timeStamp::~t_timeStamp()
{} 

// get functions
int t_timeStamp::year() const{

  if(_Y>=FIRST_DEF_YEAR || _Y<=LAST_DEF_YEAR){
    
    return _Y;
  }
  else{
    
    return 9999;
  }
}

int t_timeStamp::month() const{

  if(_M<=12 || _M>=1){
    
    return _M;
  }
  else{
    
    return 9999;
  }
}

int t_timeStamp::day() const{

  if(_D>=1 || _D<=366){
    
    return _D;
  }
  else{
    
    return 9999;
  }
}

int t_timeStamp::hour() const{

  if(_h>=0 || _h<24){
    
    return _h;
  }
  else{
    
    return 9999;
  }
}

int t_timeStamp::minute() const{

  if(_m>=0 || _m<60){
    
    return _m;
  }
  else{
    
    return 9999;
  }
}

double t_timeStamp::second() const{

  if(_s>=0.0 || _s<60.0){
    
    return _s;
  }
  else{
    
    return 9999.9;
  }
}

double t_timeStamp::mjd() const{

  if(_mjd>=FIRST_VALID_MJD || _mjd<=LAST_VALID_MJD){
    
    return _mjd;
  }
  else{
    
    return 9999.9;
  }
}

string t_timeStamp::timeStamp() const{

    return _timeStamp;
}


// - calculate date from MJD
void t_timeStamp::_mjd2Date(){
  
  // estimat Julian date  
  long J, C, Y, M;
  
  J = _mjd + 2400001 + 68569;
  C = 4 * J / 146097;
  J = J - (146097 * C + 3) / 4;
  Y = 4000 * (J + 1) / 1461001;
  J = J - 1461 * Y / 4 + 31;
  M = 80 * J / 2447;
  
  _D = J - 2447 * M / 80;
  
   J = M / 11;
   
  _M = M + 2 - (12 * J);
  _Y = 100 * (C - 49) + Y + J;
  
  // to date add hrs, min and secs
  double mmjd = 999.9; mmjd = _mjd;
  
  _h = 0;
  _m = 0;
  _s = 0.0;

  // create new time stemp
  this->_date2Mjd();
  
  int hr = (mmjd - _mjd) * 24;
  int mn = ((mmjd - _mjd) * 24 - hr) * 60;
  int sc = (double)((((mmjd - _mjd) * 24 - hr) * 60) - mn) * 60.0;
  
  _h = hr; _m = mn; _s = (double)sc;
  
#ifdef DEBUG
   cout << _Y << "  " << _M << "  " << _D << "  " << hr << "  " << mn << "  " << sc <<  endl;
#endif
}

// - calculate mjd
void t_timeStamp::_date2Mjd(){
   
   double Y = static_cast<double>(_Y);
   double M = static_cast<double>(_M);
   double D = static_cast<double>(_D);
   
   double h = static_cast<double>(_h);
   double m = static_cast<double>(_m);
  
#ifdef DEBUG
  cout 
     << " Y " << _Y 
     << " M " << _M 
     << " D " << _D 
     << " h " << _h 
     << " m " << _m 
     << " s " << _s 
     << endl;
#endif

   if(M <= 2) { Y--; M += 12;} 
   int i = Y / 100;
   int k = 2 - i +i/4;
   _mjd = (floor(365.25 * Y) + floor(30.6001 * (M + 1)) + D + k - 679006.0);
   
   double integer_part    = (int)_s;
   double fractional_part = (double)( (_s - integer_part)); // * pow(10,  3) + 0.5);
   
   // seconds of the day
   double sod = (h * 3600.0) + (m * 60.0) + _s; //integer_part;
   
   // mjd
   _mjd = _mjd +  sod/24.0/3600.0;
   
#ifdef DEBUG      
   cout << fixed << setprecision(5) << " MJD: " <<  _mjd << endl;
#endif
}

// - date to time stamp
void t_timeStamp::_date2TimeStamp(){
/*  
  string s;
  if ( _conv == false ) {
     
    int ss = static_cast<int>(_s);
    
    if(ss<10) { s = ("0"+to_string(ss)); } else { s = to_string(ss); }
  } else {
    
    if(_s<10) { s = ("0"+to_string(_s)); } else { s = to_string(_s); }
  }
  */
  
  string M; if(_M<10) { M = ("0"+to_string(_M)); } else { M = to_string(_M); }
  string D; if(_D<10) { D = ("0"+to_string(_D)); } else { D = to_string(_D); }
  string h; if(_h<10) { h = ("0"+to_string(_h)); } else { h = to_string(_h); }
  string m; if(_m<10) { m = ("0"+to_string(_m)); } else { m = to_string(_m); }
  string s; if(_s<10) { s = ("0"+to_string(_s)); } else { s = to_string(_s); }
  
  _timeStamp = (to_string(_Y)+"-"+M+"-"+D+" "+h+":"+m+":"+s);
  
#ifdef DEBUG
  cout << "from ymhdms to str " << _timeStamp << endl;
#endif
   
}

// - split time stamp into the date elements
void t_timeStamp::_timeStamp2Date(){
   
   if(sscanf(_timeStamp.c_str(), "%d-%d-%d %d:%d:%lf", &_Y, &_M, &_D, &_h, &_m, &_s) == 6){

#ifdef DEBUG      
      cout 
         << _Y << "  " 
         << _M << "  " 
         << _D << "  " 
         << _h << "  " 
         << _m << "  " 
         << _s << endl;
#endif
   }
}