//
//  Preferences.h
//  nowuseeme
//
//  
//

#ifndef __nowuseeme__Preferences__
#define __nowuseeme__Preferences__

#include <stdio.h>
#include <string>

using namespace std;

class MainActivity;

class Preferences
{
private:
    MainActivity *m_parentActivity;

public:
    Preferences(MainActivity *activity);
    ~Preferences();

    void open();
    void onFirstRun();
};

#endif /* defined(__nowuseeme__Preferences__) */
