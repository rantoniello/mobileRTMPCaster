//
//  DisplayCtrl.h
//  nowuseeme
//
//  Created by ral on 4/5/15.
//  
//

#ifndef __nowuseeme__DisplayCtrl__
#define __nowuseeme__DisplayCtrl__

#include <stdio.h>
#include <string>

class MainActivity;

/**
 * final ("static") class DisplayCtrl
 * - constructor declared private for avoiding class instantiation
 * - all methods and attributes should be constant
 */
class DisplayCtrl
{
public:
    static int MAX_WAITTIME_MILLIS;
    static void changeDisplayParams(MainActivity *parentActivity, // access to main activity publics
                                    bool display, // show or hide view
                                    int width, int height, // change view resolution
                                    int x, int y, // change view position
                                    std::string type
    );

private:
    /**
     *  private constructor: prevents instantiation by client code as it makes
     *  no sense to instantiate a "static" class
     */
    DisplayCtrl() { }
    ~DisplayCtrl() { }
};

#endif /* defined(__nowuseeme__DisplayCtrl__) */
