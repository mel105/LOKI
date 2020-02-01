#ifndef CONVTDTODD_H
#define CONVTDTODD_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Convert td format to dd.
 @Details:
   - convert from map<string, double> format to the map<double, double> format. For example, convert
     from 2000-03-03 22:22:22 to MJD
   - substract the first epoch from the rest of epoch  
 @Reference:
***************************************************************************************************/


#include <iostream>
#include <map>
#include "timeStamp.h"
#include "logger.hpp"

using namespace std;

class t_fromTDtoDD
{
   
 public:   
   

   map<double, double> FromTDtoDD( const map<string, double> & TESTVAL ) {
     
     map<double, double> OUT;
     
     if(TESTVAL.empty()) {
       
       ERR(":......t_fromTDtoDD::......No data for conversion!");
     }else{
       
       map<string, double>::const_iterator J = TESTVAL.begin();
       t_timeStamp beg(J->first); 
       
       double epo_beg = beg.mjd();
       
       for( map<string, double>::const_iterator I = TESTVAL.begin(); I != TESTVAL.end(); ++I ) {
         
         t_timeStamp epo(I->first); 
         double tmt = epo.mjd() - epo_beg;
         
         OUT[tmt] = (I->second);
       }
     }
     
     if(!OUT.empty()) {
       
       LOG1(":......t_fromTDtoDD::......Data conversion from *string to double* is done!");
     }
     
     return OUT;
   }
   
   
   /// @Details
   ///   - substract the first epoch from the rest of epoch
   ///   - ToDo:Maybe new method?
   map<double, double> FromTDtoDD( const map<double, double> & TESTVAL ) {
     
     map<double, double> OUT;
     
     if(TESTVAL.empty()) {
       
       ERR(":......t_fromTDtoDD::......No data for conversion!");
     }else{
       
       map<double, double>::const_iterator J = TESTVAL.begin();
       double epo_beg = J->first;
       
       for( map<double, double>::const_iterator I = TESTVAL.begin(); I != TESTVAL.end(); ++I ) {
         
         double tmt = I->first - epo_beg;
         
         OUT[tmt] = (I->second);
       }
     }
     
     if(!OUT.empty()) {
       
       LOG1(":......t_fromTDtoDD::......Data conversion from *string to double* is done!");
     }
     
     return OUT;
   }   
   
 private:
   
};

#endif