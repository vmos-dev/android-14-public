/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.car.hardware;

import static com.android.car.internal.ExcludeFromCodeCoverageGeneratedReport.BOILERPLATE_CODE;

import static java.lang.Integer.toHexString;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.car.VehiclePropertyIds;
import android.car.annotation.AddedInOrBefore;
import android.car.builtin.os.ParcelHelper;
import android.os.Parcel;
import android.os.Parcelable;

import com.android.car.internal.ExcludeFromCodeCoverageGeneratedReport;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.Objects;

/**
 * Stores a value for a vehicle property ID and area ID combination.
 *
 * Client should use {@code android.car.*} types when dealing with property ID, area ID or property
 * value and MUST NOT use {@code android.hardware.automotive.vehicle.*} types directly.
 *
 * @param <T> refer to {@link Parcel#writeValue(java.lang.Object)} to get a list of all supported
 *            types. The class should be visible to framework as default class loader is being used
 *            here.
 */
public final class CarPropertyValue<T> implements Parcelable {
    private static final Charset DEFAULT_CHARSET = StandardCharsets.UTF_8;

    private final int mPropertyId;
    private final int mAreaId;
    private final int mStatus;
    private final long mTimestampNanos;
    private final T mValue;

    @IntDef({
        STATUS_AVAILABLE,
        STATUS_UNAVAILABLE,
        STATUS_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PropertyStatus {}

    /**
     * {@code CarPropertyValue} is available.
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final int STATUS_AVAILABLE = 0;

    /**
     * {@code CarPropertyValue} is unavailable.
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final int STATUS_UNAVAILABLE = 1;

    /**
     * {@code CarPropertyValue} has an error.
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final int STATUS_ERROR = 2;

    /**
     * Creates an instance of {@code CarPropertyValue}.
     *
     * @param propertyId Property ID, must be one of enums in
     *   {@link android.car.VehiclePropertyIds}.
     * @param areaId Area ID of Property, must be one of enums in one of the following classes:
     *   <ul>
     *     <li><{@code VehicleAreaWindow}</li>
     *     <li><{@code VehicleAreaDoor}</li>
     *     <li><{@link android.car.VehicleAreaSeat}</li>
     *     <li><{@code VehicleAreaMirror}</li>
     *     <li><{@link android.car.VehicleAreaWheel}</li>
     *   </ul>
     *   or 0 for global property.
     * @param value Value of Property
     * @hide
     */
    public CarPropertyValue(int propertyId, int areaId, T value) {
        this(propertyId, areaId, /* timestampNanos= */ 0, value);
    }

    /**
     * Creates an instance of {@code CarPropertyValue}. The {@code timestampNanos} is the time in
     * nanoseconds at which the event happened. For a given car property, each new {@code
     * CarPropertyValue} should be monotonically increasing using the same time base as
     * {@link SystemClock#elapsedRealtimeNanos()}.
     *
     * @param propertyId Property ID, must be one of enums in
     *   {@link android.car.VehiclePropertyIds}.
     * @param areaId     Area ID of Property, must be one of enums in one of the following classes:
     *   <ul>
     *     <li><{@code VehicleAreaWindow}</li>
     *     <li><{@code VehicleAreaDoor}</li>
     *     <li><{@link android.car.VehicleAreaSeat}</li>
     *     <li><{@code VehicleAreaMirror}</li>
     *     <li><{@link android.car.VehicleAreaWheel}</li>
     *   </ul>
     *   or 0 for global property.
     * @param timestampNanos  Elapsed time in nanoseconds since boot
     * @param value      Value of Property
     * @hide
     */
    public CarPropertyValue(int propertyId, int areaId, long timestampNanos, T value) {
        this(propertyId, areaId, CarPropertyValue.STATUS_AVAILABLE, timestampNanos, value);
    }

    /**
     * @hide
     *
     * @deprecated use {@link CarPropertyValue#CarPropertyValue(int, int, long, T)} instead
     */
    @Deprecated
    public CarPropertyValue(int propertyId, int areaId, int status, long timestampNanos, T value) {
        mPropertyId = propertyId;
        mAreaId = areaId;
        mStatus = status;
        mTimestampNanos = timestampNanos;
        mValue = value;
    }

    /**
     * Creates an instance of {@code CarPropertyValue}.
     *
     * @param in Parcel to read
     * @hide
     */
    @SuppressWarnings("unchecked")
    public CarPropertyValue(Parcel in) {
        mPropertyId = in.readInt();
        mAreaId = in.readInt();
        mStatus = in.readInt();
        mTimestampNanos = in.readLong();
        String valueClassName = in.readString();
        Class<?> valueClass;
        try {
            valueClass = Class.forName(valueClassName);
        } catch (ClassNotFoundException e) {
            throw new IllegalArgumentException("Class not found: " + valueClassName, e);
        }

        if (String.class.equals(valueClass)) {
            byte[] bytes = ParcelHelper.readBlob(in);
            mValue = (T) new String(bytes, DEFAULT_CHARSET);
        } else if (byte[].class.equals(valueClass)) {
            mValue = (T) ParcelHelper.readBlob(in);
        } else {
            mValue = (T) in.readValue(valueClass.getClassLoader());
        }
    }

    @AddedInOrBefore(majorVersion = 33)
    public static final Creator<CarPropertyValue> CREATOR = new Creator<CarPropertyValue>() {
        @Override
        public CarPropertyValue createFromParcel(Parcel in) {
            return new CarPropertyValue(in);
        }

        @Override
        public CarPropertyValue[] newArray(int size) {
            return new CarPropertyValue[size];
        }
    };

    @Override
    @ExcludeFromCodeCoverageGeneratedReport(reason = BOILERPLATE_CODE)
    @AddedInOrBefore(majorVersion = 33)
    public int describeContents() {
        return 0;
    }

    @Override
    @AddedInOrBefore(majorVersion = 33)
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(mPropertyId);
        dest.writeInt(mAreaId);
        dest.writeInt(mStatus);
        dest.writeLong(mTimestampNanos);

        Class<?> valueClass = mValue == null ? null : mValue.getClass();
        dest.writeString(valueClass == null ? null : valueClass.getName());

        // Special handling for String and byte[] to mitigate transaction buffer limitations.
        if (String.class.equals(valueClass)) {
            ParcelHelper.writeBlob(dest, ((String) mValue).getBytes(DEFAULT_CHARSET));
        } else if (byte[].class.equals(valueClass)) {
            ParcelHelper.writeBlob(dest, (byte[]) mValue);
        } else {
            dest.writeValue(mValue);
        }
    }

    /**
     * @return Property id of {@code CarPropertyValue}, must be one of enums in
     *   {@link android.car.VehiclePropertyIds}.
     */
    @AddedInOrBefore(majorVersion = 33)
    public int getPropertyId() {
        return mPropertyId;
    }

    /**
     * @return Area id of {@code CarPropertyValue}, must be one of enums in one of the following
     * classes:
     *   <ul>
     *     <li><{@code VehicleAreaWindow}</li>
     *     <li><{@code VehicleAreaDoor}</li>
     *     <li><{@link android.car.VehicleAreaSeat}</li>
     *     <li><{@code VehicleAreaMirror}</li>
     *     <li><{@link android.car.VehicleAreaWheel}</li>
     *   </ul>
     *   or 0 for global property.
     */
    @AddedInOrBefore(majorVersion = 33)
    public int getAreaId() {
        return mAreaId;
    }

    /**
     * @return Status of {@code CarPropertyValue}
     *
     * @deprecated This should be added back in next major Android release.
     */
    @AddedInOrBefore(majorVersion = 33)
    public @PropertyStatus int getStatus() {
        return mStatus;
    }

    /**
     * Returns the timestamp in nanoseconds at which the {@code CarPropertyValue} happened. For a
     * given car property, each new {@code CarPropertyValue} should be monotonically increasing
     * using the same time base as {@link android.os.SystemClock#elapsedRealtimeNanos()}.
     *
     * <p>NOTE: Timestamp should be synchronized with other signals from the platform (e.g.
     * {@link android.location.Location} and {@link android.hardware.SensorEvent} instances).
     * Ideally, timestamp synchronization error should be below 1 millisecond.
     */
    @AddedInOrBefore(majorVersion = 33)
    public long getTimestamp() {
        return mTimestampNanos;
    }

    /**
     * Returns the value for {@code CarPropertyValue}.
     *
     * <p>
     * <b>Note:</b>Caller must check the value of {@link #getStatus()}. Only use
     * {@link #getValue()} when {@link #getStatus()} is {@link #STATUS_AVAILABLE}. If not,
     * {@link #getValue()} is meaningless.
     */
    @NonNull
    @AddedInOrBefore(majorVersion = 33)
    public T getValue() {
        return mValue;
    }

    /** @hide */
    @Override
    public String toString() {
        return "CarPropertyValue{"
                + "mPropertyId=0x" + toHexString(mPropertyId)
                + ", propertyName=" + VehiclePropertyIds.toString(mPropertyId)
                + ", mAreaId=0x" + toHexString(mAreaId)
                + ", mStatus=" + mStatus
                + ", mTimestampNanos=" + mTimestampNanos
                + ", mValue=" + mValue
                + '}';
    }

    /** Generates hash code for this instance. */
    @Override
    public int hashCode() {
        return Objects.hash(mPropertyId, mAreaId, mStatus, mTimestampNanos, mValue);
    }

    /** Checks equality with passed {@code object}. */
    @Override
    public boolean equals(Object object) {
        if (this == object) {
            return true;
        }
        if (!(object instanceof CarPropertyValue<?>)) {
            return false;
        }
        CarPropertyValue<?> carPropertyValue = (CarPropertyValue<?>) object;
        return mPropertyId == carPropertyValue.mPropertyId && mAreaId == carPropertyValue.mAreaId
                && mStatus == carPropertyValue.mStatus
                && mTimestampNanos == carPropertyValue.mTimestampNanos
                && Objects.equals(mValue, carPropertyValue.mValue);
    }
}
