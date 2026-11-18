package com.radiant.daw;

import android.view.ViewGroup;

public final class NativeBridge {
    static {
        System.loadLibrary("radiant");
    }

    private NativeBridge() {}

    public static native void initialiseJuceGui();
    public static native void attachEditor(ViewGroup host);
    public static native void detachEditor();
    public static native void endAndroidSession();
    public static native boolean isAndroidSessionActive();
    public static native void setEditorMode(boolean compact, int selectedPanel);
    public static native void setEditorPalette(int primary, int secondary, int tertiary,
                                               int background, int surface, int surfaceVariant, int onSurface);
    public static native boolean isEditorAttached();
    public static native int[] getEditorSize();
    public static native int getSelectedPanel();
    public static native int getVisiblePanelCount();
    public static native boolean isSelectedPanelMaximised();
    public static native int[] getWorkspacePanelState(int panel);
    public static native boolean isJuceGuiInitialised();
    public static native String getAudioOutputDeviceName();
    public static native int[] getAudioOutputConfiguration();
    public static native void play();
    public static native void stopPlayback();
    public static native boolean isTransportPlaying();
    public static native void setAudioRecordingArmed(boolean armed);
    public static native boolean isAudioRecordingArmed();
    public static native void requestAndroidOpen();
    public static native void saveCurrentProject();
    public static native void saveCurrentProjectAs();
    public static native int takeAndroidStorageAction();
    public static native boolean openAndroidProject(String path, String displayName);
    public static native boolean saveAndroidProject(String path, String displayName);
    public static native String getAndroidProjectPath();
    public static native String getAndroidProjectName();
    public static native double getAndroidSongBpm();
    public static native void markAndroidSaveFailed();
    public static native void saveSettingsNow();
    public static native void deliverAndroidSoundFont(String path, String error);
    public static native void handleTouchWheel(long peerHost, Object peerView, float localX, float localY,
                                               int viewWidth, int viewHeight, float deltaX, float deltaY, long timeMs);
    public static native void handleTouchPinch(long peerHost, Object peerView, float localX, float localY,
                                               int viewWidth, int viewHeight, float scale, boolean vertical, long timeMs);
    public static native void cancelQuickTouch();
    public static native boolean isMultitouchArea(long peerHost, Object peerView, float localX, float localY, int viewWidth, int viewHeight);
}
