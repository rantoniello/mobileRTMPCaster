//
//  ErrorCodes.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__ErrorCodes__
#define __nowuseeme__ErrorCodes__

#include <stdio.h>
#include <map>
#include <string>

extern "C" {
    #include "error_codes.h"
}

class ErrorCodes
{
public:
    const static std::map<_retcodes, std::string> m_map;
};

#endif /* defined(__nowuseeme__ErrorCodes__) */
