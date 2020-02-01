#ifndef PLOT_H
#define PLOT_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Class contains some basic scripts for figures plotting.
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <map>
#include <vector>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

class t_plot
{
   
 public:
   
   void acf();     // corelogram
   void line();    // simple line plot
   void tline();
   void trend();   // multiplot: trend plot
   void seas();    // multiplot: reg model plot
   void median();  // median time series plot
   void outlier(); // outliers identification
   void hTime();   // simple line plot
   void hDeseas(); // simple line plot
   void hTK();     // TK statistics
   void histogram(const double & min, const double & max);
   void boxplot();
   
};

#endif