/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
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

package android.media.audiofx;

import android.annotation.NonNull;
import android.media.audiofx.AudioEffect;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.StringTokenizer;
import java.util.UUID;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.concurrent.Executor;

/**
 * An RockchipEqualizer is used to alter the frequency response of a particular music source or of the main
 * output mMix.
 * <p>An application creates an RockchipEqualizer object to instantiate and control an RockchipEqualizer engine
 * in the audio framework. The application can either simply use predefined presets or have a more
 * precise control of the gain in each frequency band controlled by the equalizer.
 * <p>The methods, parameter types and units exposed by the RockchipEqualizer implementation are directly
 * mapping those defined by the OpenSL ES 1.0.1 Specification (http://www.khronos.org/opensles/)
 * for the SLEqualizerItf interface. Please refer to this specification for more details.
 * <p>To attach the RockchipEqualizer to a particular AudioTrack or MediaPlayer, specify the audio session
 * ID of this AudioTrack or MediaPlayer when constructing the RockchipEqualizer.
 * <p>NOTE: attaching an RockchipEqualizer to the global audio output mMix by use of session 0 is deprecated.
 * <p>See {@link android.media.MediaPlayer#getAudioSessionId()} for details on audio sessions.
 * <p>See {@link android.media.audiofx.AudioEffect} class for more details on controlling audio
 * effects.
 */

public class RockchipEqualizer extends AudioEffect {

    private final static String TAG = "RockchipEqualizer";

    // TODO: These constants should be synchronized with those in
    // frameworks/base/include/media/RockchipEqualizerApi.h,
    // we have not added those constants in the header file since it's a totally
    // customized vendor audio effect.

    /**
     * UUID for Rockchip Audio Effects bundle
     */
    private static final UUID EFFECT_TYPE_ROCKCHIP =
            UUID.fromString("34805d32-2e6d-4d1e-9296-0002a5d5c51b");
    /**
     * UUID for Rockchip Equalizer
     */
    private static final UUID EFFECT_UUID_ROCKCHIP_EQDRC =
            UUID.fromString("79fe72b2-4182-44c1-b2ea-0002a5d5c51b");
    /**
     * Current preset. Parameter ID for OnParameterChangeListener
     */
    public static final int PARAM_CURRENT_PRESET = 0;
    /**
     * Request number of presets. Parameter ID for OnParameterChangeListener
     */
    public static final int PARAM_GET_NUM_OF_PRESETS = 1;
    /**
     * Request preset name. Parameter ID for OnParameterChangeListener
     */
    public static final int PARAM_GET_PRESET_NAME = 2;
    /**
     * Request preset properties. Parameter ID for OnParameterChangeListener
     */
    public static final int PARAM_PROPERTIES = 3;
    /**
     * Maximum size for preset name
     */
    public static final int PARAM_STRING_SIZE_MAX = 32;

    /**
     * Number of presets implemented by RockchipEqualizer engine
     */
    private int mNumPresets;
    /**
     * Names of presets implemented by RockchipEqualizer engine
     */
    private String[] mPresetNames;

    /**
     * Registered listener for parameter changes.
     */
    private OnParameterChangeListener mParamListener = null;

    /**
     * Listener used internally to to receive raw parameter change event from AudioEffect super class
     */
    private BaseParameterListener mBaseParamListener = null;

    /**
     * Lock for access to mParamListener
     */
    private final Object mParamListenerLock = new Object();

    /**
     * Class constructor.
     * @param priority the priority level requested by the application for controlling the RockchipEqualizer
     * engine. As the same engine can be shared by several applications, this parameter indicates
     * how much the requesting application needs control of effect parameters. The normal priority
     * is 0, above normal is a positive number, below normal a negative number.
     * @param audioSession  system wide unique audio session identifier. The RockchipEqualizer will be
     * attached to the MediaPlayer or AudioTrack in the same audio session.
     */
    public RockchipEqualizer(int priority, int audioSession) {
        super(EFFECT_TYPE_ROCKCHIP, EFFECT_UUID_ROCKCHIP_EQDRC, priority, audioSession);

        if (audioSession == 0) {
            Log.w(TAG, "WARNING: attaching an RockchipEqualizer to global output mMix is deprecated!");
        }

        mNumPresets = (int)getNumberOfPresets();

        if (mNumPresets != 0) {
            mPresetNames = new String[mNumPresets];
            byte[] value = new byte[PARAM_STRING_SIZE_MAX];
            int[] param = new int[2];
            param[0] = PARAM_GET_PRESET_NAME;
            for (int i = 0; i < mNumPresets; i++) {
                param[1] = i;
                checkStatus(getParameter(param, value));
                int length = 0;
                while (value[length] != 0) length++;
                try {
                    mPresetNames[i] = new String(value, 0, length, "ISO-8859-1");
                } catch (java.io.UnsupportedEncodingException e) {
                    Log.e(TAG, "preset name decode error");
                }
            }
        }
    }

    /**
     * Gets current preset.
     * @return the preset that is set at the moment.
     */
    public int getCurrentPreset() {
        int[] result = new int[1];
        checkStatus(getParameter(PARAM_CURRENT_PRESET, result));
        return result[0];
    }

    /**
     * Sets the equalizer according to the given preset.
     * @param preset new preset that will be taken into use. The valid range is [0,
     * number of presets-1].
     * @see #getNumberOfPresets()
     */
    public void usePreset(int preset) {
        checkStatus(setParameter(PARAM_CURRENT_PRESET, preset));
    }

    /**
     * Gets the total number of presets the equalizer supports. The presets will have indices
     * [0, number of presets-1].
     * @return the number of presets the equalizer supports.
     */
    public int getNumberOfPresets() {
        int[] result = new int[1];
        checkStatus(getParameter(PARAM_GET_NUM_OF_PRESETS, result));
        return result[0];
    }

    /**
     * Gets the preset name based on the index.
     * @param preset index of the preset. The valid range is [0, number of presets-1].
     * @return a string containing the name of the given preset.
     */
    @NonNull
    public String getPresetName(int preset) {
        if (preset >= 0 && preset < mNumPresets) {
            return mPresetNames[preset];
        } else {
            return "";
        }
    }

    /**
     * The OnParameterChangeListener interface defines a method called by the RockchipEqualizer when a
     * parameter value has changed.
     */
    public interface OnParameterChangeListener  {
        /**
         * Method called when a parameter value has changed. The method is called only if the
         * parameter was changed by another application having the control of the same
         * RockchipEqualizer engine.
         * @param effect the RockchipEqualizer on which the interface is registered.
         * @param status status of the set parameter operation.
         * @param param1 ID of the modified parameter. See {@mLink #PARAM_BAND_LEVEL} ...
         * @param param2 additional parameter qualifier (e.g the band for band level parameter).
         * @param value the new parameter value.
         */
        void onParameterChange(@NonNull RockchipEqualizer effect, int status, int param1, int param2, int value);
    }

    /**
     * Listener used internally to receive unformatted parameter change events from AudioEffect
     * super class.
     */
    private class BaseParameterListener implements AudioEffect.OnParameterChangeListener {
        private BaseParameterListener() {

        }
        public void onParameterChange(AudioEffect effect, int status, byte[] param, byte[] value) {
            OnParameterChangeListener l = null;

            synchronized (mParamListenerLock) {
                if (mParamListener != null) {
                    l = mParamListener;
                }
            }
            if (l != null) {
                int p1 = -1;
                int p2 = -1;
                int v = -1;

                if (param.length >= 4) {
                    p1 = byteArrayToInt(param, 0);
                    if (param.length >= 8) {
                        p2 = byteArrayToInt(param, 4);
                    }
                }
                if (value.length == 2) {
                    v = (int)byteArrayToShort(value, 0);;
                } else if (value.length == 4) {
                    v = byteArrayToInt(value, 0);
                }

                if (p1 != -1 && v != -1) {
                    l.onParameterChange(RockchipEqualizer.this, status, p1, p2, v);
                }
            }
        }
    }

    /**
     * Registers an OnParameterChangeListener interface.
     * @param executor The executor through which the listener should be invoked.
     * @param listener OnParameterChangeListener interface registered.
     */
    public void setParameterListener(@NonNull Executor executor, @NonNull OnParameterChangeListener listener) {
        synchronized (mParamListenerLock) {
            if (mParamListener == null) {
                mParamListener = listener;
                mBaseParamListener = new BaseParameterListener();
                executor.execute(() -> {
                    super.setParameterListener(mBaseParamListener);
                });
            }
        }
    }

    /**
     * The Settings class regroups all equalizer parameters. It is used in
     * conjuntion with getProperties() and setProperties() methods to backup and restore
     * all parameters in a single call.
     */
    public static class Settings {

        public static class Speaker {
            private float mPreGain;
            @NonNull private float[] mReserved;

            public Speaker() {
                mReserved = new float[4];
            }

            public Speaker(@NonNull ByteBuffer converter) {
                mReserved = new float[4];
                mPreGain = converter.getFloat();
                for (int i = 0; i < mReserved.length; i++) {
                    mReserved[i] = converter.getFloat();
                }
            }

            public float getPreGain() {
                return mPreGain;
            }

            public void setPreGain(float preGain) {
                mPreGain = preGain;
            }

            public String toString() {
                String str = new String("Speaker: { " +
                                        "mPreGain: " + Float.toString(mPreGain) +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(20);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mPreGain);
                for (float val : mReserved) {
                    converter.putFloat(val);
                }
                return converter.array();
            }
        };

        public static class AutoGain {
            private float mTargetIDb;
            private float mAttackTime;
            private float mReleaseTime;
            private float mR128Wins;
            private float mR128Steps;
            @NonNull private float[] mReserved;

            public AutoGain() {
                mReserved = new float[5];
            }

            public AutoGain(@NonNull ByteBuffer converter) {
                mReserved = new float[5];
                mTargetIDb = converter.getFloat();
                mAttackTime = converter.getFloat();
                mReleaseTime = converter.getFloat();
                mR128Wins = converter.getFloat();
                mR128Steps = converter.getFloat();
                for (int i = 0; i < mReserved.length; i++) {
                    mReserved[i] = converter.getFloat();
                }
            }

            public float getTargetIDb() {
                return mTargetIDb;
            }

            public void setTargetIDb(float targetIDb) {
                mTargetIDb = targetIDb;
            }

            public float getAttackTime() {
                return mAttackTime;
            }

            public void setAttackTime(float attackTime) {
                mAttackTime = attackTime;
            }

            public float getReleaseTime() {
                return mReleaseTime;
            }

            public void setReleaseTime(float releaseTime) {
                mReleaseTime = releaseTime;
            }

            public float getR128Wins() {
                return mR128Wins;
            }

            public void setR128Wins(float r128Wins) {
                mR128Wins = r128Wins;
            }

            public float getR128Steps() {
                return mR128Steps;
            }

            public void setR128Steps(float r128Steps) {
                mR128Steps = r128Steps;
            }

            public String toString() {
                String str = new String("AutoGain: { " +
                                        "mTargetIDb: " + Float.toString(mTargetIDb) +
                                        ", mAttackTime: " + Float.toString(mAttackTime) +
                                        ", mReleaseTime: " + Float.toString(mReleaseTime) +
                                        ", mR128Wins: " + Float.toString(mR128Wins) +
                                        ", mR128Steps: " + Float.toString(mR128Steps) +
                                        "}");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(40);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mTargetIDb);
                converter.putFloat(mAttackTime);
                converter.putFloat(mReleaseTime);
                converter.putFloat(mR128Wins);
                converter.putFloat(mR128Steps);
                for (float val : mReserved) {
                    converter.putFloat(val);
                }
                return converter.array();
            }
        };

        public static class BassEnhancer {
            private float mAmountDb;
            private float mDrive;
            private float mBlend;
            private float mFreq;
            private float mFloorActive;
            private float mFloorFreq;
            @NonNull private float[] mReserved;

            public BassEnhancer() {
                mReserved = new float[4];
            }

            public BassEnhancer(@NonNull ByteBuffer converter) {
                mReserved = new float[4];
                mAmountDb = converter.getFloat();
                mDrive = converter.getFloat();
                mBlend = converter.getFloat();
                mFreq = converter.getFloat();
                mFloorActive = converter.getFloat();
                mFloorFreq = converter.getFloat();
                for (int i = 0; i < mReserved.length; i++) {
                    mReserved[i] = converter.getFloat();
                }
            }

            public float getAmountDb() {
                return mAmountDb;
            }

            public void setAmountDb(float amountDb) {
                mAmountDb = amountDb;
            }

            public float getDrive() {
                return mDrive;
            }

            public void setDrive(float drive) {
                mDrive = drive;
            }

            public float getBlend() {
                return mBlend;
            }

            public void setBlend(float blend) {
                mBlend = blend;
            }

            public float getFreq() {
                return mFreq;
            }

            public void setFreq(float freq) {
                mFreq = freq;
            }

            public float getFloorActive() {
                return mFloorActive;
            }

            public void setFloorActive(float floorActive) {
                mFloorActive = floorActive;
            }

            public float getFloorFreq() {
                return mFloorFreq;
            }

            public void setFloorFreq(float floorFreq) {
                mFloorFreq = floorFreq;
            }

            public String toString() {
                String str = new String("BassEnhancer: { " +
                                        "mAmountDb: " + Float.toString(mAmountDb) +
                                        ", mDrive: " + Float.toString(mDrive) +
                                        ", mBlend: " + Float.toString(mBlend) +
                                        ", mFreq: " + Float.toString(mFreq) +
                                        ", mFloorActive: " + Float.toString(mFloorActive) +
                                        ", mFloorFreq: " + Float.toString(mFloorFreq) +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(40);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mAmountDb);
                converter.putFloat(mDrive);
                converter.putFloat(mBlend);
                converter.putFloat(mFreq);
                converter.putFloat(mFloorActive);
                converter.putFloat(mFloorFreq);
                for (float val : mReserved) {
                    converter.putFloat(val);
                }
                return converter.array();
            }
        };

        public static class Exciter {
            private float mEventHarmonics;
            private float mDistortionAmount;
            private float mCutoff;
            private float mMix;
            private float mReserved;

            public Exciter() {

            }

            public Exciter(@NonNull ByteBuffer converter) {
                mEventHarmonics = converter.getFloat();
                mDistortionAmount = converter.getFloat();
                mCutoff = converter.getFloat();
                mMix = converter.getFloat();
                mReserved = converter.getFloat();
            }

            public float getEventHarmonics() {
                return mEventHarmonics;
            }

            public void setEventHarmonics(float eventHarmonics) {
                mEventHarmonics = eventHarmonics;
            }

            public float getDistortionAmount() {
                return mDistortionAmount;
            }

            public void setDistortionAmount(float distortionAmount) {
                mDistortionAmount = distortionAmount;
            }

            public float getCutoff() {
                return mCutoff;
            }

            public void setCutoff(float cutoff) {
                mCutoff = cutoff;
            }

            public float getMix() {
                return mMix;
            }

            public void setMix(float mix) {
                mMix = mix;
            }

            public String toString() {
                String str = new String("Exciter: { " +
                                        "mEventHarmonics: " + Float.toString(mEventHarmonics) +
                                        ", mDistortionAmount: " + Float.toString(mDistortionAmount) +
                                        ", mCutoff: " + Float.toString(mCutoff) +
                                        ", mMix: " + Float.toString(mMix) +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(20);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mEventHarmonics);
                converter.putFloat(mDistortionAmount);
                converter.putFloat(mCutoff);
                converter.putFloat(mMix);
                converter.putFloat(mReserved);
                return converter.array();
            }
        };

        public static class Deesser {
            private float mF0;
            private float mThreshold;
            @NonNull private float[] mReserved;

            public Deesser() {
                mReserved = new float[3];
            }

            public Deesser(@NonNull ByteBuffer converter) {
                mReserved = new float[3];
                mF0 = converter.getFloat();
                mThreshold = converter.getFloat();
                for (int i = 0; i < mReserved.length; i++) {
                    mReserved[i] = converter.getFloat();
                }
            }

            public float getF0() {
                return mF0;
            }

            public void setF0(float f0) {
                mF0 = f0;
            }

            public float getThreshold() {
                return mThreshold;
            }

            public void setThreshold(float threshold) {
                mThreshold = threshold;
            }

            public String toString() {
                String str = new String("Deesser: { " +
                                        "mF0: " + Float.toString(mF0) +
                                        ", mThreshold: " + Float.toString(mThreshold) +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(20);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mF0);
                converter.putFloat(mThreshold);
                for (float val : mReserved) {
                    converter.putFloat(val);
                }
                return converter.array();
            }
        };

        public static class Eq {

            public static class EqBand {
                private float mEnabled;
                private float mFilter;
                private float mFc;
                private float mQ;
                private float mBoost;

                public EqBand() {

                }

                public EqBand(@NonNull ByteBuffer converter) {
                    mEnabled = converter.getFloat();
                    mFilter = converter.getFloat();
                    mFc = converter.getFloat();
                    mQ = converter.getFloat();
                    mBoost = converter.getFloat();
                }

                public float getEnabled() {
                    return mEnabled;
                }

                public void setEnabled(float enabled) {
                    mEnabled = enabled;
                }

                public float getFilter() {
                    return mFilter;
                }

                public void setFilter(float filter) {
                    mFilter = filter;
                }

                public float getFc() {
                    return mFc;
                }

                public void setFc(float fc) {
                    mFc = fc;
                }

                public float getQ() {
                    return mQ;
                }

                public void setQ(float q) {
                    mQ = q;
                }

                public float getBoost() {
                    return mBoost;
                }

                public void setBoost(float boost) {
                    mBoost = boost;
                }

                public String toString() {
                    String str = new String("{ " +
                                            "mEnabled: " + Float.toString(mEnabled) +
                                            ", mFilter: " + Float.toString(mFilter) +
                                            ", mFc: " + Float.toString(mFc) +
                                            ", mQ: " + Float.toString(mQ) +
                                            ", mBoost: " + Float.toString(mBoost) +
                                            " }");
                    return str;
                }

                @NonNull
                public byte[] toBytes() {
                    ByteBuffer converter = ByteBuffer.allocate(20);
                    converter.order(ByteOrder.nativeOrder());
                    converter.putFloat(mEnabled);
                    converter.putFloat(mFilter);
                    converter.putFloat(mFc);
                    converter.putFloat(mQ);
                    converter.putFloat(mBoost);
                    return converter.array();
                }
            };

            @NonNull private EqBand[] mBands;

            public Eq() {
                mBands = new EqBand[NUM_BANDS];
                for (int i = 0; i < NUM_BANDS; i++) {
                    mBands[i] = new EqBand();
                }
            }

            public Eq(@NonNull ByteBuffer converter) {
                mBands = new EqBand[NUM_BANDS];
                for (int i = 0; i < NUM_BANDS; i++) {
                    mBands[i] = new EqBand(converter);
                }
            }

            @NonNull
            public Collection<EqBand> getBands() {
                return Arrays.asList(mBands);
            }

            public void setBands(@NonNull Collection<EqBand> bands) {
                mBands = bands.toArray(new EqBand[bands.size()]);
            }

            public String toString() {
                String str = new String("Eq: { ");
                for (int i = 0; i < NUM_BANDS; i++) {
                    str = str.concat("band" + (i + 1) + ": " +
                                     mBands[i].toString() +
                                     ", ");
                }
                str = str.concat(" }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(200);
                converter.order(ByteOrder.nativeOrder());
                for (int i = 0; i < NUM_BANDS; i++) {
                    converter.put(mBands[i].toBytes());
                }
                return converter.array();
            }
        };

        public static class Mbdrc {

            public static class MbdrcFreq {
                private float mFreqStart;
                private float mFreqEnd;
                private float mGainDb;
                private float mDrcEnabled;
                private float mCompressStart;
                private float mExpandEnd;
                private float mNoiseThreshold;
                private float mMaxGain;
                private float mMaxPeek;
                private float mAttackTime;
                private float mReleaseTime;
                private float mHoldTime;
                @NonNull private float mReserved[];

                public MbdrcFreq() {
                    mReserved = new float[8];
                }

                public MbdrcFreq(@NonNull ByteBuffer converter) {
                    mReserved = new float[8];
                    mFreqStart = converter.getFloat();
                    mFreqEnd = converter.getFloat();
                    mGainDb = converter.getFloat();
                    mDrcEnabled = converter.getFloat();
                    mCompressStart = converter.getFloat();
                    mExpandEnd = converter.getFloat();
                    mNoiseThreshold = converter.getFloat();
                    mMaxGain = converter.getFloat();
                    mMaxPeek = converter.getFloat();
                    mAttackTime = converter.getFloat();
                    mReleaseTime = converter.getFloat();
                    mHoldTime = converter.getFloat();
                    for (int i = 0; i < mReserved.length; i++) {
                        mReserved[i] = converter.getFloat();
                    }
                }

                public float getFreqStart() {
                    return mFreqStart;
                }

                public void setFreqStart(float freqStart) {
                    mFreqStart = freqStart;
                }

                public float getFreqEnd() {
                    return mFreqEnd;
                }

                public void setFreqEnd(float freqEnd) {
                    mFreqEnd = freqEnd;
                }

                public float getGainDb() {
                    return mGainDb;
                }

                public void setGainDb(float gainDb) {
                    mGainDb = gainDb;
                }

                public float getDrcEnabled() {
                    return mDrcEnabled;
                }

                public void setDrcEnabled(float drcEnabled) {
                    mDrcEnabled = drcEnabled;
                }

                public float getCompressStart() {
                    return mCompressStart;
                }

                public void setCompressStart(float compressStart) {
                    mCompressStart = compressStart;
                }

                public float getExpandEnd() {
                    return mExpandEnd;
                }

                public void setExpandEnd(float expandEnd) {
                    mExpandEnd = expandEnd;
                }

                public float getNoiseThreshold() {
                    return mNoiseThreshold;
                }

                public void setNoiseThreshold(float noiseThreshold) {
                    mNoiseThreshold = noiseThreshold;
                }

                public float getMaxGain() {
                    return mMaxGain;
                }

                public void setMaxGain(float maxGain) {
                    mMaxGain = maxGain;
                }

                public float getMaxPeek() {
                    return mMaxPeek;
                }

                public void setMaxPeek(float maxPeek) {
                    mMaxPeek = maxPeek;
                }

                public float getAttackTime() {
                    return mAttackTime;
                }

                public void setAttackTime(float attackTime) {
                    mAttackTime = attackTime;
                }

                public float getReleaseTime() {
                    return mReleaseTime;
                }

                public void setReleaseTime(float releaseTime) {
                    mReleaseTime = releaseTime;
                }

                public float getHoldTime() {
                    return mHoldTime;
                }

                public void setHoldTime(float holdTime) {
                    mHoldTime = holdTime;
                }

                public String toString() {
                    String str = new String("{ " +
                                            "mFreqStart: " + Float.toString(mFreqStart) +
                                            ", mFreqEnd: " + Float.toString(mFreqEnd) +
                                            ", mGainDb: " + Float.toString(mGainDb) +
                                            ", mDrcEnabled: " + Float.toString(mDrcEnabled) +
                                            ", mCompressStart: " + Float.toString(mCompressStart) +
                                            ", mExpandEnd: " + Float.toString(mExpandEnd) +
                                            ", mNoiseThreshold: " + Float.toString(mNoiseThreshold) +
                                            ", mMaxGain: " + Float.toString(mMaxGain) +
                                            ", mMaxPeek: " + Float.toString(mMaxPeek) +
                                            ", mAttackTime: " + Float.toString(mAttackTime) +
                                            ", mReleaseTime: " + Float.toString(mReleaseTime) +
                                            ", mHoldTime: " + Float.toString(mHoldTime) +
                                            " }");
                    return str;
                }

                @NonNull
                public byte[] toBytes() {
                    ByteBuffer converter = ByteBuffer.allocate(80);
                    converter.order(ByteOrder.nativeOrder());
                    converter.putFloat(mFreqStart);
                    converter.putFloat(mFreqEnd);
                    converter.putFloat(mGainDb);
                    converter.putFloat(mDrcEnabled);
                    converter.putFloat(mCompressStart);
                    converter.putFloat(mExpandEnd);
                    converter.putFloat(mNoiseThreshold);
                    converter.putFloat(mMaxGain);
                    converter.putFloat(mMaxPeek);
                    converter.putFloat(mAttackTime);
                    converter.putFloat(mReleaseTime);
                    converter.putFloat(mHoldTime);
                    for (float val : mReserved) {
                        converter.putFloat(val);
                    }
                    return converter.array();
                }
            };

            private float mCrossBand;
            @NonNull private float[] mReserved1;
            @NonNull private MbdrcFreq mLowFreq;
            @NonNull private MbdrcFreq mMedFreq1;
            @NonNull private MbdrcFreq mMedFreq2;
            @NonNull private MbdrcFreq mHighFreq;
            @NonNull private float[] mReserved2;

            public Mbdrc() {
                mReserved1 = new float[9];
                mReserved2 = new float[10];
                mLowFreq = new MbdrcFreq();
                mMedFreq1 = new MbdrcFreq();
                mMedFreq2 = new MbdrcFreq();
                mHighFreq = new MbdrcFreq();
            }

            public Mbdrc(@NonNull ByteBuffer converter) {
                mReserved1 = new float[9];
                mReserved2 = new float[10];
                mCrossBand = converter.getFloat();
                for (int i = 0; i < mReserved1.length; i++) {
                    mReserved1[i] = converter.getFloat();
                }
                mLowFreq = new MbdrcFreq(converter);
                mMedFreq1 = new MbdrcFreq(converter);
                mMedFreq2 = new MbdrcFreq(converter);
                mHighFreq = new MbdrcFreq(converter);
                for (int i = 0; i < mReserved2.length; i++) {
                    mReserved2[i] = converter.getFloat();
                }
            }

            public float getCrossBand() {
                return mCrossBand;
            }

            public void setCrossBand(@NonNull float crossBand) {
                mCrossBand = crossBand;
            }

            @NonNull
            public MbdrcFreq getLowFreq() {
                return mLowFreq;
            }

            public void setLowFreq(@NonNull MbdrcFreq lowFreq) {
                mLowFreq = lowFreq;
            }

            @NonNull
            public MbdrcFreq getMedFreq1() {
                return mMedFreq1;
            }

            public void setMedFreq1(@NonNull MbdrcFreq medFreq1) {
                mMedFreq1 = medFreq1;
            }

            @NonNull
            public MbdrcFreq getMedFreq2() {
                return mMedFreq2;
            }

            public void setMedFreq2(@NonNull MbdrcFreq medFreq2) {
                mMedFreq2 = medFreq2;
            }

            @NonNull
            public MbdrcFreq getHighFreq() {
                return mHighFreq;
            }

            public void setHighFreq(@NonNull MbdrcFreq highFreq) {
                mHighFreq = highFreq;
            }

            public String toString() {
                String str = new String("MBDRC: { " +
                                        "mCrossBand: " + Float.toString(mCrossBand) +
                                        ", mLowFreq: " + mLowFreq.toString() +
                                        ", mMedFreq1: " + mMedFreq1.toString() +
                                        ", mMedFreq2: " + mMedFreq2.toString() +
                                        ", mHighFreq: " + mHighFreq.toString() +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(400);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mCrossBand);
                for (float val : mReserved1) {
                    converter.putFloat(val);
                }
                converter.put(mLowFreq.toBytes());
                converter.put(mMedFreq1.toBytes());
                converter.put(mMedFreq2.toBytes());
                converter.put(mHighFreq.toBytes());
                for (float val : mReserved2) {
                    converter.putFloat(val);
                }
                return converter.array();
            }
        };

        public static class Maximizer {
            private float mMaxThreshold;
            private float mCeiling;
            private float mRelease;

            public Maximizer() {

            }

            public Maximizer(@NonNull ByteBuffer converter) {
                mMaxThreshold = converter.getFloat();
                mCeiling = converter.getFloat();
                mRelease = converter.getFloat();
            }

            public float getMaxThreshold() {
                return mMaxThreshold;
            }

            public void setMaxThreshold(float maxThreshold) {
                mMaxThreshold = maxThreshold;
            };

            public float getCeiling() {
                return mCeiling;
            }

            public void setCeiling(float ceiling) {
                mCeiling = ceiling;
            };

            public float getRelease() {
                return mRelease;
            }

            public void setRelease(float release) {
                mRelease = release;
            };

            public String toString() {
                String str = new String("MAXIMIZER: { " +
                                        "mMaxThreshold: " + Float.toString(mMaxThreshold) +
                                        ", mCeiling: " + Float.toString(mCeiling) +
                                        ", mRelease: " + Float.toString(mRelease) +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(12);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mMaxThreshold);
                converter.putFloat(mCeiling);
                converter.putFloat(mRelease);
                return converter.array();
            }
        };

        public static class Agc {
            private float mCompressStart;
            private float mExpandEnd;
            private float mNoiseThreshold;
            private float mMaxGain;
            private float mMaxPeek;
            private float mAttackTime;
            private float mReleaseTime;
            private float mHoldTime;
            @NonNull private float[] mReserved;

            public Agc() {
                mReserved = new float[9];
            }

            public Agc(@NonNull ByteBuffer converter) {
                mReserved = new float[9];
                mCompressStart = converter.getFloat();
                mExpandEnd = converter.getFloat();
                mNoiseThreshold = converter.getFloat();
                mMaxGain = converter.getFloat();
                mMaxPeek = converter.getFloat();
                mAttackTime = converter.getFloat();
                mReleaseTime = converter.getFloat();
                mHoldTime = converter.getFloat();
                for (int i = 0; i < mReserved.length; i++) {
                    mReserved[i] = converter.getFloat();
                }
            }

            public float getCompressStart(float compressStart) {
                return mCompressStart;
            }

            public void setCompressStart(float compressStart) {
                mCompressStart = compressStart;
            }

            public float getExpandEnd(float expandEnd) {
                return mExpandEnd;
            }

            public void setExpandEnd(float expandEnd) {
                mExpandEnd = expandEnd;
            }

            public float getNoiseThreshold(float noiseThreshold) {
                return mNoiseThreshold;
            }

            public void setNoiseThreshold(float noiseThreshold) {
                mNoiseThreshold = noiseThreshold;
            }

            public float getMaxGain(float maxGain) {
                return mMaxGain;
            }

            public void setMaxGain(float maxGain) {
                mMaxGain = maxGain;
            }

            public float getMaxPeek(float maxPeek) {
                return mMaxPeek;
            }

            public void setMaxPeek(float maxPeek) {
                mMaxPeek = maxPeek;
            }

            public float getAttackTime(float attackTime) {
                return mAttackTime;
            }

            public void setAttackTime(float attackTime) {
                mAttackTime = attackTime;
            }

            public float getReleaseTime(float releaseTime) {
                return mReleaseTime;
            }

            public void setReleaseTime(float releaseTime) {
                mReleaseTime = releaseTime;
            }

            public float getHoldTime(float holdTime) {
                return mHoldTime;
            }

            public void setHoldTime(float holdTime) {
                mHoldTime = holdTime;
            }

            public String toString() {
                String str = new String("AGC: { " +
                                        "mCompressStart: " + Float.toString(mCompressStart) +
                                        ", mExpandEnd: " + Float.toString(mExpandEnd) +
                                        ", mNoiseThreshold: " + Float.toString(mNoiseThreshold) +
                                        ", mMaxGain: " + Float.toString(mMaxGain) +
                                        ", mMaxPeek: " + Float.toString(mMaxPeek) +
                                        ", mAttackTime: " + Float.toString(mAttackTime) +
                                        ", mReleaseTime: " + Float.toString(mReleaseTime) +
                                        ", mHoldTime: " + Float.toString(mHoldTime) +
                                        " }");
                return str;
            }

            @NonNull
            public byte[] toBytes() {
                ByteBuffer converter = ByteBuffer.allocate(68);
                converter.order(ByteOrder.nativeOrder());
                converter.putFloat(mCompressStart);
                converter.putFloat(mExpandEnd);
                converter.putFloat(mNoiseThreshold);
                converter.putFloat(mMaxGain);
                converter.putFloat(mMaxPeek);
                converter.putFloat(mAttackTime);
                converter.putFloat(mReleaseTime);
                converter.putFloat(mHoldTime);
                for (float val : mReserved) {
                    converter.putFloat(val);
                }
                return converter.array();
            }
        };

        public static final int NUM_CHANNELS = 2;

        public static final int NUM_BANDS = 10;


        private int mCurPreset;

        private float mSamplingRate;
        private float mBitRate;
        private float mLink;
        private float mChannels;
        @NonNull private float[] mReserved1;
        private float mAutoGainEnabled;
        private float mBassEnabled;
        private float mExciterEnabled;
        private float mDeesserEnabled;
        private float mEq10Enabled;
        private float mMbdrcEnabled;
        private float mAgcEnabled;
        private float mMaximizerEnalbed;
        @NonNull private float[] mReserved2;
        @NonNull private Speaker[] mSpeakers;
        @NonNull private AutoGain[] mAutoGains;
        @NonNull private float[] mReserved3;
        @NonNull private BassEnhancer[] mBassEnhancers;
        @NonNull private Exciter[] mExciters;
        @NonNull private Deesser[] mDeessers;
        @NonNull private Eq[] mEqs;
        @NonNull private Mbdrc[] mMbdrcs;
        @NonNull private Maximizer[] mMaximizers;
        @NonNull private Agc[] mAgcs;
        @NonNull private float[] mReserved4;

        public Settings() {
            mReserved1 = new float[6];
            mReserved2 = new float[2];
            mReserved3 = new float[10];
            mReserved4 = new float[104];
            mSpeakers = new Speaker[NUM_CHANNELS];
            mAutoGains = new AutoGain[NUM_CHANNELS];
            mBassEnhancers = new BassEnhancer[NUM_CHANNELS];
            mExciters = new Exciter[NUM_CHANNELS];
            mDeessers = new Deesser[NUM_CHANNELS];
            mEqs = new Eq[NUM_CHANNELS];
            mMbdrcs = new Mbdrc[NUM_CHANNELS];
            mMaximizers = new Maximizer[NUM_CHANNELS];
            mAgcs = new Agc[NUM_CHANNELS];
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mSpeakers[i] = new Speaker();
                mAutoGains[i] = new AutoGain();
                mBassEnhancers[i] = new BassEnhancer();
                mExciters[i] = new Exciter();
                mDeessers[i] = new Deesser();
                mEqs[i] = new Eq();
                mMbdrcs[i] = new Mbdrc();
                mMaximizers[i] = new Maximizer();
                mAgcs[i] = new Agc();
            }
        }

        public Settings(@NonNull byte[] array) {
            ByteBuffer converter = ByteBuffer.wrap(array);
            converter.order(ByteOrder.nativeOrder());

            mReserved1 = new float[6];
            mReserved2 = new float[2];
            mReserved3 = new float[10];
            mReserved4 = new float[104];
            mSpeakers = new Speaker[NUM_CHANNELS];
            mAutoGains = new AutoGain[NUM_CHANNELS];
            mBassEnhancers = new BassEnhancer[NUM_CHANNELS];
            mExciters = new Exciter[NUM_CHANNELS];
            mDeessers = new Deesser[NUM_CHANNELS];
            mEqs = new Eq[NUM_CHANNELS];
            mMbdrcs = new Mbdrc[NUM_CHANNELS];
            mMaximizers = new Maximizer[NUM_CHANNELS];
            mAgcs = new Agc[NUM_CHANNELS];

            mCurPreset = converter.getInt();
            mSamplingRate = converter.getFloat();
            mBitRate = converter.getFloat();
            mLink = converter.getFloat();
            mChannels = converter.getFloat();
            for (int i = 0; i < mReserved1.length; i++) {
                mReserved1[i] = converter.getFloat();
            }
            mAutoGainEnabled = converter.getFloat();
            mBassEnabled = converter.getFloat();
            mExciterEnabled = converter.getFloat();
            mDeesserEnabled = converter.getFloat();
            mEq10Enabled = converter.getFloat();
            mMbdrcEnabled = converter.getFloat();
            mAgcEnabled = converter.getFloat();
            mMaximizerEnalbed = converter.getFloat();
            for (int i = 0; i < mReserved2.length; i++) {
                mReserved2[i] = converter.getFloat();
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mSpeakers[i] = new Speaker(converter);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mAutoGains[i] = new AutoGain(converter);
            }
            for (int i = 0; i < mReserved3.length; i++) {
                mReserved3[i] = converter.getFloat();
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mBassEnhancers[i] = new BassEnhancer(converter);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mExciters[i] = new Exciter(converter);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mDeessers[i] = new Deesser(converter);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mEqs[i] = new Eq(converter);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mMbdrcs[i] = new Mbdrc(converter);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                mMaximizers[i] = new Maximizer(converter);
                mAgcs[i] = new Agc(converter);
            }
            for (int i = 0; i < mReserved4.length; i++) {
                mReserved4[i] = converter.getFloat();
            }
        }

        public int getCurPreset() {
            return mCurPreset;
        }

        public void setCurPreset(int curPreset) {
            mCurPreset = curPreset;
        }

        public float getSamplingRate() {
            return mSamplingRate;
        }

        public void setSamplingRate(float samplingRate) {
            mSamplingRate = samplingRate;
        }

        public float getBitRate() {
            return mBitRate;
        }

        public void setBitRate(float bitRate) {
            mBitRate = bitRate;
        }

        public float getLink() {
            return mLink;
        }

        public void setLink(float link) {
            mLink = link;
        }

        public float getChannels() {
            return mChannels;
        }

        public void setChannels(float channels) {
            mChannels = channels;
        }

        public float getAutoGainEnabled() {
            return mAutoGainEnabled;
        }

        public void setAutoGainEnabled(float autoGainEnabled) {
            mAutoGainEnabled = autoGainEnabled;
        }

        public float getBassEnabled() {
            return mBassEnabled;
        }

        public void setBassEnabled(float bassEnabled) {
            mBassEnabled = bassEnabled;
        }

        public float getExciterEnabled() {
            return mExciterEnabled;
        }

        public void setExciterEnabled(float exciterEnabled) {
            mExciterEnabled = exciterEnabled;
        }

        public float getDeesserEnabled() {
            return mDeesserEnabled;
        }

        public void setDeesserEnabled(float deesserEnabled) {
            mDeesserEnabled = deesserEnabled;
        }

        public float getEq10Enabled() {
            return mEq10Enabled;
        }

        public void setEq10Enabled(float eq10Enabled) {
            mEq10Enabled = eq10Enabled;
        }

        public float getMbdrcEnabled() {
            return mMbdrcEnabled;
        }

        public void setMbdrcEnabled(float mbdrcEnabled) {
            mMbdrcEnabled = mbdrcEnabled;
        }

        public float getAgcEnabled() {
            return mAgcEnabled;
        }

        public void setAgcEnabled(float agcEnabled) {
            mAgcEnabled = agcEnabled;
        }

        public float getMaximizerEnalbed() {
            return mMaximizerEnalbed;
        }

        public void setMaximizerEnalbed(float maximizerEnalbed) {
            mMaximizerEnalbed = maximizerEnalbed;
        }

        @NonNull
        public Collection<Speaker> getSpeakers() {
            return Arrays.asList(mSpeakers);
        }

        public void setSpeakers(@NonNull Collection<Speaker> speakers) {
            mSpeakers = speakers.toArray(new Speaker[speakers.size()]);
        }

        @NonNull
        public Collection<AutoGain> getAutoGains() {
            return Arrays.asList(mAutoGains);
        }

        public void setAutoGains(@NonNull Collection<AutoGain> autoGains) {
            mAutoGains = autoGains.toArray(new AutoGain[autoGains.size()]);
        }

        @NonNull
        public Collection<BassEnhancer> getBassEnhancers() {
            return Arrays.asList(mBassEnhancers);
        }

        public void setBassEnhancers(@NonNull Collection<BassEnhancer> bassEnhancers) {
            mBassEnhancers = bassEnhancers.toArray(new BassEnhancer[bassEnhancers.size()]);
        }

        @NonNull
        public Collection<Exciter> getExciters() {
            return Arrays.asList(mExciters);
        }

        public void setExciters(@NonNull Collection<Exciter> exciters) {
            mExciters = exciters.toArray(new Exciter[exciters.size()]);
        }

        @NonNull
        public Collection<Deesser> getDeessers() {
            return Arrays.asList(mDeessers);
        }

        public void setDeessers(@NonNull Collection<Deesser> deessers) {
            mDeessers = deessers.toArray(new Deesser[deessers.size()]);
        }

        @NonNull
        public Collection<Eq> getEqs() {
            return Arrays.asList(mEqs);
        }

        public void setEqs(@NonNull Collection<Eq> eqs) {
            mEqs = eqs.toArray(new Eq[eqs.size()]);
        }

        @NonNull
        public Collection<Mbdrc> getMbdrcs() {
            return Arrays.asList(mMbdrcs);
        }

        public void setMbdrcs(@NonNull Collection<Mbdrc> mbdrcs) {
            mMbdrcs = mbdrcs.toArray(new Mbdrc[mbdrcs.size()]);
        }

        @NonNull
        public Collection<Maximizer> getMaximizers() {
            return Arrays.asList(mMaximizers);
        }

        public void setMaximizers(@NonNull Collection<Maximizer> maximizers) {
            mMaximizers = maximizers.toArray(new Maximizer[maximizers.size()]);
        }

        @NonNull
        public Collection<Agc> getAgcs() {
            return Arrays.asList(mAgcs);
        }

        public void setAgcs(@NonNull Collection<Agc> agcs) {
            mAgcs = agcs.toArray(new Agc[agcs.size()]);
        }

        @Override
        public String toString() {
            String str = new String (
                    "{ mCurPreset: " + Integer.toString(mCurPreset) +
                    ", mSamplingRate: " + Float.toString(mSamplingRate) +
                    ", mBitRate: " + Float.toString(mBitRate) +
                    ", mLink: " + Float.toString(mLink) +
                    ", mChannels: " + Float.toString(mChannels) +
                    ", mAutoGainEnabled: " + Float.toString(mAutoGainEnabled) +
                    ", mBassEnabled: " + Float.toString(mBassEnabled) +
                    ", mExciterEnabled: " + Float.toString(mExciterEnabled) +
                    ", mDeesserEnabled: " + Float.toString(mDeesserEnabled) +
                    ", mEq10Enabled: " + Float.toString(mEq10Enabled) +
                    ", mMbdrcEnabled: " + Float.toString(mMbdrcEnabled) +
                    ", mAgcEnabled: " + Float.toString(mAgcEnabled) +
                    ", mMaximizerEnalbed: " + Float.toString(mMaximizerEnalbed) +
                    ", ");

            for (int i = 0; i < NUM_CHANNELS; i++) {
                str = str.concat("channel" + (i + 1) + " : {");
                str = str.concat(mSpeakers[i].toString() + ", ");
                str = str.concat(mAutoGains[i].toString() + ", ");
                str = str.concat(mBassEnhancers[i].toString() + ", ");
                str = str.concat(mExciters[i].toString() + ", ");
                str = str.concat(mDeessers[i].toString() + ", ");
                str = str.concat(mEqs[i].toString() + ", ");
                str = str.concat(mMbdrcs[i].toString() + ", ");
                str = str.concat(mMaximizers[i].toString() + ", ");
                str = str.concat(mAgcs[i].toString() + ", ");
                str = str.concat(" }, ");
            }

            str = str.concat(" }");
            return str;
        }

        @NonNull
        public byte[] toBytes() {
            // 4 bytes of current preset and 2176 bytes of profile.
            ByteBuffer converter = ByteBuffer.allocate(2180);
            converter.order(ByteOrder.nativeOrder());
            converter.putInt(mCurPreset);
            converter.putFloat(mSamplingRate);
            converter.putFloat(mBitRate);
            converter.putFloat(mLink);
            converter.putFloat(mChannels);
            for (float val : mReserved1) {
                converter.putFloat(val);
            }
            converter.putFloat(mAutoGainEnabled);
            converter.putFloat(mBassEnabled);
            converter.putFloat(mExciterEnabled);
            converter.putFloat(mDeesserEnabled);
            converter.putFloat(mEq10Enabled);
            converter.putFloat(mMbdrcEnabled);
            converter.putFloat(mAgcEnabled);
            converter.putFloat(mMaximizerEnalbed);
            for (float val : mReserved2) {
                converter.putFloat(val);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mSpeakers[i].toBytes());
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mAutoGains[i].toBytes());
            }
            for (float val : mReserved3) {
                converter.putFloat(val);
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mBassEnhancers[i].toBytes());
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mExciters[i].toBytes());
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mDeessers[i].toBytes());
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mEqs[i].toBytes());
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mMbdrcs[i].toBytes());
            }
            for (int i = 0; i < NUM_CHANNELS; i++) {
                converter.put(mMaximizers[i].toBytes());
                converter.put(mAgcs[i].toBytes());
            }
            for (float val : mReserved4) {
                converter.putFloat(val);
            }
            return converter.array();
        }
    };

    /**
     * Gets the equalizer properties. This method is useful when a snapshot of current
     * equalizer settings must be saved by the application.
     * @return an RockchipEqualizer.Settings object containing all current parameters values
     */
    @NonNull
    public RockchipEqualizer.Settings getProperties() {
        byte[] param = new byte[2180];
        checkStatus(getParameter(PARAM_PROPERTIES, param));
        Settings settings = new Settings(param);
        return settings;
    }

    /**
     * Sets the equalizer properties. This method is useful when equalizer settings have to
     * be applied from a previous backup.
     * @param settings an RockchipEqualizer.Settings object containing the properties to apply
     */
    public void setProperties(@NonNull RockchipEqualizer.Settings settings) {
        byte[] param = settings.toBytes();
        checkStatus(setParameter(PARAM_PROPERTIES, param));
    }
}
