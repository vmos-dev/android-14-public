package com.android.tv.settings.display;

import android.os.Bundle;
import android.view.WindowManager;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.android.tv.settings.TvSettingsActivity;
import com.android.tv.settings.overlay.FlavorUtils;

/**
 * @author Weng Tao
 */
public class AIDisplayActivity extends TvSettingsActivity {

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        boolean isDim = getIntent().getBooleanExtra(DeviceFragment.KEY_AI_DISPLAY_DIM, false);
        if (!isDim) {
            WindowManager.LayoutParams params = getWindow().getAttributes();
            params.dimAmount = 0f;
            getWindow().setAttributes(params);
        }
    }

    @Override
    protected Fragment createSettingsFragment() {
        return FlavorUtils.getFeatureFactory(this).getSettingsFragmentProvider()
                .newSettingsFragment(AIDisplayFragment.class.getName(), null);
    }
}
