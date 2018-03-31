package com.bbmlf.nowuseeme;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.util.AttributeSet;
import android.util.Log;
import android.webkit.ConsoleMessage;
import android.webkit.JsPromptResult;
import android.webkit.JsResult;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.Toast;

import com.bbmlf.nowuseeme.CustomAsyncTask;

public class WebViewMod extends WebView {
	private final static String LOG_TAG= "WebViewMod";
    private EditText mFocusDistraction;
    private Context mContext;
    private boolean _toClearHistory= false;
    protected String lastRequestedUrl;

    public WebViewMod(Context context) {
        super(context);
        init(context);
    }    

    public WebViewMod(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }    

    public WebViewMod(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(context);
    }

    public WebViewMod(Context context, AttributeSet attrs, int defStyle, boolean privateBrowsing) {
        super(context, attrs, defStyle, privateBrowsing);
        init(context);
    }

    public void init(Context context) {
    	Log.v(LOG_TAG, "init");

    	// This lets the layout editor display the view.
    	if (isInEditMode()) return;

    	mContext = context;

    	mFocusDistraction = new EditText(context);
    	mFocusDistraction.setBackgroundResource(android.R.color.transparent);
    	this.addView(mFocusDistraction);
    	mFocusDistraction.getLayoutParams().width = 1;
    	mFocusDistraction.getLayoutParams().height = 1;

    	final WebSettings webSettings = getSettings();
    	webSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
    	webSettings.setJavaScriptEnabled(true);
    	webSettings.setBuiltInZoomControls(true);
    	//              webSettings.setLoadWithOverviewMode(true);
    	//              webSettings.setUseWideViewPort(true);
    	final Activity activity = ((MainActivity)mContext);
    	setWebChromeClient(new WebChromeClient(){
    		public void onProgressChanged(WebView view, int progress) {
    			activity.setTitle("Loading ...");
    			activity.setProgress(progress * 100);
    			if (progress == 100) {
    				activity.setTitle(R.string.ApplicationName);
    			}
    		}

    		@Override
    		public void onCloseWindow (WebView window) {
    			((MainActivity)mContext).finish();
    		}

    		@Override
    		public boolean onConsoleMessage (ConsoleMessage consoleMessage) {
    			Log.d(LOG_TAG, "JS console message: " + consoleMessage.sourceId() + ":" +  consoleMessage.lineNumber() + " - " +
    					consoleMessage.message());
    			return true;
    		}

    		@Override
    		public boolean onJsAlert (WebView view, String url, String message, 
    				final JsResult result) {
    			new AlertDialog.Builder((MainActivity)mContext)
    			.setTitle("Alert")
    			.setMessage(message)
    			.setPositiveButton("OK", new AlertDialog.OnClickListener() {
    				public void onClick(DialogInterface dialog, int which) {
    					// do your stuff here
    					result.confirm();
    				}
    			}).setCancelable(false).create().show();
    			return true;			
    		}

    		@Override
    		public boolean onJsConfirm(WebView view, String url, String message,
    				final JsResult result) {
    			new AlertDialog.Builder((MainActivity)mContext)
    			.setTitle("Alert")
    			.setMessage(message)
    			.setPositiveButton("OK", new AlertDialog.OnClickListener() {
    				public void onClick(DialogInterface dialog, int which) {
    					// do your stuff here
    					result.confirm();
    				}
    			}).setCancelable(false).create().show();
    			return true;
    		}

    		@Override
    		public boolean onJsPrompt(WebView view, String url, String message,
    				String defaultValue, final JsPromptResult result) {
    			new AlertDialog.Builder((MainActivity)mContext)
    			.setTitle("Alert")
    			.setMessage(message)
    			.setPositiveButton("OK", new AlertDialog.OnClickListener() {
    				public void onClick(DialogInterface dialog, int which) {
    					// do your stuff here
    					result.confirm();
    				}
    			}).setCancelable(false).create().show();
    			return true;
    		}
    	});

    	//m_webView.getSettings().setAllowUniversalAccessFromFileURLs(true);
    	addJavascriptInterface(new Api((MainActivity)mContext), "nowuseeme");
    	setWebViewClient(new WebViewClient(){

    		@Override
    		public void onPageFinished(WebView view, String url) {
    			if (_toClearHistory && !url.equals("about:blank")) {
    				_toClearHistory = false;
    				clearHistory();
    			}
    			super.onPageFinished(view, url);
    			//        				titlebar.setText(url);
    		}

    		public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
    			super.onReceivedError(view, errorCode, description, failingUrl);
    			Log.v(LOG_TAG,"on received error");
    			if (((MainActivity)mContext).getLastToast()!= null) {
    				try { ((MainActivity)mContext).getLastToast().cancel();} catch (Exception e){}
    				((MainActivity)mContext).setLastToast(null);
    			}
    			Toast t= Toast.makeText(activity, "Error loading external webpage. Please restart the application.", Toast.LENGTH_SHORT);
    			t.show();
    			((MainActivity)mContext).setLastToast(t);
    		}

    		@Override
    		public WebResourceResponse shouldInterceptRequest(WebView view, String url) {
    			@SuppressWarnings("deprecation")
    			WebResourceResponse response = super.shouldInterceptRequest(view, url);
    			String assetPath = "file:///android_asset/webkit/android-weberror.png";
    			if(url != null && url.contains(assetPath)) {
    				try {
    					response = new WebResourceResponse("image/png", "UTF8", ((MainActivity)mContext).getBaseContext().getAssets().open(assetPath));
    				} catch (IOException e) {
    					e.printStackTrace(); // Failed to load asset file
    				}
    			}
    			return response;
    		}

    		@Override
    		public boolean shouldOverrideUrlLoading(WebView view, String url) {
    			if (!lastRequestedUrl.equals(url)) {
    				Log.v(LOG_TAG, "external link");
    				openWebBrowser(url);
    				return true;
    			}
    			Log.v(LOG_TAG, "shouldOverrideUrlLoading - " + url);
    			loadUrl2(url, false);
    			return true;
    			/*	   	   		if (connectivityCheck(getBaseContext(), url)) {
//        					titlebar.setText("Web Page Loading...");
        	   	   			super.shouldOverrideUrlLoading(view, url);
        	   	   			return true;
        	   	   		} else {
        	   	   			Toast errToast = Toast.makeText(getBaseContext(), R.string.UrlInaccessible, Toast.LENGTH_SHORT);
        	   	   			errToast.show();
        	   	   			return false;
        	   	   		}*/
    			//        				m_webView.loadUrl(url);
    		}
    	});
    }

    public void openWebBrowser(String sURL) {
    	Uri uri = Uri.parse(sURL);
    	Intent launchBrowser = new Intent(Intent.ACTION_VIEW, uri);
    	((MainActivity)mContext).startActivity(launchBrowser);
    }

    @Override
    public void loadUrl(final String url) {
    	if (mContext instanceof Activity && this.isFocused()) {
    		((Activity)mContext).runOnUiThread(new Runnable() {
    			@Override
    			public void run() {
    				mFocusDistraction.requestFocus();
    				WebViewMod.super.loadUrl(url);
    				WebViewMod.this.requestFocus();
    			}
    		});
    	} else {
    		super.loadUrl(url);
    	}
    }

    public void loadUrl2(final String url, final boolean clearFirst) {
    	if (url.startsWith("file:///android_asset/")) {
    		if (clearFirst) loadUrl("about:blank");
    		loadUrl(url);
    		lastRequestedUrl = url;
    		return;
    	}

    	//  		URL url = new URL(s1);
    	//  		String path = url.getFile().substring(0, url.getFile().lastIndexOf('/'));
    	//  		String base = url.getProtocol() + "://" + url.getHost() + path;

    	if (URLCheck(url) != null) { //not valid URL
    		CustomAsyncTask mytask = new CustomAsyncTask((MainActivity)mContext);
    		mytask.execute(url);
    		mytask.setMyTaskCompleteListener(new com.bbmlf.nowuseeme.CustomAsyncTask.OnTaskComplete() {
    			@Override
    			public void setMyTaskComplete(final String message, String error) {
    				//Log.v(LOG_TAG, "loadUrl : " + message.substring(message.length() - 10) + " - " + error);
    				if (error != null) {
    					if (((MainActivity)mContext).getLastToast()!= null) {
    						try { ((MainActivity)mContext).getLastToast().cancel();} catch (Exception e){}
    						((MainActivity)mContext).setLastToast(null);
    					}
    					Toast t= Toast.makeText(((MainActivity)mContext).getBaseContext(), R.string.UrlInaccessible, Toast.LENGTH_LONG); 
    					t.show();
    					((MainActivity)mContext).setLastToast(t);

    					if (lastRequestedUrl == null) {
    						((MainActivity)mContext).finish();
    					}
    				} else {
    					lastRequestedUrl = url;
    					loadUrl3(url, message, clearFirst);

    					/*				  		final Map<String, String> noCacheHeaders = new HashMap<String, String>(2);

				  		if (m_disableCache) {
					  	    noCacheHeaders.put("Pragma", "no-cache");
					  	    noCacheHeaders.put("Cache-Control", "no-cache");
				  		}

				  		m_webView.post(new Runnable() {
				            @Override
				            public void run() {
				                m_webView.loadUrl(url, noCacheHeaders);
				            }
				        });*/					
    				}
    			}
    		});
    		//	    	Toast.makeText(this, "This toast will most probably execute before MyTask callback", Toast.LENGTH_SHORT).show();
    	}

    	//  	    m_webView.loadUrl(url, noCacheHeaders);
    }

    protected void loadUrl3(final String baseUrl, final String data, final boolean clearFirst) {
    	post(new Runnable() {
    		@Override
    		public void run() {
    			loadUrl("about:blank");
    			if (data.length() > 5) {
    				Log.v(LOG_TAG, "loadUrl3: " + baseUrl + " | " + data.substring(0, 5) + " | " +
    						data.substring(data.length() - 5));
    			} else {
    				Log.v(LOG_TAG, "loadUrl3: " + baseUrl + " | " + data);
    			}
    			loadDataWithBaseURL(baseUrl, data, "text/html", "UTF-8", null);
    		}
    	});
    }

    public static String URLCheck(String sURL) {
    	String baseUrl;
    	try {
    		URL url = new URL(sURL);
    		baseUrl = url.getProtocol() + "://" + url.getHost();
    		return baseUrl;
    	} catch (Exception e) {}
    	return null;
    }

    public static boolean connectivityCheck(Context context, String sURL) {
    	String baseUrl;
    	if ((baseUrl = URLCheck(sURL)) != null) { //not valid URL
    		return true;
    	}

    	try {
    		ConnectivityManager cm = (ConnectivityManager) context

    				.getSystemService(Context.CONNECTIVITY_SERVICE);

    		if (cm.getActiveNetworkInfo().isConnectedOrConnecting()) {

    			URL url = new URL(baseUrl);
    			HttpURLConnection urlc = (HttpURLConnection) url
    					.openConnection();
    			urlc.setConnectTimeout(3000); // mTimeout is in seconds

    			urlc.connect();

    			if (urlc.getResponseCode() == 200) {
    				return true;
    			} else {
    				Log.v(LOG_TAG, "Connectivity check = " + urlc.getResponseCode());
    				return false;
    			}
    		}
    	} catch (Exception e) {
    		Log.v(LOG_TAG, "connectivityCheck - " + e.toString());
    		//            e.printStackTrace();
    	}
    	return false;
    }
}