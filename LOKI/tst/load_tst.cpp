#include "load_tst.h"
#include "load.h"

#include "setting.h"

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

void tstLoad()
{
   // GET SETTING
   t_setting * setting = new t_setting();
   
   // LOADING SETTING
   vector <string> loadSetting = setting->getLoadSetting();
   string inputName            = loadSetting[0];
   string inputFormat          = loadSetting[1];
   
   // DATA LOADING
   t_load load(setting);
   
   map<double, double>  testval;
   //map<t_gtime, double> Testval;
   map<string, double> Testval;
   
   if      (inputFormat == "dd") {
      
      testval = load.getTestval();
      
      assert(!testval.empty());
      std::cout << "[dd]::Test passes! \n";
   }
   else if (inputFormat == "td") {
      
      Testval = load.getTimeTestval();
      
      assert(!Testval.empty());
      std::cout << "[td]::Test passes! \n";
   }
   else {
      /**/
   }
  
  /*
  for (map<t_gtime, double>::iterator i = Testval.begin(); i!= Testval.end(); i++)
     cout << i->first.str_ymdhms() << "  " << i->second << endl;
  
  cout << Testval.size() << endl;
  */
  
  // -------------------------
  map<double, double> test;
       
  ifstream dir("/home/michal/Work/PROHO/DAT/change.dat");
  
  string ymd, hms, line;
  double vals;
  
  if(dir){
    
    while ( getline(dir, line) ){
      
      istringstream istr(line);
      istr >> ymd >> hms >> vals;
      
      t_timeStamp epoch(ymd+" "+hms); 
      
      cout << ymd+" "+hms << "  " << epoch.mjd() << "  " << vals << endl;
      
      test[epoch.mjd()] = vals;
    }
  }
  
  
  
  
  
  
}
