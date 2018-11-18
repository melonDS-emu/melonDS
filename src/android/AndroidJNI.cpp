#include <jni.h>
#include "MelonDS.h"
#include "InputAndroid.h"

extern "C"
{
JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_setupEmulator(JNIEnv* env, jobject thiz, jstring configDir)
{
    const char* dir = env->GetStringUTFChars(configDir, JNI_FALSE);
    MelonDSAndroid::setup(const_cast<char *>(dir));
}

JNIEXPORT jboolean JNICALL
Java_me_magnum_melonds_ui_RenderActivity_loadRom(JNIEnv* env, jobject thiz, jstring romPath, jstring sramPath)
{
    const char* rom = env->GetStringUTFChars(romPath, JNI_FALSE);
    const char* sram = env->GetStringUTFChars(sramPath, JNI_FALSE);

    return MelonDSAndroid::loadRom(const_cast<char *>(rom), const_cast<char *>(sram)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_startEmulation( JNIEnv* env, jobject thiz, jlong initialTicks)
{
    u64 ticks = static_cast<u64>(initialTicks);
    MelonDSAndroid::start(ticks);
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_loop( JNIEnv* env, jobject thiz, jlong currentTicks)
{
    u64 ticks = static_cast<u64>(currentTicks);
    MelonDSAndroid::loop(ticks);
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_copyFrameBuffer( JNIEnv* env, jobject thiz, jobject frameBuffer)
{
    void* buf = env->GetDirectBufferAddress(frameBuffer);
    MelonDSAndroid::copyFrameBuffer(buf);
}

JNIEXPORT jint JNICALL
Java_me_magnum_melonds_ui_RenderActivity_getFPS( JNIEnv* env, jobject thiz)
{
    return MelonDSAndroid::getFPS();
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_stopEmulation( JNIEnv* env, jobject thiz)
{
    MelonDSAndroid::cleanup();
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_onScreenTouch( JNIEnv* env, jobject thiz, jint x, jint y)
{
    MelonDSAndroid::touchScreen(x, y);
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_onScreenRelease( JNIEnv* env, jobject thiz)
{
    MelonDSAndroid::releaseScreen();
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_onKeyPress( JNIEnv* env, jobject thiz, jint key)
{
    MelonDSAndroid::pressKey(key);
}

JNIEXPORT void JNICALL
Java_me_magnum_melonds_ui_RenderActivity_onKeyRelease( JNIEnv* env, jobject thiz, jint key)
{
    MelonDSAndroid::releaseKey(key);
}
}
