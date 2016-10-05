package launcher.uvdemo.app;

import android.util.Log;

/**
 * Created by mac on 6/11/16.
 */

public class MyLib {
    static {
        Log.d("mylib", "loading..");
        System.loadLibrary("mylib");
        Log.d("mylib", "mylib loaded");
    }

    public static native void initialize();
}
