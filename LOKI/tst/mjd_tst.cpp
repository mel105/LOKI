#include "mjd_tst.h"
#include "timeStamp.h"
#include <string>

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

void tstMJD()
{

  cout << "TST 1" << endl;
  string test = "2000-01-01 12:14:22.25"; t_timeStamp tst1(test);
  
  cout
     << " YR " <<  tst1.year() << "\n"
     << " MN " <<  tst1.month() << "\n"
     << " DY " <<  tst1.day() << "\n"
     << " HR " <<  tst1.hour() << "\n"
     << " MN " <<  tst1.minute() << "\n"
     << " SC " <<  tst1.second() << "\n"
     << " JD " <<  fixed<<setprecision(5)<<  tst1.mjd() << "\n"
     << " TS " <<  tst1.timeStamp()
     << endl;   
  
  
  cout << "TST 2" << endl;
  t_timeStamp tst2(2000,1,1,12,14,22.25);
  
  cout
     << " YR " <<  tst2.year() << "\n"
     << " MN " <<  tst2.month() << "\n"
     << " DY " <<  tst2.day() << "\n"
     << " HR " <<  tst2.hour() << "\n"
     << " MN " <<  tst2.minute() << "\n"
     << " SC " <<  tst2.second() << "\n"
     << " JD " <<  fixed<<setprecision(5)<<  tst2.mjd() << "\n"
     << " TS " <<  tst2.timeStamp()
     << endl;   
  
  cout << "TST 3" << endl;
  t_timeStamp tst3(51544.50998); // Note: get only int sec
  
  cout
     << " YR " <<  tst3.year() << "\n"
     << " MN " <<  tst3.month() << "\n"
     << " DY " <<  tst3.day() << "\n"
     << " HR " <<  tst3.hour() << "\n"
     << " MN " <<  tst3.minute() << "\n"
     << " SC " <<  tst3.second() << "\n"
     << " TS " <<  tst3.timeStamp()
     << endl;

}
