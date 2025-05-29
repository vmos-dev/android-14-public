/*
 * Copyright (C) 2022 The Android Open Source Project
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

package android.hardware.wifi;

/**
 * NIRA for pairing identity resolution.
 * See Wi-Fi Aware R4.0 section 9.5.21.6
 */
@VintfStability
parcelable NanIdentityResolutionAttribute {
    /**
     * A random byte string to generate tag
     */
    byte[8] nonce;

    /**
     * A resolvable identity to identify Nan identity key
     */
    byte[8] tag;
}
