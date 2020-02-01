#include "logger.hpp"

logging::logger< logging::file_log_policy > log_inst( "Casper.log" ,
                                                      logging::severity_type::debug1);
