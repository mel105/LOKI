/************************************************************************************************** 
 LOKI - software development library
  
 (c) 2019 Michal Elias
  
 This file is part of the loki C++ library.
  
 This library is free software; you can redistribute it and/or  modify it under the terms of the GNU
 General Public License as  published by the Free Software Foundation; either version 3 of the 
 License, or (at your option) any later version.
  
 This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 even the implied warranty of  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.
  
 You should have received a copy of the GNU General Public License  along with this program; if not,
 see <http://www.gnu.org/licenses>.
***************************************************************************************************/

#pragma once

#include "loki.h"

using json = nlohmann::json;

void t_loki::OpenFile(const std::string & filePath)
{
   _fs = std::ifstream(filePath);

   if (!_fs)
   {
      
      std::runtime_error("Failed to open file: [" + filePath + "]!");
   }
   
   _pj = json::parse(_fs);
}
