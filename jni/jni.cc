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
#include <fstream>
#include <semaphore.h>

#include <boost/locale.hpp>

#include <jni.h>
#include <elle/os/environ.hh>
#include <surface/gap/gap.hh>

#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/network/Interface.hh>

#include <reactor/scheduler.hh>

#ifdef INFINIT_ANDROID
#include <android/log.h>
#endif

ELLE_LOG_COMPONENT("jni");

class JavaException: public std::runtime_error
{
public:
  JavaException()
  : std::runtime_error("java exception") {}
};

static jobject global_state_instance = nullptr;
static gap_State* global_cstate_instance = nullptr;
static int request_map = 0;
static sem_t semaphore;
static std::map<std::string, elle::network::Interface> interface_map;
static std::map<std::string, elle::network::Interface> interface_get_map();

static jobject to_integer(JNIEnv* env, int value)
{
  static jclass i_class = 0;
  static jmethodID i_init;
  if (!i_class)
  {
    i_class = env->FindClass("java/lang/Integer");
    i_class = (jclass)env->NewGlobalRef(i_class);
    i_init = env->GetMethodID(i_class, "<init>", "(I)V");
  }
  jobject obj = env->NewObject(i_class, i_init, value);
  return obj;
}

static std::string to_string(JNIEnv* env, jobject jso)
{
  jstring js = (jstring)jso;
  jboolean isCopy;
  const jchar * jc = env->GetStringChars(js, &isCopy);
  jsize len = env->GetStringLength(js);
  std::string result = boost::locale::conv::utf_to_utf<char, short>(
    (const short*)jc, (const short*)jc+len);
  env->ReleaseStringChars(js, jc);
  return result;
}

static jstring from_string(JNIEnv* env, std::string const& str)
{
  auto ws = boost::locale::conv::utf_to_utf<short, char>(
    str.data(), str.data() + str.length());
  return env->NewString((const jchar*)ws.data(), ws.length());
}

static jobject throw_exception(JNIEnv* env, gap_Status st)
{
  static jclass e_class = 0;
  static jmethodID e_init;
  if (!e_class)
  {
    e_class = env->FindClass("io/infinit/State$StateException");
    ELLE_WARN("class: %s", e_class);
    e_class = (jclass)env->NewGlobalRef(e_class);
    e_init = env->GetMethodID(e_class, "<init>", "(Lio/infinit/State;I)V");
    ELLE_WARN("method: %s", e_init);
  }
  jobject obj = env->NewObject(e_class, e_init, global_state_instance, (int)st);
  env->Throw((jthrowable)obj);
  return {};
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
    env->DeleteLocalRef(v);
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
    if (!u_class)
      return nullptr;
    u_class = (jclass)env->NewGlobalRef(u_class);
    u_init = env->GetMethodID(u_class, "<init>", "()V");
  }
  jobject res = env->NewObject(u_class, u_init);
  jfieldID f;
  jobject tmp;
  f = env->GetFieldID(u_class, "id", "I");
  env->SetIntField(res, f, u.id);
  f = env->GetFieldID(u_class, "status", "Z");
  env->SetBooleanField(res, f, u.status);
  f = env->GetFieldID(u_class, "fullname", "Ljava/lang/String;");
  tmp = from_string(env, u.fullname);
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(u_class, "handle", "Ljava/lang/String;");
  tmp = from_string(env, u.handle);
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(u_class, "metaId", "Ljava/lang/String;");
  tmp = env->NewStringUTF(u.meta_id.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(u_class, "swagger", "Z");
  env->SetBooleanField(res, f, u.swagger);
  f = env->GetFieldID(u_class, "deleted", "Z");
  env->SetBooleanField(res, f, u.deleted);
  f = env->GetFieldID(u_class, "ghost", "Z");
  env->SetBooleanField(res, f, u.ghost);
  f = env->GetFieldID(u_class, "phoneNumber", "Ljava/lang/String;");
  tmp = env->NewStringUTF(u.phone_number.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(u_class, "ghostCode", "Ljava/lang/String;");
  tmp = env->NewStringUTF(u.ghost_code.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(u_class, "ghostInvitationUrl", "Ljava/lang/String;");
  tmp = env->NewStringUTF(u.ghost_invitation_url.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  return res;
}


static jobject to_linktransaction(JNIEnv* env, surface::gap::LinkTransaction const& t)
{
  static jclass lt_class = 0;
  static jclass ts_class;
  static jmethodID ts_get_value;
  static jmethodID lt_init;
  if (!lt_class)
  {
    lt_class = env->FindClass("io/infinit/LinkTransaction");
    lt_class = (jclass)env->NewGlobalRef(lt_class);
    lt_init = env->GetMethodID(lt_class, "<init>", "()V");
    ts_class = env->FindClass("io/infinit/TransactionStatus");
    ts_class = (jclass)env->NewGlobalRef(ts_class);
    ts_get_value = env->GetStaticMethodID(ts_class, "GetValue", "(I)Lio/infinit/TransactionStatus;");
  }
  jobject res = env->NewObject(lt_class, lt_init);
  jobject tmp;
  jfieldID f;
  f = env->GetFieldID(lt_class, "id", "I");
  env->SetIntField(res, f, t.id);
  f = env->GetFieldID(lt_class, "name", "Ljava/lang/String;");
  tmp = from_string(env, t.name);
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(lt_class, "mtime", "D");
  env->SetDoubleField(res, f, t.mtime);
  f = env->GetFieldID(lt_class, "link", "Ljava/lang/String;");
  tmp = env->NewStringUTF(t.link? t.link->c_str():"");
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(lt_class, "clickCount", "I");
  env->SetIntField(res, f, t.click_count);
  f = env->GetFieldID(lt_class, "status", "Lio/infinit/TransactionStatus;");
  jobject status = env->CallStaticObjectMethod(ts_class, ts_get_value, (int)t.status);
  env->SetObjectField(res, f, status);
  env->DeleteLocalRef(status);
  f = env->GetFieldID(lt_class, "senderDeviceId", "Ljava/lang/String;");
  tmp = env->NewStringUTF(t.sender_device_id.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(lt_class, "metaId", "Ljava/lang/String;");
  tmp = env->NewStringUTF(t.meta_id.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
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
  jobject tmp;
  jfieldID f;
  f = env->GetFieldID(pt_class, "id", "I");
  env->SetIntField(res, f, t.id);
  f = env->GetFieldID(pt_class, "status", "Lio/infinit/TransactionStatus;");
  jobject status = env->CallStaticObjectMethod(ts_class, ts_get_value, (int)t.status);
  env->SetObjectField(res, f, status);
  env->DeleteLocalRef(status);
  f = env->GetFieldID(pt_class, "senderId", "I");
  env->SetIntField(res, f, t.sender_id);
  f = env->GetFieldID(pt_class, "senderDeviceId", "Ljava/lang/String;");
  tmp = env->NewStringUTF(t.sender_device_id.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(pt_class, "recipientId", "I");
  env->SetIntField(res, f, t.recipient_id);
  f = env->GetFieldID(pt_class, "recipientDeviceId", "Ljava/lang/String;");
  tmp = env->NewStringUTF(t.recipient_device_id.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(pt_class, "mtime", "D");
  env->SetDoubleField(res, f, t.mtime);

  f = env->GetFieldID(pt_class, "fileNames", "[Ljava/lang/String;");
  jobject empty_string = env->NewObject(string_class, string_init);
  jobjectArray array = env->NewObjectArray(t.file_names.size(), string_class,
                                           empty_string);
  int idx = 0;
  for (std::string const& s: t.file_names)
  {
    tmp = env->NewStringUTF(s.c_str());
    env->SetObjectArrayElement(array, idx, tmp);
    env->DeleteLocalRef(tmp);
    ++idx;
  }
  env->SetObjectField(res, f, array);
  env->DeleteLocalRef(array);
  f = env->GetFieldID(pt_class, "totalSize", "J");
  env->SetLongField(res, f, t.total_size);
  f = env->GetFieldID(pt_class, "isDirectory", "Z");
  env->SetBooleanField(res, f, t.is_directory);
  f = env->GetFieldID(pt_class, "message", "Ljava/lang/String;");
  tmp = from_string(env, t.message);
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
  f = env->GetFieldID(pt_class, "metaId", "Ljava/lang/String;");
  tmp = env->NewStringUTF(t.meta_id.c_str());
  env->SetObjectField(res, f, tmp);
  env->DeleteLocalRef(tmp);
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
    jstring key = from_string(env, e.first);
    jstring value = from_string(env, e.second);
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
#ifdef __arm__
  java_vm->AttachCurrentThread(&env, 0);
#else
  java_vm->AttachCurrentThread((void**)&env, 0);
#endif
  return env;
}
static void on_critical(jobject thiz)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onCritical", "()V");
  ELLE_TRACE("Invoking onCritical at %s", m);
  env->CallVoidMethod(thiz, m);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_update_user(jobject thiz, surface::gap::User const& user)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onUpdateUser", "(Lio/infinit/User;)V");
  jobject u = to_user(env, user);
  ELLE_TRACE("Invoking onUpdateUser at %s", m);
  env->CallVoidMethod(thiz, m, u);
  env->DeleteLocalRef(u);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_deleted_swagger(jobject thiz, int id)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onDeletedSwagger", "(I)V");
  ELLE_TRACE("Invoking onDeletedSwagger at %s", m);
  env->CallVoidMethod(thiz, m, (jint)id);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_deleted_favorite(jobject thiz, int id)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onDeletedFavorite", "(I)V");
  ELLE_TRACE("Invoking onDeletedFavorite at %s",m);
  env->CallVoidMethod(thiz, m, (jint)id);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_user_status(jobject thiz, int id, bool s)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onUserStatus", "(IZ)V");
  ELLE_TRACE("Invoking onUserStatus at %s", m);
  env->CallVoidMethod(thiz, m, (jint)id, (jboolean)s);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_avatar_available(jobject thiz, int id)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onAvatarAvailable", "(I)V");
  ELLE_TRACE("Invoking onAvatarAvailable at %s", m);
  env->CallVoidMethod(thiz, m, (jint)id);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_connection(jobject thiz, bool status, bool still_trying,
                          std::string const& last_error)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onConnection", "(ZZLjava/lang/String;)V");
  ELLE_TRACE("Invoking onConnection at %s", m);
  env->CallVoidMethod(thiz, m, (jboolean)status, (jboolean)still_trying,
                      env->NewStringUTF(last_error.c_str()));
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_peer_transaction(jobject thiz,
                                surface::gap::PeerTransaction const& t)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onPeerTransaction",
                                 "(Lio/infinit/PeerTransaction;)V");
  jobject pt = to_peertransaction(env, t);
  ELLE_TRACE("Invoking onPeerTransaction %s at %s", t, m);
  env->CallVoidMethod(thiz, m, pt);
  env->DeleteLocalRef(pt);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

static void on_link(jobject thiz,
                    surface::gap::LinkTransaction const& t)
{
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(thiz);
  jmethodID m = env->GetMethodID(clazz, "onLink",
                                 "(Lio/infinit/LinkTransaction;)V");
  jobject lt = to_linktransaction(env, t);
  ELLE_TRACE("Invoking onLink at %s", m);
  env->CallVoidMethod(thiz, m, lt);
  env->DeleteLocalRef(lt);
  env->DeleteLocalRef(clazz);
  if (env->ExceptionCheck() == JNI_TRUE)
    throw JavaException();
}

#ifdef INFINIT_ANDROID
class AndroidLogger: public elle::log::TextLogger
{
public:
  AndroidLogger(std::string const& log_level, std::ostream& out);
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

AndroidLogger::AndroidLogger(std::string const& log_level,
                             std::ostream& out)
: elle::log::TextLogger(out, log_level, true, false, true, true, false)
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
  elle::log::TextLogger::_message(level, type, component, time, message, tags,
                                  indentation, file, line, function);
}
#endif

extern "C" jlong Java_io_infinit_State_gapInitialize(JNIEnv* env,
  jobject thiz, jboolean production, jstring download_dir,
  jstring persistent_config_dir, jstring non_persistent_config_dir,
  jboolean enable_mirroring,
  jlong max_mirroring_size)
{
  if (global_cstate_instance)
  {
    ELLE_ERR("gapInitialize called again");
    global_state_instance = env->NewGlobalRef(thiz);
    gap_State* state = global_cstate_instance;
    gap_critical_callback(state, boost::bind(on_critical, thiz));
    gap_update_user_callback(state, boost::bind(on_update_user, thiz, _1));
    gap_deleted_swagger_callback(state, boost::bind(on_deleted_swagger, thiz, _1));
    gap_deleted_favorite_callback(state, boost::bind(on_deleted_favorite, thiz, _1));
    gap_user_status_callback(state, boost::bind(on_user_status, thiz, _1, _2));
    gap_avatar_available_callback(state, boost::bind(on_avatar_available, thiz, _1));
    gap_connection_callback(state, boost::bind(on_connection, thiz, _1, _2, _3));
    gap_peer_transaction_callback(state, boost::bind(on_peer_transaction, thiz, _1));
    gap_link_callback(state, boost::bind(on_link, thiz, _1));
    return (jlong)global_cstate_instance;
  }
  sem_init(&semaphore, 0, 0);
  thiz = env->NewGlobalRef(thiz);
  global_state_instance = thiz;
  std::string persistent_config_dir_str = to_string(env, persistent_config_dir);

#ifdef INFINIT_ANDROID
  std::string log_file = persistent_config_dir_str + "/state.log";
  std::string log_level =
          "jni:DEBUG,"
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
  std::ofstream* output = new std::ofstream{
    log_file,
    std::fstream::trunc | std::fstream::out};
  std::unique_ptr<elle::log::Logger> logger
    = elle::make_unique<AndroidLogger>(log_level, *output);
  elle::log::logger(std::move(logger));
  ELLE_LOG("gapInitialize");
#endif

  gap_State* state = gap_new(production,
    to_string(env, download_dir), persistent_config_dir_str,
    to_string(env, non_persistent_config_dir),
    enable_mirroring,
    max_mirroring_size);
  global_cstate_instance = state;
  //using namespace std::placeholders;
  gap_critical_callback(state, boost::bind(on_critical, thiz));
  gap_update_user_callback(state, boost::bind(on_update_user, thiz, _1));
  gap_deleted_swagger_callback(state, boost::bind(on_deleted_swagger, thiz, _1));
  gap_deleted_favorite_callback(state, boost::bind(on_deleted_favorite, thiz, _1));
  gap_user_status_callback(state, boost::bind(on_user_status, thiz, _1, _2));
  gap_avatar_available_callback(state, boost::bind(on_avatar_available, thiz, _1));
  gap_connection_callback(state, boost::bind(on_connection, thiz, _1, _2, _3));
  gap_peer_transaction_callback(state, boost::bind(on_peer_transaction, thiz, _1));
  gap_link_callback(state, boost::bind(on_link, thiz, _1));
  return (jlong)state;
}

extern "C" void Java_io_infinit_State_gapFinalize(
  JNIEnv* env, jobject thiz, jlong handle)
{
  gap_free((gap_State*)handle);
}

extern "C" jlong Java_io_infinit_State_gapLogin(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring mail, jstring hash_password, jstring device_push_token,
  jstring country_code, jstring device_model, jstring device_name)
{
  boost::optional<std::string> token_opt, country_code_opt, device_model_opt, device_name_opt;
  if (device_push_token)
    token_opt = to_string(env, device_push_token);
  if (country_code)
    country_code_opt = to_string(env, country_code);
  if (device_model)
    device_model_opt = to_string(env, device_model);
  if (device_name)
    device_name_opt = to_string(env, device_name);
  ELLE_LOG("Invoking login(cc=%s, dev=%s)", country_code_opt? *country_code_opt:"none",
           device_model_opt?*device_model_opt : "none");
  gap_Status s = gap_login((gap_State*)handle, to_string(env, mail),
    to_string(env, hash_password), token_opt, country_code_opt,
    device_model_opt, device_name_opt);
  return s;
}

extern "C" jlong Java_io_infinit_State_gapRegister(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring fullname, jstring mail, jstring hash_password,
  jstring device_push_token, jstring country_code,
  jstring device_model, jstring device_name)
{
  boost::optional<std::string> token_opt, country_code_opt, device_model_opt, device_name_opt;
  if (device_push_token)
    token_opt = to_string(env, device_push_token);
  if (country_code)
    country_code_opt = to_string(env, country_code);
  if (device_model)
    device_model_opt = to_string(env, device_model);
  if (device_name)
    device_name_opt = to_string(env, device_name);

  gap_Status s = gap_register((gap_State*)handle, to_string(env, fullname),
                              to_string(env, mail), to_string(env, hash_password),
                              token_opt, country_code_opt, device_model_opt,
                              device_name_opt);
  return s;
}

extern "C" jlong Java_io_infinit_State_gapUseGhostCode(
  JNIEnv* env, jobject thiz, jlong handle, jstring code, jboolean was_link)
{
  return gap_use_ghost_code((gap_State*)handle, to_string(env, code), was_link);
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
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return nullptr;
  }
  surface::gap::PeerTransaction t;
  gap_Status s = gap_peer_transaction_by_id((gap_State*)handle, id, t);
  if (s == gap_ok)
    return to_peertransaction(env, t);
  else
    return throw_exception(env, s);
}

extern "C" jfloat Java_io_infinit_State_gapTransactionProgress(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return gap_transaction_progress((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapTransactionIsFinal(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return gap_transaction_is_final((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapTransactionConcernDevice(
  JNIEnv* env, jobject thiz, jlong handle, jint id, jboolean true_empty_recipient)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return gap_transaction_concern_device((gap_State*)handle, id, true_empty_recipient);
}

extern "C" jlong Java_io_infinit_State_gapPoll(
  JNIEnv* env, jobject thiz, jlong handle)
{
  if (request_map)
  {
    interface_map = interface_get_map();
    do {
      sem_post(&semaphore);
    }
    while (--request_map);
  }
  try {
    return gap_poll((gap_State*)handle);
  }
  catch (JavaException const& e)
  {
    ELLE_TRACE("Java exception thrown from poll handler");
  }
  return 1;
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
  return from_string(env, gap_self_fullname((gap_State*)handle));
}

extern "C" jlong Java_io_infinit_State_gapSetSelfFullname(
   JNIEnv* env, jobject thiz, jlong handle, jstring name)
{
  return gap_set_self_fullname((gap_State*)handle, to_string(env, name));
}

extern "C" jstring Java_io_infinit_State_gapSelfHandle(
  JNIEnv* env, jobject thiz, jlong handle)
{
  return from_string(env, gap_self_handle((gap_State*)handle));
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
  jlong res = gap_self_id((gap_State*)handle);
  if (!res)
    ELLE_ERR("selfId: %s", res);
  return res;
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
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return nullptr;
  }
  gap_avatar((gap_State*)handle, id, &data, &len);
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
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return nullptr;
  }
  surface::gap::User res;
  gap_Status s = gap_user_by_id((gap_State*)handle, id, res);
  if (s == gap_ok)
    return to_user(env, res);
  else
  {
    ELLE_WARN("gap_user_by_id(%s): %s", id, s);
    return throw_exception(env, s);
  }
}

extern "C" jobject Java_io_infinit_State_gapUserByEmail(
  JNIEnv* env, jobject thiz, jlong handle, jstring email)
{
  surface::gap::User res;
  gap_Status s = gap_user_by_email((gap_State*)handle, to_string(env, email), res);
  if (s == gap_ok)
    return to_user(env, res);
  else
    return throw_exception(env, s);
}

extern "C" jobject Java_io_infinit_State_gapUserByHandle(
  JNIEnv* env, jobject thiz, jlong handle, jstring email)
{
  surface::gap::User res;
  gap_Status s = gap_user_by_handle((gap_State*)handle, to_string(env, email), res);
  if (s == gap_ok)
    return to_user(env, res);
  else
    return throw_exception(env, s);
}

extern "C" jobject Java_io_infinit_State_gapUserByMetaId(
  JNIEnv* env, jobject thiz, jlong handle, jstring metaid)
{
  if (!metaid)
  {
    return throw_exception(env, gap_user_id_not_valid);
  }
  surface::gap::User res;
  gap_Status s = gap_user_by_meta_id((gap_State*)handle, to_string(env, metaid), res);
  if (s == gap_ok)
    return to_user(env, res);
  else
    return throw_exception(env, s);
}

extern "C" jlong Java_io_infinit_State_gapUserStatus(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return (jlong)gap_user_status((gap_State*)handle, id);
}

extern "C" jobject Java_io_infinit_State_gapUsersSearch(
  JNIEnv* env, jobject thiz, jlong handle, jstring query)
{
  std::vector<surface::gap::User> res;
  gap_Status s = gap_users_search((gap_State*)handle, to_string(env, query), res);
  if (s == gap_ok)
    return to_array<decltype(res)>(env, res, "io/infinit/User", &to_user);
  else
    return throw_exception(env, s);
}

extern "C" jobject Java_io_infinit_State_gapSwaggers(
  JNIEnv* env, jobject thiz, jlong handle)
{
  std::vector<surface::gap::User> res;
  gap_Status s = gap_swaggers((gap_State*)handle, res);
  if (s == gap_ok)
    return to_array<decltype(res)>(env, res, "io/infinit/User", &to_user);
  else
    return throw_exception(env, s);
}

extern "C" jobject Java_io_infinit_State_gapFavorites(
  JNIEnv* env, jobject thiz, jlong handle)
{
  std::vector<uint32_t> res;
  gap_Status s = gap_favorites((gap_State*)handle, res);
  if (s == gap_ok)
  {
    jintArray r = env->NewIntArray(res.size());
    env->SetIntArrayRegion(r, 0, res.size(), (const int *)&res[0]);
    return r;
  }
  else
    return throw_exception(env, s);
}

extern "C" jlong Java_io_infinit_State_gapFavorite(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return gap_favorite((gap_State*)handle, id);
}

extern "C" jlong Java_io_infinit_State_gapUnfavorite(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return gap_unfavorite((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapIsFavorite(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  return gap_is_favorite((gap_State*)handle, id);
}

extern "C" jboolean Java_io_infinit_State_gapIsLinkTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
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
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return nullptr;
  }
  surface::gap::LinkTransaction res;
  gap_Status s = gap_link_transaction_by_id((gap_State*)handle, id, res);
  if (s == gap_ok)
    return to_linktransaction(env, res);
  else
    return throw_exception(env, s);
}

extern "C" jobject Java_io_infinit_State_gapLinkTransactions(
  JNIEnv* env, jobject thiz, jlong handle)
{
  std::vector<surface::gap::LinkTransaction> res;
  gap_Status s = gap_link_transactions((gap_State*)handle, res);
  if (s == gap_ok)
    return to_array<decltype(res)>(env, res, "io/infinit/LinkTransaction", &to_linktransaction);
  else
    return throw_exception(env, s);
}

extern "C" jobject Java_io_infinit_State_gapPeerTransactions(
  JNIEnv* env, jobject thiz, jlong handle)
{
  std::vector<surface::gap::PeerTransaction> res;
  gap_Status s = gap_peer_transactions((gap_State*)handle, res);
  if (s == gap_ok)
    return to_array<decltype(res)>(env, res, "io/infinit/PeerTransaction", &to_peertransaction);
  else
    return throw_exception(env, s);
}

extern "C" jint Java_io_infinit_State_gapSendFiles(
  JNIEnv* env, jobject thiz, jlong handle,
  jint id, jobjectArray jfiles, jstring message, jstring sdevice_id)
{
  if (id == 0)
  {
    throw_exception(env, gap_user_id_not_valid);
    return 0;
  }
  boost::optional<std::string> device_id;
  if (sdevice_id)
    device_id = to_string(env, sdevice_id);
  std::vector<std::string> files = from_array<std::string>(env, jfiles, to_string);

  return gap_send_files((gap_State*)handle, id, files, to_string(env, message), device_id);
}

extern "C" jint Java_io_infinit_State_gapSendFilesByEmail(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring id, jobjectArray jfiles, jstring message)
{
  std::string email = to_string(env, id);
  if (email.empty())
  {
    throw_exception(env, gap_email_not_valid);
    return 0;
  }
  std::vector<std::string> files = from_array<std::string>(env, jfiles, to_string);
  return gap_send_files((gap_State*)handle, email, files, to_string(env, message));
}

extern "C" jint Java_io_infinit_State_gapPauseTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_transaction_id_not_valid);
    return 0;
  }
  return gap_pause_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapResumeTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_transaction_id_not_valid);
    return 0;
  }
  return gap_resume_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapCancelTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_transaction_id_not_valid);
    return 0;
  }
  return gap_cancel_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapDeleteTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_transaction_id_not_valid);
    return 0;
  }
  return gap_delete_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapRejectTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_transaction_id_not_valid);
    return 0;
  }
  return gap_reject_transaction((gap_State*)handle, id);
}
extern "C" jint Java_io_infinit_State_gapAcceptTransaction(
  JNIEnv* env, jobject thiz, jlong handle, jint id)
{
  if (id == 0)
  {
    throw_exception(env, gap_transaction_id_not_valid);
    return 0;
  }
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
  for (unsigned int i=0; i<extra.size(); i+=2)
    add[extra[i]] = extra[i+1];
  return gap_send_metric((gap_State*)handle, (UIMetricsType)metric, add);
}

extern "C" jlong Java_io_infinit_State_gapSendUserReport(
  JNIEnv* env, jobject thiz, jlong handle, jstring un, jstring m, jstring f)
{
  std::vector<std::string> files;
  if (f)
  {
    std::string file = to_string(env, f);
    if (!file.empty())
      files.push_back(file);
  }
  files.push_back(getenv("INFINIT_LOG_FILE"));
  return gap_send_user_report((gap_State*)handle, to_string(env, un),
                              to_string(env, m), files);
}

extern "C" jlong Java_io_infinit_State_gapSendLastCrashLogs(
  JNIEnv* env, jobject thiz, jlong handle, jstring a, jstring b, jstring c, jstring d)
{
  return gap_send_last_crash_logs((gap_State*)handle, to_string(env, a),
                              to_string(env, b), to_string(env, c),
                              to_string(env, d), true);
}

extern "C" void Java_io_infinit_State_setenv(
  JNIEnv* env, jobject thiz, jstring key, jstring value)
{
  elle::os::setenv(to_string(env, key), to_string(env, value), true);
}


extern "C" jstring Java_io_infinit_State_getenv(
  JNIEnv* env, jobject thiz, jstring key)
{
  return env->NewStringUTF(elle::os::getenv(to_string(env, key)).c_str());
}

extern "C" jstring Java_io_infinit_State_gapFacebookAppId(JNIEnv* env, jobject thiz, jlong handdle)
{
  return env->NewStringUTF(gap_facebook_app_id().c_str());
}

extern "C" jlong Java_io_infinit_State_gapFacebookConnect(
  JNIEnv* env, jobject thiz, jlong handle,
  jstring facebook_token, jstring preferred_email,
  jstring device_push_token,
  jstring country_code,
  jstring device_model,
  jstring device_name)
{
  boost::optional<std::string> mail, token, country, model, name;
  if (preferred_email != nullptr)
    mail = to_string(env, preferred_email);
  if (device_push_token != nullptr)
    token = to_string(env, device_push_token);
  if (country_code != nullptr)
    country = to_string(env, country_code);
  if (device_model != nullptr)
    model = to_string(env, device_model);
  if (device_name != nullptr)
    name = to_string(env, device_name);
  return gap_facebook_connect((gap_State*)handle, to_string(env, facebook_token),
                              mail, token, country, model, name);
}

extern "C" jlong Java_io_infinit_State_gapFacebookAlreadyRegistered(
  JNIEnv* env, jobject thiz, jlong handle, jstring facebook_id)
{
  bool res;
  gap_Status s = gap_facebook_already_registered((gap_State*)handle,
                                                 to_string(env, facebook_id),
                                                 res);
  if (s != 1)
    return s;
  return res? 1:0;
}

extern "C" jlong Java_io_infinit_State_gapUploadAddressBook(
  JNIEnv* env, jobject thiz, jlong handle, jstring jjson)
{
  std::string json = to_string(env, jjson);
  return gap_upload_address_book((gap_State*)handle, json);
}

extern "C" jlong Java_io_infinit_State_gapSendSmsGhostCodeMetric(
  JNIEnv* env, jobject thiz, jlong handle, jboolean success, jstring code, jstring fail_reason)
{
  return gap_send_sms_ghost_code_metric((gap_State*)handle, success,
    to_string(env, code), to_string(env, fail_reason));
}

extern "C" jstring Java_io_infinit_State_gapSessionId(
  JNIEnv* env, jobject thiz, jlong handle)
{
  std::string res;
  gap_session_id((gap_State*)handle, res);
  return from_string(env, res);
}

std::map<std::string, elle::network::Interface> interface_get_map()
{
  std::map<std::string, elle::network::Interface> result;
  JNIEnv* env = get_env();
  jclass clazz = env->GetObjectClass(global_state_instance);
  jmethodID m = env->GetMethodID(clazz, "getNetworkInterfaces", "()[Lio/infinit/NetInterface;");
  jclass ifClass = env->FindClass("io/infinit/NetInterface");
  jfieldID idMac = env->GetFieldID(ifClass, "mac", "Ljava/lang/String;");
  jfieldID idIP = env->GetFieldID(ifClass, "ip", "Ljava/lang/String;");
  jfieldID idName = env->GetFieldID(ifClass, "name", "Ljava/lang/String;");

  jobjectArray res = (jobjectArray)env->CallObjectMethod(global_state_instance, m);
  int sz = env->GetArrayLength(res);
  for (int i=0; i<sz; ++i)
  {
    jobject e = env->GetObjectArrayElement(res, i);
    jobject jmac = env->GetObjectField(e, idMac);
    jobject jip = env->GetObjectField(e, idIP);
    jobject jname = env->GetObjectField(e, idName);
    std::string mac = to_string(env, jmac);
    std::string ip = to_string(env, jip);
    std::string name = to_string(env, jname);
    if (ip.find_first_of('.') == ip.npos)
      continue;
    elle::network::Interface itf;
    itf.mac_address = mac;
    itf.ipv4_address = ip;
    result[name] = itf;
  }
  return result;
}

namespace elle
{
  namespace network
  {
    std::map<std::string, Interface>
    Interface::get_map(Interface::Filter filter)
    {
      ++request_map;
      ELLE_TRACE("Requesting interface map, waiting...");
      while (sem_trywait(&semaphore) == -1)
        reactor::sleep(100_ms);
      ELLE_TRACE("...done, got %s results", interface_map.size());
      return interface_map;
    }
  }
}
