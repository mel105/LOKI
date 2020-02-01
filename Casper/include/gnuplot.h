#ifndef GNUPLOT_H
#define GNUPLOT_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: 
 @Details: Class allows to use Gnuplot for figures plotting
  
 @Reference:
***************************************************************************************************/

#include <string>
#include <iostream>

using namespace std;

class t_gnuplot
{
   
 public:
   
   t_gnuplot();
   ~t_gnuplot();
   
   void operator () (const string & command);
   
 protected:
   
   FILE *gnuplotpipe;
};

#endif