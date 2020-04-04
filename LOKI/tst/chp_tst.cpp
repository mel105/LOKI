#include "appMedian.h"
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
  
  // todo: toto aj v maine treba doriesit. Uz neviem, ze ako som to riesil
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
  
  /// ak td, potom convertuj na dd.
  t_fromTDtoDD fromTDtoDD; map<double, double> origData = fromTDtoDD.FromTDtoDD(Testval);
  map<double, double>::const_iterator beg = origData.begin(); // pociatok casovej rady
  map<double, double>::reverse_iterator end = origData.rbegin(); // koniec casovej rady
  
  /// POPISNA STATISTIKA
  vector<double> toStat;
  for (map<double, double>::iterator it = origData.begin(); it != origData.end(); ++it) {
    
    toStat.push_back(it->second);
  }     
  t_stat stat(toStat); 
  stat.calcMean(); double mean = stat.getMean();
  stat.calcSdev(); double sdev = stat.getSdev();
  stat.calcVare(); double vare = stat.getVare();
  
  /// VYGENEROVANIE NAHODNEHO BODU ZMENY A NAHODNEHO SKOKU. BUDE POTREBA NAPISAT VHODNY GENERATOR.
  /// Random seed
  random_device rd;
  
  /// vygenerovany pocet zmien
  /// Initialize Mersenne Twister pseudo-random number generator
  mt19937 genPocet(rd());
  //default_random_engine generator;
  uniform_int_distribution<> distPocet(1, 5);
  int pocet = distPocet(genPocet);
  
  /// vygenerovana epocha
  /// Initialize Mersenne Twister pseudo-random number generator
  mt19937 genChangePoint(rd());
  //default_random_engine generator;
  uniform_int_distribution<> distChangePoint(floor(beg->first), floor(end->first));
  for ( int iChp = 0; iChp <= pocet; ++iChp ) {
    
    int epo = distChangePoint(genChangePoint);
    cout << epo << endl;
  }
  
  /// vygenerovany offset
  /// Initialize Mersenne Twister pseudo-random number generator
  mt19937 genOffset(rd());
  //default_random_engine generator;
  uniform_real_distribution<double> distOffset(-vare, vare);
  for ( int iOff = 0; iOff <= pocet; ++iOff ) {
    
    double offset = distOffset(genOffset);
    cout << offset << endl;
  }

  /// ELIMINOVANIE SEZONNEJ ZLOZKY
  t_coredata * coredata = new t_coredata(Testval);
  new t_appMedian(setting, coredata);
  
  
  
  /// DETEKOVANIE CHANGE POINTU
  
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