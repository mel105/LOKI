#include "autoCov_tst.h"
#include "autoCov.h"
//#include "convTDtoDD.h"
#include "load.h"
#include "setting.h"
#include "plot.h"

#include <set>
#include <map>
#include <list>
#include <vector>

#include <string>

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "timeStamp.h"
#include "workingdir.h"

void tstAutoCov()
{
   // GET SETTING
   t_setting * setting = new t_setting();
   
   // LOADING SETTING
   vector <string> loadSetting = setting->getLoadSetting();
   string inputName            = loadSetting[0];
   string inputFormat          = loadSetting[1];
   string plotOnOff            = setting->getPlotOnOff();
    
   /// Get actual working folder path
   t_workingDir workingDir; string outGnuplot = workingDir.GetCurrentWorkingDir();
    
   // DATA LOADING
   t_load load(setting);
   
   map<double, double>  testval;
   //map<t_gtime, double> Testval;
   map<string, double> Testval;
   
   if      (inputFormat == "dd") {
      
      testval = load.getTestval();
   }
   else if (inputFormat == "td") {
      
      Testval = load.getTimeTestval();
      
     //t_fromTDtoDD TDtoDD; testval = TDtoDD.FromTDtoDD(Testval);
   }
   else {
      /**/
   }
   
   // ESTIMATE ACF
   vector<double> ACF;
   t_autoCov autoCov(testval); ACF = autoCov.getAutoCov();
   
   assert(!ACF.empty());
   std::cout << " Test passes! \n";
   
   // MAKE PLOT
   if (plotOnOff == "on") {
      
      t_plot mplot;
      
      // Prepare data for plot
     
      ofstream mLine((outGnuplot + "/gnuplot/acf.pt"));
 
      for ( size_t i = 0; i < 100; ++i) {
	 
	 mLine << fixed << i << "  " << setprecision(5) << ACF[i] <<"\n";
      }
            
      mLine.close();
 
      /// Plot
      mplot.acf();
   }
}
