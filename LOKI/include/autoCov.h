#ifndef AUTOCOV_H
#define AUTOCOV_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Method provides autoregression coefficients estimation
 @Details:
  
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

#include "coredata.h"
#include "setting.h"

#include "newmat/newmat.h"
#include "newmat/newmatio.h"

using json = nlohmann::json;

using namespace std;
using namespace NEWMAT;


class t_autoCov
{
   
   typedef map<double, double>  m_dd;
   
  public:

   ///   Constructor
   t_autoCov( const vector< double>     & data );
   t_autoCov( const map<double, double> & data );
   
   ///   Destructor   
   virtual ~t_autoCov(){};
   
   vector<double> getAutoCov();
   
 protected:
   
   void _autoCov();
   
   vector <double> _xcov;
   vector <double> _dataVec;
};
#endif