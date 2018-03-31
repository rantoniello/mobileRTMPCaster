//
//  DisplayCtrl.cpp
//  nowuseeme
//
//  
//

#include "DisplayCtrl.h"

#include "MainActivity.h"
extern "C" {
    #include "log.h"
    #include "AVViewDelegate_ciface.h"
}

/*static*/ int DisplayCtrl::MAX_WAITTIME_MILLIS= 10*1000;

/*static*/ void DisplayCtrl::changeDisplayParams(MainActivity *parentActivity, // access to main activity publics
                                                 bool display, // show or hide view
                                                 int width, int height, // change view resolution
                                                 int x, int y, // change view position
                                                 std::string type
)
{
    LOGV("Showing display view (view pointer: %p)\n", parentActivity->getMainUIView());
    setupAVSublayerLayer(parentActivity->getMainUIView(), display? 1: 0, width, height, x, y, type.c_str());
//FIXME!!
#if 0
    Runnable onUiRunnable= new Runnable()
    {
        @Override
        public void run()
        {
            // Set new resolution parameters
            ViewGroup.LayoutParams params= view.getLayoutParams();
            if(params== null)
            {
                Log.e(LOG_TAG, "'view.getLayoutParams()' parameters returned 'null'");
                parentActivity.setLastErrorCode(ErrorCodes.SET_PREVIEW_DISPLAY_ERROR);
            }
            else
            {
                DisplayMetrics m= MainActivity.getDisplayMetrics();
                
                // Set new position parameters
                if(display && (x!= view.getX() || y!= view.getY()) )
                {
                    // set new visible position
                    Log.v(LOG_TAG, "Showing display view (x="+ x+ "y="+ y+ ")");
                    view.setX(x);
                    view.setY(y);
                }
                else if(!display && (x!= m.widthPixels || y!= m.heightPixels) )
                {
                    // hide view taking it outside of visible bounds
                    Log.v(LOG_TAG, "Hiding display view (x="+ m.widthPixels+ "y="+ m.heightPixels+ ")");
                    view.setX(m.widthPixels);
                    view.setY(m.heightPixels);
                }
                
                //change size, call setLayoutParams (expensive)
                if(width!= -1 && height!= -1 &&
                   (!display || width!= params.width || height!= params.height) )
                {
                    // Updates UI layout
                    Log.v(LOG_TAG, "Updating layout resolution (width="+ width+ "height="+ height+ ")");
                    //RAL: some devices do not accept drawing outside the physical area,
                    //     then we set w=h=1 as a hack for hiding view
                    params.width= display? width: 1;
                    params.height= display? height: 1;
                    view.setLayoutParams(params);
                }
                
                Log.v(LOG_TAG, "New display parameters set O.K.");
            }
            synchronized(this) { this.notify(); }
        }
    };
    synchronized(onUiRunnable)
    {
        parentActivity.runOnUiThread(onUiRunnable);
        try
        {
            onUiRunnable.wait(MAX_WAITTIME_MILLIS); // unlocks myRunable while waiting
        }
        catch(InterruptedException e)
        {
            Log.e(LOG_TAG, "'onUiRunnable.wait()' was interrupted");
        }
    }
    Log.v(LOG_TAG, "Joined onUiRunnable thread (new display parameters set)");
#endif
}
