#include <jni.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/android_sink.h"

extern "C"
{
JNIEXPORT void JNICALL Java_launcher_uvdemo_myapplication_MyLib_initialize(JNIEnv* env, jobject obj)
{
    auto androidSink = std::make_shared<spdlog::sinks::android_sink_mt>("launcher.uvdemo.myapplication");
    auto logger = spdlog::create("MyLib", {androidSink});
    logger->info("Welcome to spdlog!");
}
} // extern "C"
