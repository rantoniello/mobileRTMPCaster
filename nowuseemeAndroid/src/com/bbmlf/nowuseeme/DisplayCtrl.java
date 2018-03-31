package com.bbmlf.nowuseeme;

import android.util.DisplayMetrics;
import android.util.Log;
import android.view.ViewGroup;

/**
 * final ("static") class DisplayCtrl
 * - declared final for preventing extension of the class
 * - constructor declared private for avoiding class instantiation
 * - all methods and attributes should be constant
 */
public final class DisplayCtrl
{
	private final static String LOG_TAG= "DisplayCtrl";
	public final static int MAX_WAITTIME_MILLIS= 10*1000;

	/**
	 *  private constructor: prevents instantiation by client code as it makes 
	 *  no sense to instantiate a "static" class
	 */
	private DisplayCtrl()
	{
	}

	public static void changeDisplayParams(
			final android.view.View view, // view to be modified
			final MainActivity parentActivity, // access to main activity publics 
			final boolean display, // show or hide view 
			final int width, final int height, // change view resolution
			final int x, final int y // change view position
			)
	{
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
	}
}
