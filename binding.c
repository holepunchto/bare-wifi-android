#include <assert.h>
#include <bare.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <js.h>
#include <string.h>
#include <utf.h>

#ifdef __ANDROID__
#include <jni.h>

// Populated via bare_android_wifi_manager_init() at app startup.
// TODO: replace with bare_android_get_java_vm() / bare_android_get_context() once
// the bare-android runtime exposes an API for addon-accessible Android context.
static JavaVM *android_jvm = NULL;
static jobject android_context = NULL;
static jobject multicast_lock = NULL;
#endif

// Returns the IPv4 address of the first WiFi-like interface as a JS string,
// or null if none is found. Uses getifaddrs (Android NDK 24+, macOS, Linux).
static js_value_t *
bare_android_wifi_manager_get_wifi_ip (js_env_t *env, js_callback_info_t *info) {
  int err;

  struct ifaddrs *ifap;
  if (getifaddrs(&ifap) != 0) {
    js_value_t *result;
    err = js_get_null(env, &result);
    assert(err == 0);
    return result;
  }

  // Preferred interface names in order (Android = wlan0, macOS = en0, Linux = wlan0/wlp*)
  const char *preferred[] = {"wlan0", "wlan1", "en0", "en1", NULL};
  char ip[INET_ADDRSTRLEN] = {0};

  for (int i = 0; preferred[i] != NULL && ip[0] == '\0'; i++) {
    for (struct ifaddrs *ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
      if (strcmp(ifa->ifa_name, preferred[i]) != 0) continue;
      struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;
      inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
      break;
    }
  }

  // Fallback: any non-loopback IPv4
  if (ip[0] == '\0') {
    for (struct ifaddrs *ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
      struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;
      uint32_t addr = ntohl(sa->sin_addr.s_addr);
      if (addr >> 24 == 127) continue;
      inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
      break;
    }
  }

  freeifaddrs(ifap);

  js_value_t *result;
  if (ip[0] == '\0') {
    err = js_get_null(env, &result);
  } else {
    err = js_create_string_utf8(env, (utf8_t *) ip, -1, &result);
  }
  assert(err == 0);
  return result;
}

#ifdef __ANDROID__

// Called once at app startup with the JavaVM pointer (as BigInt) and a JNI
// global ref to the Android application Context.
//
// In a Pear Android app shell this would look like:
//   const addon = require('bare-android-wifi-manager')
//   addon.init(bare_android.javaVM, bare_android.applicationContext)
//
// TODO: remove once bare-android exposes a stable addon API for Context access.
static js_value_t *
bare_android_wifi_manager_init (js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];
  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  uint64_t jvm_ptr;
  err = js_get_value_bigint_uint64(env, argv[0], &jvm_ptr, NULL);
  assert(err == 0);
  android_jvm = (JavaVM *) (uintptr_t) jvm_ptr;

  uint64_t ctx_ptr;
  err = js_get_value_bigint_uint64(env, argv[1], &ctx_ptr, NULL);
  assert(err == 0);
  android_context = (jobject) (uintptr_t) ctx_ptr;

  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

static js_value_t *
bare_android_wifi_manager_acquire_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;

  if (android_jvm == NULL || android_context == NULL) {
    err = js_throw_error(env, NULL, "bare-android-wifi-manager: call init() first");
    assert(err == 0);
    return NULL;
  }

  JNIEnv *jni;
  (*android_jvm)->AttachCurrentThread(android_jvm, &jni, NULL);

  size_t argc = 1;
  js_value_t *argv[1];
  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  utf8_t tag[64];
  memcpy(tag, "bare-mdns", 10);
  if (argc > 0) js_get_value_string_utf8(env, argv[0], tag, sizeof(tag), NULL);

  jclass ctx_class = (*jni)->FindClass(jni, "android/content/Context");
  jfieldID wifi_field = (*jni)->GetStaticFieldID(jni, ctx_class, "WIFI_SERVICE", "Ljava/lang/String;");
  jobject wifi_service_name = (*jni)->GetStaticObjectField(jni, ctx_class, wifi_field);

  jmethodID get_service = (*jni)->GetMethodID(jni, ctx_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
  jobject wifi_mgr = (*jni)->CallObjectMethod(jni, android_context, get_service, wifi_service_name);

  jclass wifi_mgr_class = (*jni)->GetObjectClass(jni, wifi_mgr);
  jmethodID create_lock = (*jni)->GetMethodID(jni, wifi_mgr_class, "createMulticastLock", "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;");
  jobject lock = (*jni)->CallObjectMethod(jni, wifi_mgr, create_lock, (*jni)->NewStringUTF(jni, (const char *) tag));

  jclass lock_class = (*jni)->GetObjectClass(jni, lock);
  jmethodID acquire = (*jni)->GetMethodID(jni, lock_class, "acquire", "()V");
  (*jni)->CallVoidMethod(jni, lock, acquire);

  if (multicast_lock != NULL) (*jni)->DeleteGlobalRef(jni, multicast_lock);
  multicast_lock = (*jni)->NewGlobalRef(jni, lock);

  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

static js_value_t *
bare_android_wifi_manager_release_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;

  if (android_jvm == NULL || multicast_lock == NULL) {
    js_value_t *result;
    err = js_get_undefined(env, &result);
    assert(err == 0);
    return result;
  }

  JNIEnv *jni;
  (*android_jvm)->AttachCurrentThread(android_jvm, &jni, NULL);

  jclass lock_class = (*jni)->GetObjectClass(jni, multicast_lock);
  jmethodID release = (*jni)->GetMethodID(jni, lock_class, "release", "()V");
  (*jni)->CallVoidMethod(jni, multicast_lock, release);

  (*jni)->DeleteGlobalRef(jni, multicast_lock);
  multicast_lock = NULL;

  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

#else

static js_value_t *
bare_android_wifi_manager_init (js_env_t *env, js_callback_info_t *info) {
  int err;
  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

static js_value_t *
bare_android_wifi_manager_acquire_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;
  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

static js_value_t *
bare_android_wifi_manager_release_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;
  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

#endif

static js_value_t *
bare_android_wifi_manager_exports (js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("init", bare_android_wifi_manager_init)
  V("acquireMulticastLock", bare_android_wifi_manager_acquire_multicast_lock)
  V("releaseMulticastLock", bare_android_wifi_manager_release_multicast_lock)
  V("getWifiIP", bare_android_wifi_manager_get_wifi_ip)
#undef V

  return exports;
}

BARE_MODULE(bare_android_wifi_manager, bare_android_wifi_manager_exports)
