package launcher.uvdemo.app;

import android.util.Log;

/**
 * Created by Alexander Dalshov on 6/11/16.
 */

public class MyLib {
    static {
        Log.d("mylib", "loading..");
        System.loadLibrary("mylib");
        Log.d("mylib", "mylib loaded");
    }

    public static native void boom();
    public static native void throwException() throws java.lang.Error;
}
