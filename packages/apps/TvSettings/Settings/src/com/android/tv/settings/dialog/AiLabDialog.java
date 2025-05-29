package com.android.tv.settings;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.SystemProperties;
import android.util.Log;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.View.OnFocusChangeListener;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatDialog;
import androidx.constraintlayout.widget.Group;
import androidx.fragment.app.DialogFragment;

import com.android.tv.settings.R;

import java.util.Locale;

public class AiLabDialog extends AppCompatDialog implements SeekBar.OnSeekBarChangeListener,
        CompoundButton.OnCheckedChangeListener, OnFocusChangeListener {
    private static final String TAG = "AiLabDialog";

    // needed properties
    public static final String PROP_AI_SVEP_MODE  = "persist.sys.svep.mode";
    public static final String PROP_AI_SVEP_CONTRAST_MODE  = "persist.sys.svep.contrast_mode";
    public static final String PROP_AI_SVEP_CONTRAST_RATIO  = "persist.sys.svep.contrast_offset_ratio";
    public static final String PROP_AI_SVEP_ENHANCE_RATE  = "persist.sys.svep.enhancement_rate";

    private CheckBox mCheckBoxSvepMode, mCheckBoxSvepContrastMode, mCheckBoxSvepGlobalUI;
    private SeekBar  mSeekBarSvepContrastRatio, mSeekBarSvepEnhanceRate;
    private TextView mTxtSvepContrastRatioValue, mTxtSvepEnhanceRateValue;
    private TextView mTxtSvepMode, mTxtSvepContrastMode, mTxtSvepGlobalUi;
    private TextView mTxtSvepContrastRatio, mTxtSvepEnhanceRate;
    private LinearLayout mLayoutSvepMode, mLayoutSvepGlobalUI, mLayoutSvepContrastMode;
    private LinearLayout mLayoutSvepContrastRatio, mLayoutSvepEnhanceRate;

    private boolean mUpdatedInner = false;

    public AiLabDialog(Context context) {
        this(context, 0);
    }

    public AiLabDialog(Context context, int theme) {
        super(context, theme);
        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);
        setContentView(View.inflate(context, R.layout.layout_ai_lab_dialog, null));
        getWindow().setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY);
        updateDialogSize(context);
        initViews();
        initListeners();
        updateAILabStatus();
    }

    private void updateDialogSize(Context context) {
        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.height = WindowManager.LayoutParams.WRAP_CONTENT;
        params.width  = (int)(dm.widthPixels * 0.16f);
        getWindow().setAttributes(params);
        getWindow().setGravity(Gravity.TOP | Gravity.RIGHT);
        getWindow().setBackgroundDrawable(new ColorDrawable(Color.argb(80, 169, 169, 169)));
    }

    protected AiLabDialog(Context context, boolean cancelable, OnCancelListener cancelListener) {
        this(context, 0);
        setCancelable(cancelable);
        setOnCancelListener(cancelListener);
    }

    private void initViews() {
        // CheckBox
        mCheckBoxSvepMode = findViewById(R.id.checkbox_svep_mode);
        mCheckBoxSvepGlobalUI = findViewById(R.id.checkbox_svep_global_ui);
        mCheckBoxSvepContrastMode = findViewById(R.id.checkbox_svep_contrast_mode);
        // SeekBar
        mSeekBarSvepContrastRatio  = findViewById(R.id.seekbar_svep_contrast_ratio);
        mSeekBarSvepEnhanceRate = findViewById(R.id.seekbar_svep_enhance_rate);
        mSeekBarSvepContrastRatio.incrementProgressBy(1);
        mSeekBarSvepEnhanceRate.incrementProgressBy(1);
        // TextView
        mTxtSvepMode = findViewById(R.id.textview_svep_mode);
        mTxtSvepGlobalUi = findViewById(R.id.textview_svep_global_ui);
        mTxtSvepContrastMode = findViewById(R.id.textview_svep_contrast_mode);
        mTxtSvepContrastRatio = findViewById(R.id.textview_svep_contrast_ratio);
        mTxtSvepContrastRatioValue = findViewById(R.id.textview_svep_contrast_ratio_value);
        mTxtSvepEnhanceRate = findViewById(R.id.textview_svep_enhance_rate);
        mTxtSvepEnhanceRateValue = findViewById(R.id.textview_svep_enhance_rate_value);
        // LinearLayout
        mLayoutSvepMode = findViewById(R.id.layout_svep_mode);
        mLayoutSvepGlobalUI = findViewById(R.id.layout_svep_global_ui);
        mLayoutSvepContrastMode = findViewById(R.id.layout_svep_contrast_mode);
        mLayoutSvepContrastRatio = findViewById(R.id.layout_svep_contrast_ratio);
        mLayoutSvepEnhanceRate = findViewById(R.id.layout_svep_enhance_rate);
    }

    private void initListeners() {
        mCheckBoxSvepMode.setOnCheckedChangeListener(this);
        mCheckBoxSvepGlobalUI.setOnCheckedChangeListener(this);
        mCheckBoxSvepContrastMode.setOnCheckedChangeListener(this);

        mSeekBarSvepContrastRatio.setOnSeekBarChangeListener(this);
        mSeekBarSvepEnhanceRate.setOnSeekBarChangeListener(this);

        mCheckBoxSvepMode.setOnFocusChangeListener(this);
        mCheckBoxSvepGlobalUI.setOnFocusChangeListener(this);
        mCheckBoxSvepContrastMode.setOnFocusChangeListener(this);
        mSeekBarSvepContrastRatio.setOnFocusChangeListener(this);
        mSeekBarSvepEnhanceRate.setOnFocusChangeListener(this);
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int value, boolean fromUser) {
        Log.d(TAG, "onProgressChanged() SeekBar = " + seekBar.toString()  + "; value = " + value);
        switch (seekBar.getId()) {
            case R.id.seekbar_svep_contrast_ratio:
                mTxtSvepContrastRatioValue.setText(String.format(Locale.getDefault(), "%d", value));
                int ratio = Math.min(Math.max(value, 10), 90);    // range: 10~90, default: 50
                SystemProperties.set(PROP_AI_SVEP_CONTRAST_RATIO, String.valueOf(ratio));
                break;
            case R.id.seekbar_svep_enhance_rate:
                mTxtSvepEnhanceRateValue.setText(String.format(Locale.getDefault(), "%d", value));
                int enhancement = Math.min(Math.max(value, 0), 10);    // range: 0~10, default: 5
                SystemProperties.set(PROP_AI_SVEP_ENHANCE_RATE, String.valueOf(enhancement));
                break;
            default:
                Log.d(TAG, "onProgressChanged: no SeekBar listener");
                break;
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        Log.e(TAG, "onCheckedChanged isChecked=" + isChecked + " updatedByInner="+mUpdatedInner);
        if (mUpdatedInner) {
            return ;
        }
        switch (buttonView.getId()) {
            case R.id.checkbox_svep_mode:
                setAiImageSuperResolution(mCheckBoxSvepMode.isChecked());
                break;
            case R.id.checkbox_svep_global_ui:
                setGlobalUISvep(mCheckBoxSvepGlobalUI.isChecked());
                break;
            case R.id.checkbox_svep_contrast_mode:
                setAiImageSplitMode(mCheckBoxSvepContrastMode.isChecked());
                break;
            default:
                Log.d(TAG, "onCheckedChanged: no CheckBox listner");
                break;
        }
    }

    @Override
    public void onFocusChange(View view, boolean isFocused) {
        Log.e(TAG, "onCheckedChanged isFocused=" + isFocused);
        mLayoutSvepMode.setBackgroundColor(Color.argb(0, 169, 169, 169));
        mLayoutSvepGlobalUI.setBackgroundColor(Color.argb(0, 169, 169, 169));
        mLayoutSvepContrastMode.setBackgroundColor(Color.argb(0, 169, 169, 169));
        mLayoutSvepContrastRatio.setBackgroundColor(Color.argb(0, 169, 169, 169));
        mLayoutSvepEnhanceRate.setBackgroundColor(Color.argb(0, 169, 169, 169));

        switch (view.getId() ) {
            case R.id.checkbox_svep_mode:
                mLayoutSvepMode.setBackgroundColor(Color.argb(160, 169, 169, 169));
                break;
            case R.id.checkbox_svep_contrast_mode:
                mLayoutSvepContrastMode.setBackgroundColor(Color.argb(160, 119, 132, 149));
                break;
            case R.id.checkbox_svep_global_ui:
                mLayoutSvepGlobalUI.setBackgroundColor(Color.argb(160, 119, 132, 149));
                break;
            case R.id.seekbar_svep_contrast_ratio:
                mLayoutSvepContrastRatio.setBackgroundColor(Color.argb(160, 119, 132, 149));
                break;
            case R.id.seekbar_svep_enhance_rate:
                mLayoutSvepEnhanceRate.setBackgroundColor(Color.argb(160, 119, 132, 149));
                break;
            default:
                Log.d(TAG, "onFocusChange: unkown view.getId()");
                break;
        }
    }

    private void updateSeekBar(SeekBar seekbar, int min, int max, int value) {
        seekbar.setMin(min);
        seekbar.setMax(max);
        seekbar.setProgress(value);
    }

    private void setAiImageSuperResolution(boolean enabled) {
        SystemProperties.set(PROP_AI_SVEP_MODE, String.valueOf(enabled ? 1 : 0));
        updateAILabStatus();
    }

    private void setAiImageSplitMode(boolean enabled) {
        SystemProperties.set(PROP_AI_SVEP_CONTRAST_MODE, String.valueOf(enabled ? 1 : 0));
        updateAILabStatus();
    }

    private void setGlobalUISvep(boolean enabled) {
        SystemProperties.set(PROP_AI_SVEP_MODE, String.valueOf(enabled ? 2 : 1));
        updateAILabStatus();
    }

    private void updateAILabStatus() {
        updateAISvepProps();

        // update CheckBoxes.
        mUpdatedInner = true;
        boolean status = false;
        status = SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) > 0;
        mCheckBoxSvepMode.setChecked(status);
        status = SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) == 2;
        mCheckBoxSvepGlobalUI.setChecked(status);
        status = SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_MODE, 0) == 1;
        mCheckBoxSvepContrastMode.setChecked(status);
        mUpdatedInner = false;

        // update SeekBars.
        int svepValue = 0;
        status = SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_MODE, 0) > 0;
        mSeekBarSvepContrastRatio.setEnabled(status);
        svepValue = SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_RATIO, 50);
        this.updateSeekBar(mSeekBarSvepContrastRatio, 10, 90, svepValue);

        status = SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) > 0;
        mSeekBarSvepEnhanceRate.setEnabled(status);
        svepValue = SystemProperties.getInt(PROP_AI_SVEP_ENHANCE_RATE, 5);
        this.updateSeekBar(mSeekBarSvepEnhanceRate, 0, 10, svepValue);
    }

    private void updateAISvepProps() {
        // update SVEP properties.
        if ((SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) == 0) && 
            (SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_MODE, 0) == 1)) {
            SystemProperties.set(PROP_AI_SVEP_CONTRAST_MODE, String.valueOf(0));
        } 
    }
}
