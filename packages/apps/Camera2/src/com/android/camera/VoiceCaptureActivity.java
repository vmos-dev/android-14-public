/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.camera;

import android.content.Intent;
import android.app.Activity;
import android.os.Bundle;

// Use a different activity for capture intents, so it can have a different
// task affinity from others. This makes sure the regular camera activity is not
// reused for IMAGE_CAPTURE or VIDEO_CAPTURE intents from other activities.
public class VoiceCaptureActivity extends Activity {
    @Override
    protected final void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        Intent intent = new Intent(this, CameraActivity.class);
        intent.setAction(getIntent().getAction());
        intent.putExtras(getIntent());
	if (getIntent().getCategories() != null)
            for (String category : getIntent().getCategories())
                intent.addCategory(category);
        startActivity(intent);
        finish();
    }
}
