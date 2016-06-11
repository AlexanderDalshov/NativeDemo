#include <jni.h>
#include <android/log.h>

#define LOG_INFO(STR, ...)       __android_log_print(ANDROID_LOG_INFO,  "mylib", STR, ##__VA_ARGS__)

extern "C"
{
JNIEXPORT void JNICALL Java_launcher_uvdemo_myapplication_MyLib_test(JNIEnv* env, jobject obj)
{
    LOG_INFO("test");
}
} // extern "C"
