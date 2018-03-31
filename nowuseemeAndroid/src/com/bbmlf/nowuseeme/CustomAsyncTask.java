package com.bbmlf.nowuseeme;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Random;

import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.DefaultHttpClient;

import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.AsyncTask;
import android.util.Log;

public class CustomAsyncTask extends AsyncTask<String, Integer, String> {
	protected Context context;
    private ProgressDialog dialog;
	private OnTaskComplete onTaskComplete;
	private String strError = null; 
	
	public interface OnTaskComplete {
		public void setMyTaskComplete(String message, String error);
	}
 
	public void setMyTaskCompleteListener(OnTaskComplete onTaskComplete) {
		this.onTaskComplete = onTaskComplete;
	}
		
	public CustomAsyncTask(Context context){
		final CustomAsyncTask task = this;
		this.context = context;  
		dialog = new ProgressDialog(context);
		dialog.setCancelable(true);
		dialog.setMessage("Loading, please wait...");
		dialog.setOnCancelListener(new DialogInterface.OnCancelListener() {         
		    @Override
		    public void onCancel(DialogInterface dialog) {
		    	task.cancel(true);
				onTaskComplete.setMyTaskComplete(null, "Canceled by user");
		        //do whatever you want the back key to do
		    }
		});
	}
	
	@Override
	protected void onPreExecute() {
		super.onPreExecute();
		if (dialog != null) dialog.show();
	}
	

	@Override
	protected String doInBackground(String... params) {
        String sUrl = params[0] + "";
        StringBuilder res = new StringBuilder();
        String html = null;
        
  		if (sUrl.startsWith("file:///android_asset/")) {
  			try {
				html = getStringFromFile(sUrl.substring(22));
			} catch (Exception e) {
				// TODO Auto-generated catch block
				strError = "Cannot load resource file";
				return null;
			}
  		} else {
  			/*  	public static boolean connectivityCheck(Context context, String sURL) {
  	  		String baseUrl;
//  	  		if ((baseUrl = URLCheck(sURL)) != null) { //not valid URL
//  	  			return true;
//  	  		}

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
//  	                	Log.v(LOG_TAG, "Connectivity check = " + urlc.getResponseCode());
  	                    return false;
  	                }
  	            }
  	        } catch (Exception e) {
//  	        	Log.v(LOG_TAG, "connectivityCheck - " + e.toString());
//  	            e.printStackTrace();
  	        }
  	        return false;
  	    }*/

//	        HttpClient client = new DefaultHttpClient();
//	        HttpGet request = new HttpGet(url);
//	        HttpResponse response = null;
//	        InputStream in = null;
	
	        try {
	        	ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
	        	NetworkInfo networkInfo = cm.getActiveNetworkInfo();
	        	if (!cm.getActiveNetworkInfo().isConnectedOrConnecting() ||
	        			networkInfo == null ||
	        			!networkInfo.isConnected()) {
	        		strError = "Cannot detect an active internet connection";
	        		return null;
	        	}
	        } catch (Exception e) {
        		strError = "Cannot detect an active internet connection";
        		return null;
	        }

	        InputStream is = null;
	        
	        try {
	        	URL url = new URL(sUrl);
	        	HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	        	conn.setReadTimeout(10000);
	        	conn.setConnectTimeout(15000);
	        	conn.setRequestMethod("GET");
	        	conn.setDoInput(true);
	        	conn.connect();
	        	int response = conn.getResponseCode();
	        	Log.d("CustomAsyncTask", "GET response is: " + response);
    		    BufferedReader in = new BufferedReader(new InputStreamReader(conn.getInputStream(), "UTF-8"));
    		    String str;
    	
    		    while ((str=in.readLine()) != null) {
    		      res.append(str);
    		    }
    	
    		    in.close();
	        	html = res.toString();
	        } catch (Exception e) {
				// TODO Auto-generated catch block
				strError = "Cannot read from the server";
				return null;
			} finally {
	        	if (is != null) {
	        		try {
						is.close();
					} catch (IOException e) {}
	        	}
	        }
/*	        try {
	            response = client.execute(request);
	        } catch (ClientProtocolException e1) {
	    		strError = "Invalid response from the server";
	            return null;
	        } catch (IOException e1) {
	    		strError = "I/O related error";
	            return null;
	        } catch (Exception e1) {
	    		strError = "Unknown error (1)";
	            return null;
	        }
	
	        try {
	            in = response.getEntity().getContent();
	        } catch (IllegalStateException e) {
	    		strError = "Invalid permissions";
	            return null;
	        } catch (IOException e) {
	    		strError = "I/O related error";
	            return null;
	        } catch (Exception e) {
	    		strError = "Unknown error (2)";
	            return null;
	        }
	        html = null;
	
	        BufferedReader reader = null;
	        try {
	            reader = new BufferedReader(new InputStreamReader(in));
	        } catch (Exception e) {
	
	//            this.publishProgress();
	//            this.cancel(true);
	    		strError = "Cannot read from the server";
	            return null;
	        }
	
	        StringBuilder str = new StringBuilder();
	        String line = null;
	
	        try {
	            while ((line = reader.readLine()) != null) {
	                str.append(line);
	            }
	        } catch (IOException e) {
	    		strError = "Connection with the server was reset";
	            return null;
	        }
	
	        try {
	            in.close();
	        } catch (IOException e) {}
	        
	        html = str.toString();*/
  		}
        return html;
	}
	
	@Override
	protected void onPostExecute(String message) {
		if (dialog != null) dialog.dismiss();
		onTaskComplete.setMyTaskComplete(message, strError);
	}
	
	protected String getStringFromFile (String filePath) throws Exception {
	    StringBuilder buf = new StringBuilder();
		try {
		    InputStream fileObj = context.getAssets().open(filePath);
		    BufferedReader in = new BufferedReader(new InputStreamReader(fileObj, "UTF-8"));
		    String str;
	
		    while ((str=in.readLine()) != null) {
		      buf.append(str);
		    }
	
		    in.close();
		} catch (Exception e) {
			Log.v("getStringFromFile", filePath + " - " + e.toString());
		}
	    return buf.toString();
	}
	
	// Reads an InputStream and converts it to a String.
	public String readIt(InputStream stream, int len) throws IOException, UnsupportedEncodingException {
	    Reader reader = null;
	    reader = new InputStreamReader(stream, "UTF-8");        
	    char[] buffer = new char[len];
	    reader.read(buffer);
	    return new String(buffer);
	}
}
