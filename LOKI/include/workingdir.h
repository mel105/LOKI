/**************************************************************************************************
 @Founder: Michal Elias
 @Linux Ubuntu 18.4.2 LTS
 @Last revision: 2017-03-06 10:08:45 Author: Michal Elias  

 @Brief: Returns actual path to working dir.
 @Details:
  
 @Reference:
***************************************************************************************************/

#include <stdio.h>  /* defines FILENAME_MAX */
// #define WINDOWS  /* uncomment this line to use it for windows.*/
 
#ifdef WINDOWS
  #include <direct.h>
  #define GetCurrentDir _getcwd
#else
  #include <unistd.h>
  #define GetCurrentDir getcwd
#endif

#include <iostream>

///@brief get current working folder address
///@reference http://www.codebind.com/cpp-tutorial/c-get-current-directory-linuxwindows/

class t_workingDir
{
   
public:   
   std::string GetCurrentWorkingDir( void ) 
   {
      char buff[FILENAME_MAX];
      GetCurrentDir( buff, FILENAME_MAX );
      std::string current_working_dir(buff);
      
      return current_working_dir;
   }
   
private:
   
};