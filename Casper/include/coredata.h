#ifndef COREDATA_H
#define COREDATA_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: 
 @Details: The main point of the class is to temporary save original and other data
  
 @Reference:
***************************************************************************************************/

#include <map>
#include <vector>
#include <iostream>

#include "logger.hpp"
#include "timeStamp.h"
//#include "gtime.h"

using namespace std;

class t_coredata
{
  public:
   
   typedef map<double, double>  m_dd;
   typedef map<string, double> m_td;
   
   t_coredata();
   t_coredata(const m_dd & origval);
   t_coredata(const m_td & Origval);
   
   virtual ~t_coredata(){};
   
   void addData( const string & datatype, m_dd & data );
   void addData( const string & datatype, m_td & data );   
   
   m_dd getData    ( const string & datatype );
   m_td getTimeData( const string & datatype );
   
   // void remData()... /// to znamena, ze funkcia zabezpeci to, ze na zakade nejakeho ID vycisti prislusny kontajner
   
  protected:
   
   void _checkAdd(const string & datatype);
   void _CheckAdd(const string & datatype);   
   
   m_dd _origval;
   m_td _Origval;
   
   m_dd _elimTrend;
   m_dd _elimSeasonal;
   m_dd _elimOutliers;
   m_dd _homogenTS;
   
   m_td _ElimTrend;
   m_td _ElimSeasonal;
   m_td _ElimOutliers;
   m_td _HomogenTS;
   
};

#endif