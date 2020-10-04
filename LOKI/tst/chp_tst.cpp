#include "appMedian.h"
#include "appDetection.h"
#include "chp_tst.h"
#include "autoCov.h"
#include "convTDtoDD.h"
#include "load.h"
#include "setting.h"
#include "plot.h"
#include "stat.h"

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
#include <random>

#include "timeStamp.h"
#include "workingdir.h"

void manager()
{
  
  // GET SETTING
  t_setting * setting = new t_setting();
   
  // LOADING SETTING
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
  
  /// Get actual working folder path
  t_workingDir workingDir; string outGnuplot = workingDir.GetCurrentWorkingDir();
  
  //cout << inputFormat << "  " << outGnuplot << endl;
  
  /// DATA LOADING
  t_load load(setting);
  
  map<double, double>  testval;
  map<string, double> Testval;
  
  // todo: .here possible problem
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
  else { 
    /**/
  }
  
  /*
  cout << Testval.size() << endl;
  for (map<string, double>::iterator it = Testval.begin(); it != Testval.end(); ++it) {
    
    cout << it->first << "  " << it->second << endl;
  }
  */   
  
  /// if td, then convert to  dd.
  t_fromTDtoDD fromTDtoDD; map<double, double> origData = fromTDtoDD.FromTDtoDD(Testval);
  map<double, double>::const_iterator beg = origData.begin(); // time series begining
  map<double, double>::reverse_iterator end = origData.rbegin(); // end of time series
  
  /// statistic
  vector<double> toStat;
  for (map<double, double>::iterator it = origData.begin(); it != origData.end(); ++it) {
    
    toStat.push_back(it->second);
  }     
  t_stat stat(toStat); 
  stat.calcMean(); double mean = stat.getMean();
  stat.calcSdev(); double sdev = stat.getSdev();
  stat.calcVare(); double vare = stat.getVare();
  
  /// synthetic change point generation
  /// Random seed
  random_device rd;
  
  /// Initialize Mersenne Twister pseudo-random number generator
  mt19937 genPocet(rd());
  //default_random_engine generator;
  uniform_int_distribution<> distPocet(1, 5);
  int pocet = distPocet(genPocet);
  cout << "Number of change points: " << pocet << endl;
  
   /// generated epoch
   /// Initialize Mersenne Twister pseudo-random number generator
   mt19937 genChangePoint(rd());
   //default_random_engine generator;
   uniform_int_distribution<> distChangePoint(floor(beg->first), floor(end->first));
   vector<int> epoIdxVec;
   
   for ( int iChp = 0; iChp < pocet; iChp++ ) {
      
      int epo = distChangePoint(genChangePoint); epoIdxVec.push_back(epo);
   }
  
   /// generated offset
   /// Initialize Mersenne Twister pseudo-random number generator
   mt19937 genOffset(rd());
   //default_random_engine generator;
   vector<double> offVec;
   
   uniform_real_distribution<double> distOffset(-vare, vare);
   for ( int iOff = 0; iOff < pocet; iOff++ ) {
      
      double offset = distOffset(genOffset); offVec.push_back(offset);
   }
   
   // synthetic data sorting
   map<int, double> synthOff;
   for (int i = 0; i<pocet; i++) { cout << "List of synthetic change points  " << epoIdxVec[i] << " " << offVec[i] << endl; synthOff[epoIdxVec[i]] = offVec[i]; }
         
   ///
   map<string, double> newTestval; newTestval = Testval;
   for (map<int, double>::iterator iO = synthOff.begin(); iO != synthOff.end(); iO++) { // 
      
      int idx = 0;      

      for (map<string, double>::iterator itSet = Testval.begin(); itSet != Testval.end(); ++itSet) {
        	 
	 if (idx >= iO->first) {
	    
	    newTestval[itSet->first] = itSet->second + iO->second;
	 }
	 
	 idx++;
      } // end for loop
   }
   
#ifdef DEBUG
   int aIdx = 1;
   for (map<string, double>::iterator it = Testval.begin(); it!=Testval.end(); ++it) {
     
      map<string, double>::iterator itNew = newTestval.find(it->first);
      
      if ( itNew != newTestval.end()) {
	 
	 t_timeStamp mTimeStamp(it->first);

	 cout << aIdx << "  " << mTimeStamp.timeStamp() << "  " << it->second << " " << itNew->second << "  " << it->second - itNew->second << endl;

      }
      
      aIdx++;
   }
#endif
   
   /// data adding into the coredata
   t_coredata * coredata = new t_coredata(newTestval);
   
   /// deseasonalising
   new t_appMedian(setting, coredata);
   
   /// change point detection
   new t_appDetection(setting, coredata);
  
   /// NAVRH PROTOKOLOV
  
   /// TP, FP, TN, FN STATISTKY A CELKOVE PREHLADY. NAPR:
   ///   1. AKA JE CETNOST GENEROVANYCH CH.P. KEBY SOM ICH POLOHY ROZDELIL NA TRETINY V RAMCI ORIG.
   ///      CASOVEJ RADY
   ///   2. AKA JE USPESNOST DETEKOVANIA VZHLADOM K NASTAVENIU, T.J. POLOHA CHANGE POINTU VZ. 
   ///      VELKOST SKOKU
   ///   3. AKY JE EFEKT AR MODELU?
   
   
   /// Delete
   if ( coredata ) delete coredata ;
   if ( setting  ) delete setting  ;
}