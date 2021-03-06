/************************************************************************************************** 
 LOKI - software development library
  
 (c) 2019 Michal Elias
  
 This file is part of the LOKI C++ library.
  
 This library is free software; you can redistribute it and/or  modify it under the terms of the GNU
 General Public License as  published by the Free Software Foundation; either version 3 of the 
 License, or (at your option) any later version.
  
 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.
  
 You should have received a copy of the GNU General Public License  along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#include <stdio.h>
#include <iostream>
#include <string>
#include <ctime>
#include <time.h>

#include "newmat/newmat.h"
#include "logger.hpp"
#include "loki.h"
#include "load.h"
#include "coredata.h"
#include "version.h"
#include "help.h"
#include "timeStamp.h"
#include "setting.h"

#include "appStat.h"
#include "appRegress.h"
#include "appDetection.h"
#include "appMedian.h"

// - tst 
#include "chp_tst.h" 
#include "load_tst.h"
#include "autoCov_tst.h"
#include "mjd_tst.h"
#include "synth_tst.h"

using json = nlohmann::json;

using namespace std;

const char* const COMPILED = __DATE__ " @ " __TIME__;

int main(int argc, char ** argv)
{
   
   LOG1(":.main::*** Software: LOKI (app started) ");
   time_t now = time(0); tm* localtm = localtime(&now);
   clock_t start = clock();
   
   /// .set and get version
   t_version version("0","0","21", "alpha");
   LOG1(":.main::*** Version: " , version.Version());
   LOG1(":.main::*** Compiled: ", COMPILED);
   
   /// .get setting
   t_setting * setting = new t_setting();
   if(setting != NULL){ 
      
      LOG1(":.main::...Loaded configuration file"); 
   }
   else {
      
      ERR(":.main::...Problem with configuration file loading!");
   }
   
   vector <string> loadSetting = setting->getLoadSetting();
   string inputName = loadSetting[0];
   string inputFormat = loadSetting[2];
   
   string outputName = setting->getOutputName();
   string outputHist = setting->getOutputHist();
   string statOnOff = setting->getStatOnOff();
   string regressOnOff = setting->getRegressOnOff();
   string detectionOnOff = setting->getDetectionOnOff();
   string medianOnOff = setting->getMedianOnOff();
   
   bool convTdDd = setting->getInputConvTdDd();
   
   remove(outputName.c_str()); ofstream ofile; ofile.open(outputName.c_str(), ios::out | ios::app);
   remove(outputHist.c_str()); ofstream hfile; hfile.open(outputHist.c_str(), ios::out | ios::app);
   
   /// .history
   hfile << "[2019-02-10] [mel] -> [main] Library structure created\n";
   hfile << "[2019-02-19] [mel] -> [json] Added Json library;  Tested first config file\n";
   hfile << "[2019-02-22] [mel] -> [load] Simple data file's decoder created\n";
   hfile << "[2019-03-12] [mel] -> [main] Added list of history; Added version class; Tested output setting; Log file created\n";
   hfile << "[2019-03-13] [mel] -> [plot] Added gnuplot for simple figures ploting; Added external class for time-string handling\n";
   hfile << "[2019-05-01] [mel] -> [load] Data file's decoder improving\n";
   hfile << "[2020-12-13] [mel] -> [main] Multi-files processing\n";
   hfile << "[2020-12-15] [mel] -> [main] Multi-change point detection\n";   
   
   /// .data loading
   t_load load(setting);
   
   /// .check number of data files
   vector<string> dataNames = load.getDataFiles();
   
   for (auto i = dataNames.begin(); i!= dataNames.end(); ++i) {
	
            
      // .set actual file name
      load.setDataFileName(*i);
      
      // .read data
      load.readData();
      
      map<double, double>  testval;
      map<string, double> Testval;
      
      if ( inputFormat == "dd" ) {
	 
	 testval = load.getTestval();
      }
      else if ( inputFormat == "td" ) {
	 
	 if (convTdDd) {
	    
	    Testval = load.getTimeTestval();
	 }
	 else {
	    
	    testval = load.getTestval();
	 }
      }
      else {  }
      
      /// .set log file
      if(!testval.empty() || !Testval.empty()) {
	 
	 LOG1(":.main::...Loaded data file");
	 if(inputFormat == "dd") {
	    
	    LOG1(":.main::...Data container size: ", testval.size());
	 }
	 else if(inputFormat == "td" && convTdDd == true) {
	    
	    LOG1(":.main::...Data container size: ", Testval.size()); 
	 }
	 else if(inputFormat == "td" && convTdDd == false) {
	    
	    LOG1(":.main::...Data container size: ", testval.size()); 
	 }    
	 else{ 
	    
	    ERR(":.main::...Input data format %s is not supported: ", inputFormat); return -1; 
	 }
      }
      else {
	 
	 ERR(":.main::...Data container is empty!"); return -1; 
      }
      
      /*TEST LOADING*/
      //tstLoad();
      /*TEST AUTOCOV*/
      //tstAutoCov();
      /*TEST MJD*/
      //tstMJD();
      /*SYNTHETIC SHIFTS*/
      //tstSYNTH();
      /*Chp method testing*/
      //manager();
      
      /// .loaded data filled into the coredata class
      ofile << "# Software: LOKI\n";
      ofile << "# Version: " << version.Version() << "\n";
      ofile << "# ----------------\n";
      ofile << "# Protocol created: " << asctime(localtm) << endl;
      
      cout << "\n" << endl;
      cout << "# File name: " << *i << endl;
      cout << "\n" << endl;
     
      ofile << "\n" << endl;
      ofile << "# File name: " << *i << endl;
      ofile << "\n" << endl;
      
      /// .copy pointers into the coredata object. ToDo: here could be problem with data decision. Check it!
      if (inputFormat == "dd") {
	 
	 /// .pointer
	 t_coredata * coredata = new t_coredata(testval);
	 
	 /// .log
	 if(coredata != NULL){
	    
	    LOG1(":.main::...Pointer to coredata obj.");
	 }
	 
	 /// .call loki applications
	 if (statOnOff     == "on") { LOG1(":.main::...Request for appStat");     new t_appStat(setting, coredata);     }
	 if (regressOnOff  == "on") { LOG1(":.main::...Request for appRegress");  new t_appRegress(setting, coredata);  }
	 if (medianOnOff   == "on") { LOG1(":.main::...Request for appMedian");   new t_appMedian(setting, coredata);   }    
	 if (detectionOnOff  == "on") { LOG1(":.main::...Request for appdetection");  new t_appDetection(setting, coredata);  }
	 
	 /// .delete
	 if ( coredata ) delete coredata ;
      }
      else if (inputFormat == "td") {
	 
	 /// .pointer
	 t_coredata * Coredata = new t_coredata(Testval);
	 t_coredata * coredata = new t_coredata(testval);
	 
	 /// .log
	 if(Coredata != NULL) {
	    
	    LOG1(":.main::...Pointer to Coredata obj.");
	 }
	 
	 /// .call loki applications
	 if ( convTdDd ) {
	    
	    if (statOnOff     == "on") { LOG1(":.main::...Request for appStat");     new t_appStat(setting, Coredata);     }
	    if (regressOnOff  == "on") { LOG1(":.main::...Request for appRegress");  new t_appRegress(setting, Coredata);  }
	    if (medianOnOff   == "on") { LOG1(":.main::...Request for appMedian");   new t_appMedian(setting, Coredata);   }      
	    if (detectionOnOff  == "on") { LOG1(":.main::...Request for appdetection");  new t_appDetection(setting, Coredata);  }
	 }
	 else {
	    
	    if (statOnOff     == "on") { LOG1(":.main::...Request for appStat");     new t_appStat(setting, coredata);     }
	    if (regressOnOff  == "on") { LOG1(":.main::...Request for appRegress");  new t_appRegress(setting, coredata);  }
	    if (medianOnOff   == "on") { LOG1(":.main::...Request for appMedian");   new t_appMedian(setting, coredata);   }      
	    if (detectionOnOff  == "on") { LOG1(":.main::...Request for appdetection");  new t_appDetection(setting, coredata);  }
	 }
	 
	 /// .delete
	 if ( Coredata ) delete Coredata ;
	 if ( coredata ) delete coredata ;
      }
      else { 
	 
	 ERR(":.main::...Requested input data format is not supported!");
      }
      
      
      /// .clear/close/delete
      LOG1(":.main::...Clear/Close/Delete");
      testval.clear();
      Testval.clear();
      
      ofile.close();
      hfile.close();
   }
   
   
   if ( setting  ) delete setting  ;
   
   /// .finish the proces of detection
   clock_t stop = clock();
   double duration = double(stop - start) / CLOCKS_PER_SEC;
   
   LOG1(":.main::*** LOKI (app finished)");
   LOG1(":.main::*** TOTAL TIME TAKEN: ", duration, " [s]!");
   
   // .get version
   if(argc == 2 && strcmp(argv[1], "--version")==0) {
      cout << "\n\nLOKI [--version]: " << version.Version() << endl;
   }   
   
   // .get help
   if(argc == 2 && strcmp(argv[1], "--help")==0) {
      cout << "\n\nLOKI [--help]\n" <<endl;
      t_help lh; lh.lokiHelp();
   }
   
   return 0;
}