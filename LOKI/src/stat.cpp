/************************************************************************************************** 
 LOKI - software development library
  
 (c) 019 Michal Elias
  
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

#include  "stat.h"

// constructor
// --------------------
t_stat::t_stat(const vector<double> & data)
{
   /// Default setting
   _mean   = 999.9999;
   _median = 999.9999;
   _1Qt    = 999.9999;
   _3Qt    = 999.9999;
   _iqr    = 999.9999;
   _upp    = 999.9999;
   _low    = 999.9999;
   _sdev   = 999.9999;
   _vare   = 999.9999;
   _min    = 999.9999;
   _max    = 999.9999;
   
   _iqrCnfd = IQR_CNFD_DEF;
   
   /// Set data
  this-> _setData(data);
}

// constructor
// --------------------
t_stat::t_stat(const vector<double> & data, const double & iqrCnfd)
{
   /// Default setting
   _mean   = 999.9999;
   _median = 999.9999;
   _1Qt    = 999.9999;
   _3Qt    = 999.9999;
   _iqr    = 999.9999;
   _upp    = 999.9999;
   _low    = 999.9999;
   _sdev   = 999.9999;
   _vare   = 999.9999;
   _min    = 999.9999;
   _max    = 999.9999;
   
   _iqrCnfd = iqrCnfd;
   
   /// Set data
   this->_setData(data);
}

/// @detail Set & get: Mean
void   t_stat::calcMean(){ this->_calcMean();     }
double t_stat::getMean() { return _mean;          }

/// @detail Set & get: Mode
void   t_stat::calcMode(){ this->_calcMode();     }
vector<double> t_stat::getMode() { return _mode;  }
double t_stat::getModeFreq(){ return _modeF;      }

/// @detail Set & get: Median
void   t_stat::calcMedian(){ this->_calcMedian(); }
double t_stat::getMedian() { return _median;      }

/// @detail Set & get: 1st quartile
void   t_stat::calc1Qt(){ this->_calc1Qt();       }
double t_stat::get1Qt() { return _1Qt;            }
//
/// @detail Set & get: 3rd quartile
void   t_stat::calc3Qt(){ this->_calc3Qt();       }
double t_stat::get3Qt() { return _3Qt;            }

/// @detail Set & get: IQR
void   t_stat::calcIQR(){ this->_calcIQR();       }
double t_stat::getIQR() { return _iqr;            }
double t_stat::getUPP() { return _upp;            }
double t_stat::getLOW() { return _low;            }

/// @detail Set & get: Sdev
void   t_stat::calcSdev(){ this->_calcSdev();     }
double t_stat::getSdev() { return _sdev;          }

/// @detail Set & get: Vare
void   t_stat::calcVare(){ this->_calcVare();     }
double t_stat::getVare() { return _vare;          }

/// @detail Set & get: MinMax
void   t_stat::calcMinMax(){ this->_calcMinMax(); }
double t_stat::getMin() { return _min;            }
double t_stat::getMax() { return _max;            }

/// @detail Set & get: Quantile
double t_stat::calcQuantile(const double reqQuantile)
{ 
   _quantile = -999.9999;
   
   this->_calcQuantile(reqQuantile);
   
   return _quantile;
}

/// @detail Calc Mode
void t_stat::_calcMode()
{
   
   map<double, int> frequency;
   
   for(vector<double>::iterator it = _minmax.begin(); it != _minmax.end(); ++it)
     {
	frequency[*it]++;
     }
   
   vector<double> cetVec, modVec;
   
   for(map<double, int>::iterator j = frequency.begin(); j != frequency.end(); ++j)
     {
	modVec.push_back(j->first);
	cetVec.push_back(j->second);
     }
   
   double maxF = *max_element(cetVec.begin(), cetVec.end()); _modeF = maxF;
   
   for(unsigned int x = 0; x < cetVec.size(); ++x)
     {
	if (cetVec[x] == maxF)
	  {
#ifdef DEBUG
	     cout << "max je "  << cetVec[x] << "  " << modVec[x] << endl;
#endif
	     
	     _mode.push_back(modVec[x]);
	  }
     }
}

/// @detail Calc 1st quartile
void t_stat::_calcQuantile(const double reqQ)
{
   
   size_t N = _minmax.size();
   
   if (reqQ < 0.0 || reqQ > 100.0)
     {
	cout << "Error::stat.cpp::_calcQuantile::Requested Quantile is not supported!" << endl;
	_quantile = -999.9999;
     }
   else if (N < 100)
     {
	cout << "Error::stat.cpp::_calcQuantile::Small data size!"<< endl;
	_quantile = -999.9999;
     }
   else if (reqQ == 0.0)
     {
	_quantile = _minmax[0];
     }
   else if (reqQ == 100.0)
     {
	_quantile = _minmax[N-1];
     }
   else
     {
	double R = (reqQ / 100.0) * (N + 1);
	int    k = int(floor(R));
	double d = R-k;
	
	_quantile = _minmax[k] + d * (_minmax[k+1] - _minmax[k]);
     }
}

/// @detail Calc 1st quartile
void t_stat::_calc1Qt()
{
   int n = _data.size();
   
   if ( n % 4 == 0 )
     {
	int idx_1 = n / 4.0;
	int idx_2 = n / 4.0 + 1.0;
	_1Qt      = ( _minmax[idx_1] + _minmax[idx_2] ) / 2.0;
     }
   else if ( n % 4 != 0 )
     {
	double num = n / 4.0;
	int idx    = (int)ceil(num);
	_1Qt       = _minmax[idx-1];
     }
   else
     {
	cout << "Error::stat.cpp::1Qt::Something wrong!" << endl;
     }
}

                                                                                                  /// @detail Calc 3rd quartile
void t_stat::_calc3Qt()
{
   int n = _data.size();
   
   if ( n % 4 == 0 )
     {
	int idx_1 = 3.0 * n / 4.0;
	int idx_2 = 3.0 * n / 4.0 + 1.0;
	_3Qt      = ( _minmax[idx_1] + _minmax[idx_2] ) / 2.0;
     }
   else if( n % 4 != 0 )
     {
	double num = n / 4.0;
	int idx    = (int)ceil(3.0 * num);
	_3Qt       = _minmax[idx-1];
     }
   else
     {
	cout << "Error::stat.cpp::3Qt::Something wrong!" << endl;
     }
}

/// @detail IQR & upper border & lower border
void t_stat::_calcIQR()
{
   if ( _1Qt == 999.9999 ||
	_3Qt == 999.9999 )
     {
	this->_calc1Qt();
	this->_calc3Qt();
	
	_iqr = _3Qt - _1Qt;
	
	double cnd = _iqr * _iqrCnfd;
	_upp = _3Qt + cnd;
	_low = _1Qt - cnd;
     }
   else
     {
	_iqr = _3Qt - _1Qt;
	
	double cnd = _iqr * _iqrCnfd;
	_upp = _3Qt + cnd;
	_low = _1Qt - cnd;
     }
}

/// @detail Calc mean value
void t_stat::_calcMinMax()
{
   _min = _minmax[0];
   _max = _minmax[_minmax.size()-1];
}

/// @detail Calc mean value
void t_stat::_calcMean()
{
   double sum = 0.0;
   for(unsigned int i = 0; i < _data.size(); ++i)
     {
        sum = sum + _data[i];
     }
   
   _mean = sum / _data.size();
}

/// @detail Calc median value
void t_stat::_calcMedian()
{
   size_t N = _data.size();
   
   if ( N % 2 == 0)
     {
	int i = N / 2;
	int j = i + 1;
	
	_median = (_minmax[i-1] + _minmax[j-1])/2;
     }
   else
     {
	int i = N / 2 + 1;
	_median = _minmax[i-1];
     }
}

/// @detail Calc standard deviation
void t_stat::_calcSdev()
{
   size_t N = _data.size();
   this->_calcMean(); double mean = getMean();
   
   double sum = 0.0;
   
   for(unsigned int i = 0; i < N; ++i)
     {
        sum = sum + pow((_data[i] - mean), 2.0);
     }
   
   _sdev = sqrt(sum / (N - 1));
   
}

/// @detail Calc variation error
void t_stat::_calcVare()
{
   this->_calcSdev(); double stde = getSdev();
   
   _vare = pow(stde, 2.0);
   
}

/// @detail
///   Set data into the protected container
void t_stat::_setData( const vector<double> & data)
{
   _data = data;  //data.clear(); Ako sa tych dat potom zbavit?
   _minmax; sort (_data.begin(), _data.end());
   
   for ( auto x : _data)
     {
	_minmax.push_back(x);
     }
}