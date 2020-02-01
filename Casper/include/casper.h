/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: 
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class t_casper
{
   
 public:
   
   void OpenFile(const std::string & filePath);
   
   template<typename T>
     T getValue(const std::initializer_list<std::string> & il);
   
 private:
   std::ifstream _fs;
   json _pj;
};

template<typename T>
inline T t_casper::getValue(const std::initializer_list<std::string> & il)
{
   json::value_type tmp;
   
   for (const auto & elm : il)
   {
      tmp = tmp.empty() ? _pj[elm] : tmp[elm];
   }

   return tmp.is_null() ? static_cast<nlohmann::json>(0) : tmp.front();
}