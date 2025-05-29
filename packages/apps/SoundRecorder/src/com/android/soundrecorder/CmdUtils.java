package com.android.soundrecorder;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.util.Log;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class CmdUtils {

    private static final String TAG = "CmdUtils";

    public static void execCmd(String cmd) {
        Log.v(TAG, "execCmd " + cmd);
        OutputStream outputStream = null;
        DataOutputStream dataOutputStream = null;
        try {
            Process p = Runtime.getRuntime().exec("sh");
            outputStream = p.getOutputStream();
            dataOutputStream = new DataOutputStream(outputStream);
            dataOutputStream.writeBytes(cmd);
            dataOutputStream.flush();
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (null != dataOutputStream) {
                try {
                    dataOutputStream.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
            if (null != outputStream) {
                try {
                    outputStream.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public static boolean execCmdWithResult(String cmd, String[] results) {
        InputStreamReader reader = null;
        BufferedReader bufferedReader = null;
        String lineText = null;
        try {
            Process process = Runtime.getRuntime().exec(cmd);
            reader = new InputStreamReader(process.getInputStream());
            bufferedReader = new BufferedReader(reader);
            while ((lineText = bufferedReader.readLine()) != null) {
                for (String result : results) {
                    if (lineText.contains(result)) {
                        Log.v(TAG, "execCmd=" + cmd + ", have result=" + lineText);
                        return true;
                    }
                }
                Log.d(TAG, "lineText=" + lineText);
            }
        } catch (Exception e) {
            Log.e(TAG, "process Runtime error!!");
            e.printStackTrace();
        } finally {
            if (null != reader) {
                try {
                    reader.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
            if (null != bufferedReader) {
                try {
                    bufferedReader.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        Log.e(TAG, "execCmd=" + cmd + ", but not result=" + results);
        return false;
    }

    public static boolean execCmd(String cmd, CommandResponseListener listener){
        Process process = null;
        DataOutputStream os = null;
        try{
            process = Runtime.getRuntime().exec(cmd);
            os = new DataOutputStream(process.getOutputStream());
            os.writeBytes("\n");
            os.flush();
            os.writeBytes("exit\n");
            os.flush();
            if(listener!=null){
                InputStream resIn = process.getInputStream();
                InputStream errIn = process.getErrorStream();
                listener.onResponse(resIn, errIn);
            }
            process.waitFor();
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        } finally {
            try {
                if (os != null)   {
                    os.close();
                }
                process.destroy();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return true;
    }

    public interface CommandResponseListener{
        public void onResponse(InputStream resIn, InputStream errIn);

    }
}
