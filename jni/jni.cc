/*
 * Copyright (C) 2009 The Android Open Source Project
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
 *
 */
#include <string.h>
#include <jni.h>
#include <surface/gap/gap.hh>

#ifdef INFINIT_ANDROID
#include <android/log.h>
#endif

static std::string to_string(JNIEnv* env, jobject jso)
{
  jstring js = (jstring)jso;
  jboolean isCopy;
  const jchar * jc = env->GetStringChars(js, &isCopy);
  jsize len = env->GetStringLength(js);
  std::string str(jc, jc + len);
  env->ReleaseStringChars(js, jc);
  return str;
}

template<typename T>
std::vector<T>
from_array(JNIEnv* env, jobjectArray src, std::function<T (JNIEnv*, jobject)> convert)
{
  int sz = env->GetArrayLength(src);
  std::vector<T> res;
  for (int i=0; i<sz; ++i)
  {
    jobject e = env->GetObjectArrayElement(src, i);
    res.push_back(convert(env, e));
  }
  return res;
}

template<typename C>
jobject to_array(JNIEnv* env, C const& source,
  std::string const& class_name,
  std::function<jobject (JNIEnv*, typename C::value_type const& v)> element_converter)
{
  jclass c = env->FindClass(class_name.c_str());
  jmethodID init = env->GetMethodID(c, "<init>", "()V");
  jobject obj = env->NewObject(c, init);
  jobjectArray res = env->NewObjectArray(source.size(), c, obj);
  int idx = 0;
  for(auto const& e: source)
  {
    jobject v = element_converter(env, e);
    env->SetObjectArrayElement(res, idx, v);
    ++idx;
  }
  return res;
}

static jobject to_user(JNIEnv* env, surface::gap::User const& u)
{
  static jclass u_class = 0;
  static jmethodID u_init;
  if (!u_class)
  {
    u_class = env->FindClass("io/infinit/User");
    u_class = (jclass)env->NewGlobalRef(u_class);
    u_init = env->GetMethodID(u_class, "<init>", "()V");
  }
  jobject res = env->NewObject(u_class, u_init);
  jfieldID f;
  f = env->GetFieldID(u_class, "id", "I");
  env->SetIntField(res, f, u.id);
  f = env->GetFieldID(u_class, "status", "Z");
  env->SetBooleanField(res, f, u.status);
  f = env->GetFieldID(u_class, "fullname", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(u.fullname.c_str()));
  f = env->GetFieldID(u_class, "handle", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(u.handle.c_str()));
  f = env->GetFieldID(u_class, "metaId", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(u.meta_id.c_str()));
  f = env->GetFieldID(u_class, "swagger", "Z");
  env->SetBooleanField(res, f, u.swagger);
  f = env->GetFieldID(u_class, "deleted", "Z");
  env->SetBooleanField(res, f, u.deleted);
  f = env->GetFieldID(u_class, "ghost", "Z");
  env->SetBooleanField(res, f, u.ghost);
  return res;
}


static jobject to_linktransaction(JNIEnv* env, surface::gap::LinkTransaction const& t)
{
  static jclass lt_class = 0;
  static jclass string_class;
  static jclass ts_class;
  static jmethodID ts_get_value;
  static jmethodID lt_init;
  static jmethodID string_init;
  if (!lt_class)
  {
    lt_class = env->FindClass("io/infinit/LinkTransaction");
    lt_class = (jclass)env->NewGlobalRef(lt_class);
    lt_init = env->GetMethodID(lt_class, "<init>", "()V");
    string_class = env->FindClass("java/lang/String");
    string_class = (jclass)env->NewGlobalRef(string_class);
    string_init = env->GetMethodID(string_class, "<init>", "()V");
    ts_class = env->FindClass("io/infinit/TransactionStatus");
    ts_class = (jclass)env->NewGlobalRef(ts_class);
    ts_get_value = env->GetStaticMethodID(ts_class, "GetValue", "(I)Lio/infinit/TransactionStatus;");
  }
  jobject res = env->NewObject(lt_class, lt_init);
  jfieldID f;
  f = env->GetFieldID(lt_class, "id", "I");
  env->SetIntField(res, f, t.id);
  f = env->GetFieldID(lt_class, "name", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(t.name.c_str()));
  f = env->GetFieldID(lt_class, "mtime", "D");
  env->SetDoubleField(res, f, t.mtime);
  f = env->GetFieldID(lt_class, "link", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(t.link? t.link->c_str():""));
  f = env->GetFieldID(lt_class, "clickCount", "I");
  env->SetIntField(res, f, t.click_count);
  f = env->GetFieldID(lt_class, "status", "Lio/infinit/TransactionStatus;");
  jobject status = env->CallStaticObjectMethod(ts_class, ts_get_value, (int)t.status);
  env->SetObjectField(res, f, status);
  f = env->GetFieldID(lt_class, "senderDeviceId", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(t.sender_device_id.c_str()));
  return res;
}
static jobject to_peertransaction(JNIEnv* env, surface::gap::PeerTransaction const& t)
{
  static jclass pt_class = 0;
  static jclass string_class;
  static jclass ts_class;
  static jmethodID pt_init;
  static jmethodID string_init;
  static jmethodID ts_get_value;
  if (!pt_class)
  {
    pt_class = env->FindClass("io/infinit/PeerTransaction");
    pt_class = (jclass)env->NewGlobalRef(pt_class);
    pt_init = env->GetMethodID(pt_class, "<init>", "()V");
    string_class = env->FindClass("java/lang/String");
    string_class = (jclass)env->NewGlobalRef(string_class);
    string_init = env->GetMethodID(string_class, "<init>", "()V");
    ts_class = env->FindClass("io/infinit/TransactionStatus");
    ts_class = (jclass)env->NewGlobalRef(ts_class);
    ts_get_value = env->GetStaticMethodID(ts_class, "GetValue", "(I)Lio/infinit/TransactionStatus;");
  }
  jobject res = env->NewObject(pt_class, pt_init);
  jfieldID f;
  f = env->GetFieldID(pt_class, "id", "I");
  env->SetIntField(res, f, t.id);
  f = env->GetFieldID(pt_class, "status", "Lio/infinit/TransactionStatus;");
  jobject status = env->CallStaticObjectMethod(ts_class, ts_get_value, (int)t.status);
  env->SetObjectField(res, f, status);

  f = env->GetFieldID(pt_class, "senderId", "I");
  env->SetIntField(res, f, t.sender_id);
  f = env->GetFieldID(pt_class, "senderDeviceId", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(t.sender_device_id.c_str()));
  f = env->GetFieldID(pt_class, "recipientId", "I");
  env->SetIntField(res, f, t.recipient_id);
  f = env->GetFieldID(pt_class, "recipientDeviceId", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(t.recipient_device_id.c_str()));
  f = env->GetFieldID(pt_class, "mtime", "D");
  env->SetDoubleField(res, f, t.mtime);

  f = env->GetFieldID(pt_class, "fileNames", "[Ljava/lang/String;");
  jobject empty_string = env->NewObject(string_class, string_init);
  jobjectArray array = env->NewObjectArray(t.file_names.size(), string_class,
                                           empty_string);
  int idx = 0;
  for (std::string const& s: t.file_names)
  {
    env->SetObjectArrayElement(array, idx, env->NewStringUTF(s.c_str()));
    ++idx;
  }

  f = env->GetFieldID(pt_class, "totalSize", "J");
  env->SetLongField(res, f, t.total_size);
  f = env->GetFieldID(pt_class, "isDirectory", "Z");
  env->SetBooleanField(res, f, t.is_directory);
  f = env->GetFieldID(pt_class, "message", "Ljava/lang/String;");
  env->SetObjectField(res, f, env->NewStringUTF(t.message.c_str()));
  return res;
}

static jobject to_hash(JNIEnv* env, std::unordered_map<std::string, std::string> const& map)
{
  static jclass hash_class = 0;
  static jmethodID init;
  static jmethodID put;
  if (!hash_class)
  {
    hash_class = env->FindClass("java/util/HashMap");
    hash_class = (jclass)env->NewGlobalRef(hash_class);
    init = env->GetMethodID(hash_class, "<init>", "(I)V");
    put = env->GetMethodID(hash_class, "put",
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  }
  jobject hashMap = env->NewObject(hash_class, init, map.size());
  for (auto const& e: map)
  {
    jstring key = env->NewStringUTF(e.first.c_str());
    jstring value = env->NewStringUTF(e.second.c_str());
    jobject res = env->CallObjectMethod(hashMap, put, key, value);
    env->DeleteLocalRef(res);
    env->DeleteLocalRef(key);
    env->DeleteLocalRef(value);
  }
  return hashMap;
}
static JavaVM* java_vm;
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved)
{
  java_vm = vm;
  return JNI_VERSION_1_2;
}

static JNIEnv* get_env()
{
  JNIEnv* env;
  java_vm->AttachCurrentThread(&env, 0);
  return env;
}
static void on_critical(jobject thiz)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onCritical", "()V");
  env->CallVoidMethod(thiz, m);
}
static void on_new_swagger(jobject thiz, surface::gap::User const& u)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onNewSwagger", "(Lio/infinit/User;)V");
  jobject ju = to_user(env, u);
  env->CallVoidMethod(thiz, m, ju);
}

static void on_deleted_swagger(jobject thiz, int id)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onDeleteSwagger", "(I)V");
  env->CallVoidMethod(thiz, m, (jint)id);
}

static void on_deleted_favorite(jobject thiz, int id)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onDeletedFavorite", "(I)V");
  env->CallVoidMethod(thiz, m, (jint)id);
}

static void on_user_status(jobject thiz, int id, bool s)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onUserStatus", "(IZ)V");
  env->CallVoidMethod(thiz, m, (jint)id, (jboolean)s);
}

static void on_avatar_available(jobject thiz, int id)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onAvatarAvailable", "(I)V");
  env->CallVoidMethod(thiz, m, (jint)id);
}

static void on_connection(jobject thiz, bool status, bool still_trying,
                          std::string const& last_error)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onConnection", "(ZZLjava/lang/String;)V");
  env->CallVoidMethod(thiz, m, (jboolean)status, (jboolean)still_trying,
                      env->NewStringUTF(last_error.c_str()));
}

static void on_peer_transaction(jobject thiz,
                                surface::gap::PeerTransaction const& t)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onPeerTransaction",
                                 "(Lio/infinit/PeerTransaction;)V");
  env->CallVoidMethod(thiz, m, to_peertransaction(env, t));
}

static void on_link(jobject thiz,
                                surface::gap::LinkTransaction const& t)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onLink",
                                 "(Lio/infinit/LinkTransaction;)V");
  env->CallVoidMethod(thiz, m, to_linktransaction(env, t));
}

#ifdef INFINIT_ANDROID
class AndroidLogger: public elle::log::Logger
{
public:
  AndroidLogger(std::string const& log_level);
protected:
  void _message(Level level,
               elle::log::Logger::Type type,
               std::string const& component,
               boost::posix_time::ptime const& time,
               std::string const& message,
               std::vector<std::pair<std::string, std::string>> const& tags,
               int indentation,
               std::string const& file,
               unsigned int line,
               std::string const& function) override;
};

AndroidLogger::AndroidLogger(std::string const& log_level)
:elle::log::Logger(log_level)
{
}
void
AndroidLogger::_message(Level level,
               elle::log::Logger::Type type,
               std::string const& component,
               boost::posix_time::ptime const& time,
               std::string const& message,
               std::vector<std::pair<std::string, std::string>> const& tags,
               int indentation,
               std::string const& file,
               unsigned int line,
               std::string const& function)
{
  android_LogPriority prio;
  using elle::log::Logger;
  switch(level)
  {
  case Logger::Level::none:    prio = ANDROID_LOG_SILENT; break;
  case Logger::Level::log:     prio = ANDROID_LOG_INFO; break;
  case Logger::Level::trace:   prio = ANDROID_LOG_INFO; break;
  case Logger::Level::debug:   prio = ANDROID_LOG_DEBUG; break;
  case Logger::Level::dump:    prio = ANDROID_LOG_VERBOSE; break;
  }
  switch(type)
  {
  case Logger::Type::info: break;
  case Logger::Type::warning: prio = ANDROID_LOG_WARN; break;
  case Logger::Type::error: prio = ANDROID_LOG_ERROR; break;
  }
  __android_log_write(prio, component.c_str(), message.c_str());
}
#endif

extern "C" jlong Java_io_infinit_State_gapInitialize(JNIEnv* env,
  jobject thiz, jboolean production, jstring download_dir,
  jstring persistent_config_dir, jstring non_persistent_config_dir,
  jboolean enable_mirroring,
  jlong max_mirroring_size)
{
  thiz = env->NewGlobalRef(thiz);
  gap_State* state = gap_new(production,
    to_string(env, download_dir), to_string(env, persistent_config_dir),
    to_string(env, non_persistent_config_dir),
    enable_mirroring,
    max_mirroring_size);
  using namespace std::placeholders;
  gap_critical_callback(state, std::bind(on_critical, thiz));
  gap_new_swagger_callback(state, std::bind(on_new_swagger, thiz, _1));
  gap_deleted_swagger_callback(state, std::bind(on_deleted_swagger, thiz, _1));
  gap_deleted_favorite_callback(state, std::bind(on_deleted_favorite, thiz, _1));
  gap_user_status_callback(state, std::bind(on_user_status, thiz, _1, _2));
  gap_avatar_available_callback(state, std::bind(on_avatar_available, thiz, _1));
  gap_connection_callback(state, std::bind(on_connection, thiz, _1, _2, _3));
  gap_peer_transaction_callback(state, std::bind(on_peer_transaction, thiz, _1));
  gap_link_callback(state, std::bind(on_link, thiz, _1));
#ifdef INFINIT_ANDROID
  std::string log_level =
          "elle.CrashReporter:DEBUG,"
          "*FIST*:TRACE,"
          "*FIST.State*:DEBUG,"
          "frete.Frete:TRACE,"
          "infinit.surface.gap.Rounds:DEBUG,"
          "*meta*:TRACE,"
          "OSX*:DUMP,"
          "reactor.fsm.*:TRACE,"
          "reactor.network.upnp:DEBUG,"
          "station.Station:DEBUG,"
          "surface.gap.*:TRACE,"
          "surface.gap.TransferMachine:DEBUG,"
          "*trophonius*:DEBUG";
  std::unique_ptr<elle::log::Logger> logger
    = elle::make_unique<AndroidLogger>(log_level);
  elle::log::logger(std::move(logger));
#endif
  return (jlong)state;
}

extern "C" void Java_io_infinit_State_gapFinalize(
  JNIEnv* env, jobject thiz, jlong handle)
{
  gap_free((gap_State*)handle);
}

extern "C" jlong Java_io_infinit_State_gapLogin(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring mail, jstring hash_password)
{
  gap_Status s = gap_login((gap_State*)handle, to_string(env, mail), to_string(env, hash_password));
  return s;
}

extern "C" jlong Java_io_infinit_State_gapRegister(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring fullname, jstring mail, jstring hash_password)
{
  gap_Status s = gap_register((gap_State*)handle, to_string(env, fullname), to_string(env, mail), to_string(env, hash_password));
  return s;
}

extern "C" jlong Java_io_infinit_State_gapSetProxy(
  JNIEnv* env, jobject thiz, jlong handle,
  jint type, jstring host, jshort port,
  jstring username, jstring password)
{
  return gap_set_proxy((gap_State*)handle, (gap_ProxyType)type, to_string(env, host), port,
    to_string(env, username), to_string(env, password));
}

extern "C" jlong Java_io_infinit_State_gapUnsetProxy(
  JNIEnv* env, jobject thiz, jlong handle,
  jint type)
{
  return gap_unset_proxy((gap_State*)handle, (gap_ProxyType)type);
}

extern "C" void Java_io_infinit_State_gapCleanState(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return gap_clean_state((gap_State*)handle);
}

extern "C" jobject Java_io_infinit_State_gapFetchFeatures(
  JNIEnv* env, jobject thiz, jlong handle)
{
  auto features = gap_fetch_features((gap_State*)handle);
  return to_hash(env, features);
}

extern "C" bool Java_io_infinit_State_gapLoggedIn(
   JNIEnv* env, jobject thiz, jlong handle)
{
  return gap_logged_in((gap_State*)handle);
}

extern "C" jlong Java_io_infinit_State_gapLogout(
   JNIEnv* env, jobject thiz, jlong handle)
{
  return gap_logout((gap_State*)handle);
}

extern "C" jlong Java_io_infinit_State_gapInternetConnection(
  JNIEnv* env, jobject thiz, jlong handle, jboolean connected)
{
  return gap_internet_connection((gap_State*)handle, connected);
}

extern "C" jobject Java_io_infinit_State_gapPeerTransactionById(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  surface::gap::PeerTransaction t = gap_peer_transaction_by_id((gap_State*)handle, id);
  return to_peertransaction(env, t);
}

extern "C" jfloat Java_io_infinit_State_gapTransactionProgress(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_transaction_progress((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapTransactionIsFinal(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_transaction_is_final((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapTransactionConcernDevice(
  JNIEnv* env, jobject thiz, jlong handle, jint id, jboolean true_empty_recipient)
{
  return gap_transaction_concern_device((gap_State*)handle, id, true_empty_recipient);
}

extern "C" jlong Java_io_infinit_State_gapPoll(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return gap_poll((gap_State*)handle);
}

extern "C" jlong Java_io_infinit_State_gapDeviceStatus(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return gap_device_status((gap_State*)handle);
}

extern "C" jlong Java_io_infinit_State_gapSetDeviceName(
   JNIEnv* env, jobject thiz, jlong handle, jstring name)
{
  return gap_set_device_name((gap_State*)handle, to_string(env, name));
}

extern "C" jstring Java_io_infinit_State_gapSelfEmail(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return env->NewStringUTF(gap_self_email((gap_State*)handle).c_str());
}

extern "C" jlong Java_io_infinit_State_gapSetSelfEmail(
   JNIEnv* env, jobject thiz, jlong handle, jstring mail, jstring password)
{
  return gap_set_self_email((gap_State*)handle, to_string(env, mail),
    to_string(env, password));
}

extern "C" jstring Java_io_infinit_State_gapSelfFullname(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return env->NewStringUTF(gap_self_fullname((gap_State*)handle).c_str());
}

extern "C" jlong Java_io_infinit_State_gapSetSelfFullname(
   JNIEnv* env, jobject thiz, jlong handle, jstring name)
{
  return gap_set_self_fullname((gap_State*)handle, to_string(env, name));
}

extern "C" jstring Java_io_infinit_State_gapSelfHandle(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return env->NewStringUTF(gap_self_handle((gap_State*)handle).c_str());
}

extern "C" jlong Java_io_infinit_State_gapSetSelfHandle(
   JNIEnv* env, jobject thiz, jlong handle, jstring name)
{
  return gap_set_self_handle((gap_State*)handle, to_string(env, name));
}

extern "C" jlong Java_io_infinit_State_gapChangePassword(
   JNIEnv* env, jobject thiz, jlong handle, jstring old, jstring password)
{
  return gap_change_password((gap_State*)handle, to_string(env, old),
    to_string(env, password));
}

extern "C" jlong Java_io_infinit_State_gapSelfId(
   JNIEnv* env, jobject thiz, jlong handle)
{
  return gap_self_id((gap_State*)handle);
}


extern "C" jlong Java_io_infinit_State_gapUpdateAvatar(
  JNIEnv* env, jobject thiz, jlong handle, jbyteArray data)
{
  jbyte* p = env->GetByteArrayElements(data, 0);
  gap_Status res = gap_update_avatar((gap_State*)handle, p,
    env->GetArrayLength(data));
  env->ReleaseByteArrayElements(data, p, 0);
  return res;
}


extern "C" jstring Java_io_infinit_State_gapSelfDeviceId(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return env->NewStringUTF(gap_self_device_id((gap_State*)handle).c_str());
}

extern "C" jbyteArray Java_io_infinit_State_gapAvatar(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{ // FIXME: find way to report error
  void* data = 0;
  size_t len = 0;
  gap_Status stat = gap_avatar((gap_State*)handle, id, &data, &len);
  jbyteArray res = env->NewByteArray(len);
  env->SetByteArrayRegion(res, 0, len, (jbyte*)data);
  return res;
}

extern "C" void Java_io_infinit_State_gapRefreshAvatar(
   JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  gap_refresh_avatar((gap_State*)handle, id);
}

extern "C" jobject Java_io_infinit_State_gapUserById(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return to_user(env, gap_user_by_id((gap_State*)handle, id));
}

extern "C" jobject Java_io_infinit_State_gapUserByEmail(
  JNIEnv* env, jobject thiz, jlong handle, jstring email)
{
  return to_user(env, gap_user_by_email((gap_State*)handle,
                                        to_string(env, email)));
}

extern "C" jobject Java_io_infinit_State_gapUserByHandle(
  JNIEnv* env, jobject thiz, jlong handle, jstring email)
{
  return to_user(env, gap_user_by_handle((gap_State*)handle,
                                         to_string(env, email)));
}

extern "C" jlong Java_io_infinit_State_gapUserStatus(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return (jlong)gap_user_status((gap_State*)handle, id);
}

extern "C" jobject Java_io_infinit_State_gapUsersSearch(
  JNIEnv* env, jobject thiz, jlong handle, jstring query)
{
  auto result = gap_users_search((gap_State*)handle, to_string(env, query));
  return to_array<decltype(result)>(env, result, "io/infinit/User", &to_user);
}

extern "C" jobject Java_io_infinit_State_gapSwaggers(
  JNIEnv* env, jobject thiz, jlong handle)
{
  auto result = gap_swaggers((gap_State*)handle);
  return to_array<decltype(result)>(env, result, "io/infinit/User", &to_user);
}

extern "C" jobject Java_io_infinit_State_gapFavorites(
  JNIEnv* env, jobject thiz, jlong handle)
{
  auto result = gap_favorites((gap_State*)handle);
  jintArray r = env->NewIntArray(result.size());
  for (int i=0; i<result.size(); ++i)
    env->SetIntArrayRegion(r, 0, result.size(), (const int *)&result[0]);
  return r;
}

extern "C" jlong Java_io_infinit_State_gapFavorite(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_favorite((gap_State*)handle, id);
}

extern "C" jlong Java_io_infinit_State_gapUnfavorite(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_unfavorite((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapIsFavorite(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_is_favorite((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapIsLinkTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_is_link_transaction((gap_State*)handle, id);
}

extern "C" int Java_io_infinit_State_gapCreateLinkTransaction(
  JNIEnv* env, jobject thiz, jlong handle,
  jobjectArray jfiles, jstring message)
{
  std::vector<std::string> files = from_array<std::string>(env, jfiles, to_string);
  return gap_create_link_transaction((gap_State*)handle, files,
                                     to_string(env, message));
}

extern "C" jobject Java_io_infinit_State_gapLinkTransactionById(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  surface::gap::LinkTransaction res = gap_link_transaction_by_id((gap_State*)handle, id);
  return to_linktransaction(env, res);
}

extern "C" jobject Java_io_infinit_State_gapLinkTransactions(
  JNIEnv* env, jobject thiz, jlong handle)
{
  auto r = gap_link_transactions((gap_State*)handle);
  return to_array<decltype(r)>(env, r, "io/infinit/LinkTransaction", &to_linktransaction);
}

extern "C" jobject Java_io_infinit_State_gapPeerTransactions(
  JNIEnv* env, jobject thiz, jlong handle)
{
  auto r = gap_peer_transactions((gap_State*)handle);
  return to_array<decltype(r)>(env, r, "io/infinit/PeerTransaction", &to_peertransaction);
}

extern "C" jint Java_io_infinit_State_gapSendFiles(
  JNIEnv* env, jobject thiz, jlong handle,
  jint id, jobjectArray jfiles, jstring message)
{
  std::vector<std::string> files = from_array<std::string>(env, jfiles, to_string);
  
  return gap_send_files((gap_State*)handle, id, files, to_string(env, message));
}

extern "C" jint Java_io_infinit_State_gapSendFilesByEmail(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring id, jobjectArray jfiles, jstring message)
{
  std::vector<std::string> files = from_array<std::string>(env, jfiles, to_string);
  return gap_send_files_by_email((gap_State*)handle, to_string(env, id), files, to_string(env, message));
}

extern "C" jint Java_io_infinit_State_gapPauseTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_pause_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapResumeTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_resume_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapCancelTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_cancel_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapDeleteTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_delete_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapRejectTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_reject_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapAcceptTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  return gap_accept_transaction((gap_State*)handle, id);
}

extern "C" jint Java_io_infinit_State_gapOnboardingReceiveTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jstring path, int tt)
{
  return gap_onboarding_receive_transaction((gap_State*)handle,
                                            to_string(env, path),
                                            tt);
}

extern "C" jlong Java_io_infinit_State_gapOnboardingSetPeerStatus(
  JNIEnv* env, jobject thiz, jlong handle, int id, jboolean status)
{
  return gap_onboarding_set_peer_status((gap_State*)handle, id, status);
}

extern "C" jlong Java_io_infinit_State_gapOnboardingSetPeerAvailability(
  JNIEnv* env, jobject thiz, jlong handle, int id, jboolean status)
{
  return gap_onboarding_set_peer_availability((gap_State*)handle, id, status);
}

extern "C" jlong Java_io_infinit_State_gapSetOutputDir(
  JNIEnv* env, jobject thiz, jlong handle, jstring path, jboolean fallback)
{
  return gap_set_output_dir((gap_State*)handle, to_string(env, path), fallback);
}

extern "C" jstring Java_io_infinit_State_gapGetOutputDir(
  JNIEnv* env, jobject thiz, jlong handle)
{
  std::string res = gap_get_output_dir((gap_State*)handle);
  return env->NewStringUTF(res.c_str());
}

extern "C" jlong Java_io_infinit_State_gapSendMetric(
  JNIEnv* env, jobject thiz, jlong handle, jlong metric, jobjectArray jextra)
{
  // restore extra map
  std::unordered_map<std::string, std::string> add;
  std::vector<std::string> extra = from_array<std::string>(env, jextra, to_string);
  for (int i=0; i<extra.size(); i+=2)
    add[extra[i]] = extra[i+1];
  return gap_send_metric((gap_State*)handle, (UIMetricsType)metric, add);
}

extern "C" jlong Java_io_infinit_State_gapSendUserReport(
  JNIEnv* env, jobject thiz, jlong handle, jstring un, jstring m, jstring f)
{
  return gap_send_user_report((gap_State*)handle, to_string(env, un),
                              to_string(env, m), to_string(env, f));
}

extern "C" jlong Java_io_infinit_State_gapSendLastCrashLogs(
  JNIEnv* env, jobject thiz, jlong handle, jstring a, jstring b, jstring c, jstring d)
{
  return gap_send_last_crash_logs((gap_State*)handle, to_string(env, a),
                              to_string(env, b), to_string(env, c),
                              to_string(env, d));
}