/*
 * Copyright (c) 2021 Rockchip Electronics Co., Ltd
 */

package com.android.server.pm;

/**
 * Settings data for a particular shared user ID we know about.
 */
public class PackagePerformanceSetting {
    String name;
    int mode;

    PackagePerformanceSetting(String _name, int _mode) {
        name = _name;
        mode = _mode;
    }

    public void add(String _name) {
        name = _name;
    }

    public void setMode(int _mode) {
        mode = _mode;
    }

    @Override
    public String toString() {
        return "PackagePerformanceSetting{" + Integer.toHexString(System.identityHashCode(this)) + " "
            + name + "/" + mode + "}";
    }
}
