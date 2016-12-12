package launcher.uvdemo.app;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        final Button button = (Button) findViewById(R.id.crash_btn);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                MyLib.boom();
            }
        });

        final TextView text = (TextView) findViewById(R.id.text);
        final Button throwButton = (Button) findViewById(R.id.throw_btn);
        throwButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                try {
                    MyLib.throwException();
                } catch (Error e) {
                    e.printStackTrace();
                    text.setText(e.getMessage());
                }
            }
        });
    }
}
