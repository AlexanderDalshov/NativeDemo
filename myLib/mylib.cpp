#include <jni.h>

#include <signal.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/android_sink.h"
#include "Callstack.h"

static std::shared_ptr<spdlog::logger> g_logger;

namespace {
    struct sigaction old_sa[NSIG];

    void android_sigaction(int signal, siginfo_t *info, void *reserved) {
        using namespace Navionics;
        //(*env)->CallVoidMethod(env, obj, nativeCrashed);
        Callstack stk;
        g_logger->critical("Signal received: {}", signal);
        g_logger->critical(Wide(stk));
        old_sa[signal].sa_handler(signal);
    }
}

#define CATCHSIG(X) sigaction(X, &handler, &old_sa[X])

struct T
{
    int x;

    void foo2() {
        x = 42;
    }

    void foo() {
        foo2();
    }

    void foo3() {
        foo4();
    }

    void foo4() {
        throwException();
    }

    void throwException() {
        using namespace Navionics;
        Callstack stk;
        throw std::runtime_error(Wide(stk));
    }
};

extern "C"
{

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
    auto androidSink = std::make_shared<spdlog::sinks::android_sink_mt>("launcher.uvdemo.app");
    g_logger= spdlog::create("MyLib", {androidSink});
    g_logger->info("Initialize MyLib");

    // Try to catch crashes...
    {
        g_logger->info("Install crash handlers");
        struct sigaction handler;
        memset(&handler, 0, sizeof(struct sigaction));
        handler.sa_sigaction = android_sigaction;
        handler.sa_flags = SA_RESETHAND;

        CATCHSIG(SIGILL);
        CATCHSIG(SIGABRT);
        CATCHSIG(SIGBUS);
        CATCHSIG(SIGFPE);
        CATCHSIG(SIGSEGV);
        CATCHSIG(SIGSTKFLT);
        CATCHSIG(SIGPIPE);
    }
    return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL Java_launcher_uvdemo_app_MyLib_boom(JNIEnv* env, jobject obj)
{
    g_logger->info("crash application");

    T *t = nullptr;
    t->foo();
}

JNIEXPORT void JNICALL Java_launcher_uvdemo_app_MyLib_throwException(JNIEnv* env, jobject obj)
{
    try{
        T t;
        t.foo3();
    } catch(std::runtime_error e) {
        jclass newExcCls = env->FindClass("java/lang/Error");
        if (newExcCls != nullptr)
            env->ThrowNew(newExcCls, e.what());
    } catch(...) {
        jclass newExcCls = env->FindClass("java/lang/Error");
        if (newExcCls != nullptr)
            env->ThrowNew(newExcCls, "unknown exception");
    }
}
} // extern "C"
