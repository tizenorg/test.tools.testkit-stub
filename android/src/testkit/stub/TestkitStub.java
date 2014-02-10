/*
 * Copyright (C) 2013-2014 Intel Corporation, All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package testkit.stub;

import java.io.DataInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.widget.TextView;

public class TestkitStub extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        TextView tv = new TextView(this);
        tv.setClickable(false);
        setContentView(tv);

        String stub = "testkit-stub";
        String path = getFilesDir().getPath() + "/";
        try {// if can't open, then create it
			FileInputStream fis = openFileInput(stub);
			try {fis.close();} catch (IOException e) {}
		} catch (FileNotFoundException e1) {
			try {
				InputStream input = getAssets().open("bins/" + Build.CPU_ABI + "/" + stub); // create stub according to ABI
				FileOutputStream fo = openFileOutput(stub, 0);
				int length = input.available();
				byte [] buffer = new byte[length];// how to delete buffer?
				input.read(buffer);
				input.close();
				fo.write(buffer);
				fo.flush();
				fo.close();
				//getAssets().close(); // this will cause fc if launch again
				Runtime.getRuntime().exec("chmod 744 " + path + stub);
	        } catch (IOException e) {
				tv.setText(e.getMessage());
				e.printStackTrace();
			}
		}

        try {
			Process p = Runtime.getRuntime().exec(path + stub);
			String line = "", res = "";

			InputStream input = p.getInputStream();
			DataInputStream osRes = new DataInputStream(input);
			while((line = osRes.readLine()) != null) res += line + "\n";
			osRes.close();
			input.close();

			input = p.getErrorStream();
			osRes = new DataInputStream(input);
			while((line = osRes.readLine()) != null) res += line + "\n";
			osRes.close();
			input.close();

			tv.setText(res);
        } catch (IOException e) {
			tv.setText(e.getMessage());
			e.printStackTrace();
		}
    }
}
