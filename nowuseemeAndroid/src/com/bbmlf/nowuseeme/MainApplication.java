package com.bbmlf.nowuseeme; 
import org.acra.*;
import org.acra.annotation.*;

import android.app.Application;

/*@ReportsCrashes(formKey = "", // will not be used
	mailTo = "bbmlf-crash@nod.cc",
	mode = ReportingInteractionMode.TOAST,
	resToastText = R.string.crashToastText
)*/
@ReportsCrashes(formKey="dGVacG0ydVHnaNHjRjVTUTEtb3FPWGc6MQ",
	mailTo = "bbmlf-crash@nod.cc",
	mode = ReportingInteractionMode.DIALOG,
	resToastText = R.string.crashToastText, // optional, displayed as soon as the crash occurs, before collecting data which can take a few seconds
	resDialogText = R.string.crash_dialog_text,
	resDialogIcon = android.R.drawable.ic_dialog_info, //optional. default is a warning sign
	resDialogTitle = R.string.crash_dialog_title, // optional. default is your application name
	resDialogCommentPrompt = R.string.crash_dialog_comment_prompt, // optional. when defined, adds a user text field input with this text resource as a label
	resDialogOkToast = R.string.crash_dialog_ok_toast // optional. displays a Toast message when the user accepts to send a report.
)

public class MainApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();

        // The following line triggers the initialization of ACRA
        ACRA.init(this);
    }
}