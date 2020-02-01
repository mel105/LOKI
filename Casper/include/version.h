#ifndef VERSION_H
#define VERSION_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Set version
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <cstring>

using namespace std;

class t_version
{
   
  public:
   
   // Construnctor
   t_version(const string & major,
             const string & minor,
             const string & revision,
             const string & state);
   
   // Destructor
   virtual ~t_version(){};
   
   string Version();
   
  protected:
   
   void _setVersion();
   
   string _major; 
   string _minor; 
   string _revision;
   string _state;
   
   string _version;
};

#endif