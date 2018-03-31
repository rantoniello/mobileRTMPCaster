//
//  Preferences.cpp
//  nowuseeme
//
//  
//

#include "Preferences.h"

#include "MainActivity.h"

extern "C" {
    #include "MainActivity_ciface.h"
    #include "Preferences_ciface.h"
}

Preferences::Preferences(MainActivity *activity):
    m_parentActivity(activity)
{
}

Preferences::~Preferences()
{
}

/**
 * Open 'Preferences' class.
 */
void Preferences::open()
{
    if(Preferences_open_ciface()== 1)
    {
        onFirstRun();
    }
}

/**
 * TODO
 */
void Preferences::onFirstRun()
{
    // TODO
    return;
}
