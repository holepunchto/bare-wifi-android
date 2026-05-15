#include <assert.h>
#include <bare.h>
#include <js.h>
#include <utf.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>

#ifdef __ANDROID__
#include <jnitl.h>

static java_global_ref_t<java_object_t<"android/net/wifi/WifiManager$MulticastLock">> multicast_lock;

static inline java_vm_t
get_jvm () {
  return java_vm_t::get_created().value();
}

// Follows the same pattern as bare-bluetooth-android: get the Android
// application context via ActivityThread.currentApplication() without
// requiring an explicit init() call from JS.
static inline java_object_t<"android/content/Context">
get_context (JNIEnv *env) {
  auto cls = java_class_t<"android/app/ActivityThread">(env);
  auto current_app = cls.get_static_method<java_object_t<"android/app/Application">()>("currentApplication");
  return java_object_t<"android/content/Context">(env, current_app());
}
#endif

// Returns the IPv4 address of the active WiFi interface as a JS string, or null.
// Works via getifaddrs on Android NDK 24+, macOS, and Linux.
static js_value_t *
bare_wifi_android_get_wifi_ip (js_env_t *env, js_callback_info_t *info) {
  int err;

  struct ifaddrs *ifap;
  if (getifaddrs(&ifap) != 0) {
    js_value_t *result;
    err = js_get_null(env, &result);
    assert(err == 0);
    return result;
  }

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

static js_value_t *
bare_wifi_android_acquire_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];
  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  utf8_t tag[64];
  memcpy(tag, "bare-mdns", 10);
  if (argc > 0) js_get_value_string_utf8(env, argv[0], tag, sizeof(tag), NULL);

  auto jvm = get_jvm();
  auto guard = jvm.attach_current_thread();
  JNIEnv *jni = guard.env();

  auto ctx = get_context(jni);
  auto ctx_cls = java_class_t<"android/content/Context">(jni);
  auto get_service = ctx_cls.get_method<java_object_t<"java/lang/Object">(java_object_t<"java/lang/String">)>("getSystemService");

  // Context.WIFI_SERVICE == "wifi"
  auto service_name = java_object_t<"java/lang/String">(jni, jni->NewStringUTF("wifi"));
  auto wifi_mgr = java_object_t<"android/net/wifi/WifiManager">(jni, (jobject) get_service(ctx, service_name));

  auto wm_cls = java_class_t<"android/net/wifi/WifiManager">(jni);
  auto create_lock = wm_cls.get_method<java_object_t<"android/net/wifi/WifiManager$MulticastLock">(java_object_t<"java/lang/String">)>("createMulticastLock");
  auto tag_str = java_object_t<"java/lang/String">(jni, jni->NewStringUTF((const char *) tag));
  auto lock = create_lock(wifi_mgr, tag_str);

  auto lock_cls = java_class_t<"android/net/wifi/WifiManager$MulticastLock">(jni);
  auto acquire = lock_cls.get_method<void()>("acquire");
  acquire(lock);

  multicast_lock = java_global_ref_t<java_object_t<"android/net/wifi/WifiManager$MulticastLock">>(jni, lock);

  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

static js_value_t *
bare_wifi_android_release_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;

  if (!multicast_lock) {
    js_value_t *result;
    err = js_get_undefined(env, &result);
    assert(err == 0);
    return result;
  }

  auto jvm = get_jvm();
  auto guard = jvm.attach_current_thread();
  JNIEnv *jni = guard.env();

  auto lock = multicast_lock.get(jni);
  auto lock_cls = java_class_t<"android/net/wifi/WifiManager$MulticastLock">(jni);
  auto release = lock_cls.get_method<void()>("release");
  release(lock);

  multicast_lock.reset();

  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

#else

static js_value_t *
bare_wifi_android_acquire_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;
  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

static js_value_t *
bare_wifi_android_release_multicast_lock (js_env_t *env, js_callback_info_t *info) {
  int err;
  js_value_t *result;
  err = js_get_undefined(env, &result);
  assert(err == 0);
  return result;
}

#endif

static js_value_t *
bare_wifi_android_exports (js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("acquireMulticastLock", bare_wifi_android_acquire_multicast_lock)
  V("releaseMulticastLock", bare_wifi_android_release_multicast_lock)
  V("getWifiIP", bare_wifi_android_get_wifi_ip)
#undef V

  return exports;
}

BARE_MODULE(bare_wifi_android, bare_wifi_android_exports)
