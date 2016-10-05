#include <jni.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/android_sink.h"

#include "Callstack.h"

#include <cmath>

extern "C"
{
int fact(int x, std::shared_ptr<spdlog::logger> logger)
{
    if (x == 1) {
        Callstack st;
        logger->info("{}", Tiny(st));
        return 1;
    }
    return x * fact(x - 1, logger);
}

JNIEXPORT void JNICALL Java_launcher_uvdemo_app_MyLib_initialize(JNIEnv* env, jobject obj)
{
    auto androidSink = std::make_shared<spdlog::sinks::android_sink_mt>("launcher.uvdemo.app");
    auto logger = spdlog::create("MyLib", {androidSink});
    logger->info("Welcome to spdlog!");
    auto x = std::round(1.5f);
    x++;
    int y = fact(30);
    logger->info("fact: {}", y);
}
} // extern "C"
