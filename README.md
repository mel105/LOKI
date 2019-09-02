# LOKI

LOKI is the application designed to analyse climatological (temperature, pressure, water vapor pressure), GNSS (atmospheric delays, IWV) or other time-series.

##### Core developer: Michal Elias 
##### Email: misko_elias@yahoo.com 
##### Web: TBD 
##### Version: [0.0.11.-alpha]
##### Manual: TBD

LOKI uses Third party tools/libraries
* JSON parser [1] to create a parser
* Logger [2] to create a logger file
* Newmat library [3], [4] for matrix calculations

> sudo apt-get install libnewmat10-dev or see [4]

* GnuPlot [5] to create a plots

> sudo apt-get install gnuplot

### Run LOKI

1. cd Development
2. cmake ../CMakeLists.txt (*)
3. cd ..
4. make
5. ./Build/LOKI

##### (*) Problem with builder
```
The CXX compiler identification is unknown
CMake Error at CMakeLists.txt:XXX (project):
No CMAKE_CXX_COMPILER could be found.
Tell CMake where to find the compiler by setting either the environment
variable "CXX" or the CMake cache entry CMAKE_CXX_COMPILER to the full
path
to the compiler, or to the compiler name if it is in the PATH.


Configuring incomplete, errors occurred!
```

##### Solution see [6].
> sudo apt-get update

>sudo apt-get install -y build-essential


##### References
1. https://nlohmann.github.io/json/
2. 
3. http://www.robertnz.net/nm_intro.htm
4. https://people.mech.kuleuven.be/~tdelaet/bfl_doc/installation_guide/node6.html
5. http://www.gnuplot.info/
6. https://stackoverflow.com/questions/31421327/cmake-cxx-compiler-broken-while-compiling-with-cmake
