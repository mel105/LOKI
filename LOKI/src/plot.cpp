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

#include "gnuplot.h"
#include "plot.h"

void t_plot::acf()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/eps/acf.eps";
   
   // plot
   plot("set term post eps color solid 'Helvetica,25'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 1.0,1.0");
   plot("set style line 11 lc rgb '#808080' lt 1");
   plot("set border 3 back lt -1");
   plot("set tics nomirror");
   plot("set yrange [-1:1]");
   plot("set ytics 0.2");
   plot("set noborder");
   plot("set ylabel 'ACF' font 'Arial-Bold,bold,40' offset 1.5,0 rotate by 90");
   plot("set xtics 10 offset 0,-6.0");
   plot("set xlabel 'LAG' font 'Arial-Bold,bold,20'");
   plot("set xtics axis");
   plot("set xzeroaxis lt -1 lw 2");
   plot("set yzeroaxis lt -1 lw 2");
   plot("set ytics axis");
   plot("unset grid");
   plot("plot \'/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/acf.pt\' u 1:2 not ' ' w impulses lc rgb 'black' lw 7");
}

void t_plot::hTime()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/hTime.eps";
   
   // plot
   plot("set term post eps color noenhanced solid 'Helvetica,12'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 3.0,1.0");
   plot("set style fill solid 0.5");
  //
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/homogenDD\' u 1:2 not ' ' w l lc rgb 'black'");
}

void t_plot::hDeseas()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/hDeseas.eps";
   
   // plot
   plot("set term post eps color noenhanced solid 'Helvetica,12'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 3.0,1.0");
   plot("set style fill solid 0.5");
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/homogenDD\' u 1:3 not ' ' w l lc rgb 'black'");
}

void t_plot::hTK()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/hTK.eps";
   
   // plot
   plot("set term post eps color noenhanced solid 'Helvetica,12'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 3.0,1.0");
   plot("set style fill solid 0.5");
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/homogenDD\' u 1:4 not ' ' w l lc rgb 'black' ,\'' u 1:5 not ' ' w l lc rgb 'red' lw 3");
}



/// @Detail
///   - plot regression model with outliers
void t_plot::outlier()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/eps/outliers.eps";
   
   // plot
   plot("set term post eps color noenhanced solid 'Helvetica,12'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 3.0,1.0");
   plot("set style fill solid 0.5");
   plot("plot \'/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/outliersDD\' u 1:2 t 'Original TS' w l lc rgb 'black' ,\'' u 1:3 t 'Regression' w l lc rgb '#09ad00' lw 3 ,\'' u 1:4 not w l lc rgb '#09ad00' lw 1 ,\'' u 1:5 not '' w l lc rgb '#09ad00' lw 1");

}

/// @Detail
///   - plot trend plot
void t_plot::trend()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/eps/trend.eps";
   
   // plot
   plot("set term post eps color noenhanced solid 'Helvetica,12'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 1.0,1.0");
   plot("set multiplot layout 2, 1");
   plot("set style fill solid 0.5");
   plot("unset xtics");
   plot("set tmargin 3");
   plot("set rmargin 3.5");
   plot("set size 1.0,0.5");
   plot("set origin 0,0.5");
   plot("plot \'/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/trendDD\' u 1:2 t 'Original TS' w l lc rgb 'black' ,\'' u 1:3 t 'Linear trend' w l lc rgb '#09ad00' lw 3");
   plot("set size 1.0,0.5");
   plot("set origin 0,0");
   plot("unset tmargin");
   plot("plot \'/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/trendDD\' u 1:4 t 'De-trended TS' w l  lc rgb 'black'");
   
   plot("unset multiplot");
 
}

void t_plot::median()
{
  
  // declaration
  t_gnuplot plot;
  
  // output
  string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/median.eps";
  
  // plot
  plot("set term post eps color noenhanced solid 'Helvetica,12'");
  plot(("set output '"+out+"'").c_str());
  plot("set size 1.0,1.0");
  plot("set multiplot layout 2, 1");
  plot("set style fill solid 0.5");
  plot("unset xtics");
  plot("set tmargin 3");
  plot("set rmargin 3.5");
  plot("set size 1.0,0.5");
  plot("set origin 0,0.5");
  plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/medianDD\' u 1:2 t 'Original TS' w l lc rgb 'black' ,\'' u 1:3 t 'Median model' w l lc rgb '#09ad00' lw 3");
  plot("set size 1.0,0.5");
  plot("set origin 0,0");
  plot("unset tmargin");
  plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/medianDD\' u 1:4 t 'De-seasonalised TS' w l  lc rgb 'black'");
  
  plot("unset multiplot");
}

void t_plot::seas()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/seas.eps";
   
   // plot
   plot("set term post eps color noenhanced solid 'Helvetica,12'");
   plot(("set output '"+out+"'").c_str());
   plot("set size 1.0,1.0");
   plot("set multiplot layout 2, 1");
   plot("set style fill solid 0.5");
   plot("unset xtics");
   plot("set tmargin 3");
   plot("set rmargin 3.5");
   plot("set size 1.0,0.5");
   plot("set origin 0,0.5");
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/seasDD\' u 1:2 t 'Original TS' w l lc rgb 'black' ,\'' u 1:3 t 'Reg model' w l lc rgb '#09ad00' lw 3");
   plot("set size 1.0,0.5");
   plot("set origin 0,0");
   plot("unset tmargin");
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/seasDD\' u 1:4 t 'De-seasonalised TS' w l  lc rgb 'black'");
   
   plot("unset multiplot");
   
//   plot(" \' \' u 1:3 t 'Linear trend' w l");
     
//   plot("plot \'/home/michal/Work/LOKI/LOKI/LOKI/gnuplot/trendDD\' u 1:2 t 'Original ts' w l , u 1:3 t 'Linear trend' w l");
}

void t_plot::line()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/line.eps";
   
   // plot
   plot("set term postscript eps");
   plot(("set output '"+out+"'").c_str());
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/line\' u 1:2 not w l");
}

void t_plot::tline()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/line-synt.eps";
   
   // plot
   plot("set term postscript eps");
   plot(("set output '"+out+"'").c_str());
   plot("set xdata time");
   plot("set timefmt \'%Y-%m-%d %H:%M:%S\'");
   plot("set format x \'%Y\'");
   plot("set xtics rotate by 0");
   plot("set xlabel \'TIME\' offset 1.0,0 rotate by 0 font \'Arial-Bold, bold, 40\'");
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/line\' u 1:3 not w l");
}


/// @Detail
///   - Plot histogram.
///   - Input: min&max values of histogram
void t_plot::histogram(const double & min,
		       const double & max) // eventualne este N?
{
   // from int to string
   string MIN; stringstream smin; smin << min; MIN = smin.str();
   string MAX; stringstream smax; smax << max; MAX = smax.str();   
   // declaration
   t_gnuplot plot;
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/histogram.eps";
   // plot
   plot("set term postscript eps");
   plot(("set output '"+out+"'").c_str());
   plot("n=30");
   plot("max='"+MAX+"'");
   plot("min='"+MIN+"'");
   plot("width=(max-min)/n");
   plot("hist(x,width)=width*floor(x/width)+width/2.0");
   plot("set boxwidth width*0.9");
   plot("set style fill solid 0.5 # fill style");
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/line\' u (hist($2,width)):(1.0) smooth freq w boxes lc rgb \'red\' not");
}

/// @Detail
///   - Plot histogram.
///   - Input: min&max values of histogram
void t_plot::boxplot()
{
   // declaration
   t_gnuplot plot;
   
   // output
   string out = "/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/eps/boxplot.eps";
   // plot
   plot("set term postscript eps");
   
   plot("set style fill solid 0.25 border -1");
   plot("set style boxplot outliers pointtype 7");   
   plot("set style data boxplot");
   plot("unset xtics");
   plot(("set output '"+out+"'").c_str());
   plot("plot \'/home/michal/Work/gitHub/LOKI/LOKI/gnuplot/line\' u (0):2 not");
}
