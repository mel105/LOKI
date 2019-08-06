#ifndef DESEAS_H
#define DESEAS_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Method provides deseasonalised time series.
 @Details: Method provides seasonal and also deseasonalised time series. Is valid for regression.
  
 @Reference:
***************************************************************************************************/

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;

class t_deseas
{
   
   typedef map<double, double> m_dd;
   
 public:
   
   t_deseas(const map<double, double> & data,  const ColumnVector & fit, const double & res);
   virtual ~t_deseas(){};
   
   map<double, double> getSeas();
   map<double, double> getDeseas();
   
 protected:
   
   void _elimSeas();
   void _setPeriod();
      
   ColumnVector _fit;
   
   m_dd _data;
   m_dd _seas;
   m_dd _deseas;
   
   double _res;
   double _per;
   
};
#endif
