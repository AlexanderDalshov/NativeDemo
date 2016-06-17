package launcher.uvdemo.myapplication;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

import static launcher.uvdemo.myapplication.MyLib.initialize;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        MyLib.initialize();
    }
}
