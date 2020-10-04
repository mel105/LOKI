#ifndef TIMESTAMP_H
#define TIMESTAMP_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Class handles with time stamps
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <ctime>
#include <time.h>
#include <math.h>
#include <string>
#include <string.h>

using namespace std;

/*
 * History:
 * [2019-04-29] [mel] Class development.
 * [2019-04-30] [mel] Working on methods.
 */

/*
 * ToDo:
 * date2DOY
 * DOY2Date
 * get: doy();
 * zoznam dni v roku aj so zretelom na prestupne roky, asi vo forme timestamp, ktory si budem vediet previes do mjd, pripadne neskor na doy
 */

#define FIRST_DEF_YEAR 1900
#define LAST_DEF_YEAR 2100
#define FIRST_VALID_MJD 15079 // 1900
#define LAST_VALID_MJD 88127  // 2100

class t_timeStamp
{
   
 public:
   
   // constructors
   t_timeStamp(const string& timeStamp);
   //t_timeStamp(const int& Y, const int& M, const int& D, const int& h, const int& m, const double& s, const bool& conv);
   t_timeStamp(const int& Y, const int& M, const int& D, const int& h, const int& m, const double& s);
   t_timeStamp(const double& mjd);
    
   // destructor
   ~t_timeStamp( );
   
   /// get TIME outputs
   int year() const;
   int month() const;
   int day() const;
   int hour() const;
   int minute() const;
   double second() const;
   double mjd() const;
   string timeStamp() const;
   
 protected:
   
   /// convert from date to timeStamp, 
   /// ex1: Y, M, D, h, m, s -> Y-M-D h:m:s
   /// ex2: Y, M, D          -> Y-M-D
   void _date2TimeStamp();
   
   /// convert from timeStamp to date, 
   /// ex1: Y-M-D h:m:s -> Y, M, D, h, m, s
   /// ex2: Y-M-D       -> Y, M, D
   void _timeStamp2Date();
   
   /// convert date to MJD
   void _date2Mjd();
   
   /// convert MJD to date
   void _mjd2Date();
   
   /// data:
   string _timeStamp;
   
   int _Y;
   int _M;
   int _D;
   int _h;
   int _m;
   
   double _s;
   double _mjd;
   
   //bool _conv;
};

#endif