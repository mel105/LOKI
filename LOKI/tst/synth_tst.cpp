#include "load.h"
#include "setting.h"
#include "plot.h"
#include "timeStamp.h"
#include "stat.h"
#include "workingdir.h"

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

void tstSYNTH()
{
  
  // GET SETTING
  t_setting * setting = new t_setting();
   
  // LOADING SETTING
  vector <string> loadSetting = setting->getLoadSetting();
  string inputName = loadSetting[0];
  string inputFormat = loadSetting[1];
  string plotOnOff = setting->getPlotOnOff();
  
  /// Get actual working folder path
  t_workingDir workingDir; string outGnuplot = workingDir.GetCurrentWorkingDir();
    
  // DATA LOADING
  t_load load(setting);
   
  map<double, double>  testval;
  map<string, double> Testval;
   
  if (inputFormat == "dd") {
      
    testval = load.getTestval();
  } else if (inputFormat == "td") {
    
    Testval = load.getTimeTestval();
    
    //t_fromTDtoDD TDtoDD; testval = TDtoDD.FromTDtoDD(Testval);
  } else {
    /**/
  }
  
  vector <double> toStat;
  for ( map<string, double>::iterator i = Testval.begin(); i != Testval.end(); ++i ) {
    
    toStat.push_back(i->second);
    //cout << i->first << "  " << i->second << endl;
  }
  
  // Descriptive statistics
  t_stat stat(toStat); 
  stat.calcMean(); double mean = stat.getMean(); //cout << mean << endl;
  stat.calcSdev(); double sdev = stat.getSdev(); //cout << sdev << endl;
  stat.calcVare(); double vare = stat.getVare(); //cout << vare << endl;
  
  // Synthetic shifts
  /*1*/ t_timeStamp time_a("1992-02-10 12:00:00"); double mjd_a = time_a.mjd(); double shift_a = vare * 15.0; cout << time_a.timeStamp() << "  " << mjd_a << "  " << shift_a << endl;
  /*2*/ t_timeStamp time_b("1996-04-12 12:00:00"); double mjd_b = time_b.mjd(); double shift_b = vare * 10.0; cout << time_b.timeStamp() << "  " << mjd_b << "  " << shift_b << endl;
  /*3*/ t_timeStamp time_c("2000-06-14 12:00:00"); double mjd_c = time_c.mjd(); double shift_c = vare * 5.0; cout << time_c.timeStamp() << "  " << mjd_c << "  " << shift_c << endl;
  /*4*/ t_timeStamp time_d("2004-08-16 12:00:00"); double mjd_d = time_d.mjd(); double shift_d = vare * 30.0; cout << time_d.timeStamp() << "  " << mjd_d << "  " << shift_d << endl;
  /*5*/ t_timeStamp time_e("2008-10-18 12:00:00"); double mjd_e = time_e.mjd(); double shift_e = vare * 25.0; cout << time_e.timeStamp() << "  " << mjd_e << "  " << shift_e << endl;
  /*6*/ t_timeStamp time_f("2012-12-20 12:00:00"); double mjd_f = time_f.mjd(); double shift_f = vare * 20.0; cout << time_f.timeStamp() << "  " << mjd_f << "  " << shift_f << endl;
  
  map<string, double> newts;
  
  for ( map<string, double>::iterator i = Testval.begin(); i != Testval.end(); ++i ) {
    
    t_timeStamp tmp = i->first; double mjd_t = tmp.mjd();
    
    if ( mjd_t > mjd_a) {
      
      newts[i->first] = i->second + shift_a;
    } else {
      
      newts[i->first] = i->second;
    }
    
    if ( mjd_t > mjd_b) {
      
      newts[i->first] = i->second - shift_b;
    } 
    
    if ( mjd_t > mjd_c) {
      
      newts[i->first] = i->second + shift_c;
    } 
    
    if ( mjd_t > mjd_d) {
      
      newts[i->first] = i->second - shift_d;
    } 
    
    if ( mjd_t > mjd_e) {
      
      newts[i->first] = i->second + shift_e;
    } 
    
    if ( mjd_t > mjd_f) {
      
      newts[i->first] = i->second - shift_f;
    } 
      
      
    
    
  }
   
  ofstream mLine((outGnuplot + "/gnuplot/line"));
  
  for (map<string, double>::iterator i = newts.begin(); i != newts.end(); ++i) {
//  for (map<string, double>::iterator i = Testval.begin(); i != Testval.end(); ++i) {    
    
    mLine << fixed << setprecision(5)
       << i->first  << " "
       << i->second << endl;
  }
  
  mLine.close();
  
  t_plot mPlot; mPlot.tline();
}
