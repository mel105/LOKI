/************************************************************************************************** 
 Casper - software development library
  
 (c) 2019 Michal Elias
  
 This file is part of the Casper C++ library.
  
 This library is free software; you can redistribute it and/or  modify it under the terms of the GNU
 General Public License as  published by the Free Software Foundation; either version 3 of the 
 License, or (at your option) any later version.
  
 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.
  
 You should have received a copy of the GNU General Public License  along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#include "version.h"

using namespace std;

t_version::t_version(const string & major,
                     const string & minor, 
                     const string & revision,
                     const string & state)
{
   
   _major = major;
   _minor = minor;
   _revision = revision;
   _state = state;
   
   _setVersion();
}


string t_version::Version() { return _version; }

void t_version::_setVersion()
{
  
  _version = ("[") + _major + (".") + _minor + (".") + _revision + (" - ") + _state + ("]");
}

