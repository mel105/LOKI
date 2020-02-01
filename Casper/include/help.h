#ifndef CASPERHELP_H
#define CASPERHELP_H

/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2020-02-01 18:27:11 Author: Michal Elias  

 @Brief: Returns help.
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <cstring>

using namespace std;

class t_help
{
   
 public:
   
   int lokiHelp( ) {
      
      cout << "                            \n";
      cout << " Configuration file setting.\n";
      cout << " [Casper/res/casper.json]   \n";
      cout << "                            \n";
      cout << " general:plotOnOff:[on/off] \n";
      cout << "   on:ploting is allowed (gnuplot installation is requested).\n";
      cout << "   off:plotting is not allowed.\n";
      cout << "                            \n";
      cout << " input:inputName:Setup the name of input file,\n";
      cout << " input:inputFormat:[td/]:Setup the format of input file.\n";
      cout << "   td:time-double.\n";
      cout << " input:inputResolution:Setup the time resolution of input time series (in seconds [s]). Eg. 86400 is daily resolution! \n";
      cout << " input:inputConvTdDd:[true/false]\n";
      cout << "   true:convert from time format to double format.\n";
      cout << " input:inputFolder:[address to data folder]. Eg. /Data \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
            cout << " \n";
      
      
      /*
       *   },
       *     "output": {
       *     "outputName": "Casper.txt",
       *     "outputHist": "Casper.hst"
       *   },
       *   "stat": {
       *     "statOnOff": "off",
       *     "iqrCnfd": 2.5
       *   },
       *   "regression": {
       *     "regressOnOff": "off",
       *     "regModel": 3,
       *     "regOrder": 1,
       *     "elimTrend": false,
       *     "elimSeas": true
       *   },
       *   "median": {
       *     "medianOnOff": "on",
       *     "fixedHour": true,
       *     "constHour": 12
       *   },
       *   "reference": {
       *     "referenceOnOff": "off"
       * 
       */
      
      return 0;
   }
   
   
 private:
   
};

#endif