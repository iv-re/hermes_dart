#pragma GCC visibility push(default)
#include "hermes_dart.h"
#pragma GCC visibility pop

#include <mutex>
#include <string>
#include <unordered_map>

#include "hermes/ADT/ManagedChunkedList.h"
#include "hermes/BCGen/HBC/HBC.h"
#include "hermes/Public/RuntimeConfig.h"
#include "hermes/Support/UTF16Stream.h"
#include "hermes/Support/UTF8.h"
#include "hermes/VM/Callable.h"
#include "hermes/VM/HostModel.h"
#include "hermes/VM/JSArray.h"
#include "hermes/VM/JSArrayBuffer.h"
#include "hermes/VM/JSLib/RuntimeJSONParse.h"
#include "hermes/VM/Runtime.h"
#include "llvh/Support/ConvertUTF.h"

using namespace hermes;
using namespace facebook::hermes;

#define HERMES_ABI_POINTER_TYPES(F) \
  F(Object)                         \
  F(Array)                          \
  F(String)                         \
  F(BigInt)                         \
  F(Symbol)                         \
  F(Function)                       \
  F(ArrayBuffer)                    \
  F(PropNameID)                     \
  F(WeakObject)

namespace abi {

#define DECLARE_HERMES_ABI_POINTER_HELPERS(name)                          \
  inline HermesABI##name create##name(HermesABIManagedPointer *ptr) {     \
    return {ptr};                                                         \
  }                                                                       \
  inline HermesABI##name##OrError create##name##OrError(                  \
    HermesABIManagedPointer *ptr                                          \
  ) {                                                                     \
    return {(uintptr_t)ptr};                                              \
  }                                                                       \
  inline HermesABI##name##OrError create##name##OrError(                  \
    HermesABIErrorCode err                                                \
  ) {                                                                     \
    return {static_cast<uintptr_t>((err << 2) | 1)};                      \
  }                                                                       \
  inline bool isError(const HermesABI##name##OrError &p) {                \
    return p.ptr_or_error & 1;                                            \
  }                                                                       \
  inline HermesABIErrorCode getError(const HermesABI##name##OrError &p) { \
    assert(isError(p));                                                   \
    return (HermesABIErrorCode)(p.ptr_or_error >> 2);                     \
  }                                                                       \
  inline HermesABI##name get##name(HermesABI##name##OrError p) {          \
    assert(!isError(p));                                                  \
    return create##name((HermesABIManagedPointer *)p.ptr_or_error);       \
  }
HERMES_ABI_POINTER_TYPES(DECLARE_HERMES_ABI_POINTER_HELPERS)
#undef DECLARE_HERMES_ABI_POINTER_HELPERS

inline void releasePointer(HermesABIManagedPointer *mp) {
  if (mp && mp->vtable && mp->vtable->invalidate) {
    mp->vtable->invalidate(mp);
  }
}

inline HermesABIVoidOrError createVoidOrError(void) {
  return {0};
}

inline HermesABIVoidOrError createVoidOrError(HermesABIErrorCode err) {
  return {(uintptr_t)((err << 2) | 1)};
}

inline bool isError(const HermesABIVoidOrError &v) {
  return v.void_or_error & 1;
}

inline HermesABIErrorCode getError(const HermesABIVoidOrError &v) {
  assert(isError(v));
  return (HermesABIErrorCode)(v.void_or_error >> 2);
}

inline HermesABIBoolOrError createBoolOrError(bool val) {
  return {(uintptr_t)((val ? 1 : 0) << 2)};
}

inline HermesABIBoolOrError createBoolOrError(HermesABIErrorCode err) {
  return {(uintptr_t)((err << 2) | 1)};
}

inline bool isError(const HermesABIBoolOrError &p) {
  return p.bool_or_error & 1;
}

inline HermesABIErrorCode getError(const HermesABIBoolOrError &p) {
  return (HermesABIErrorCode)(p.bool_or_error >> 2);
}

inline bool getBool(const HermesABIBoolOrError &p) {
  return p.bool_or_error >> 2;
}

inline HermesABIPropNameIDListPtrOrError createPropNameIDListPtrOrError(
  HermesABIPropNameIDList *ptr
) {
  return {(uintptr_t)ptr};
}

inline HermesABIPropNameIDListPtrOrError createPropNameIDListPtrOrError(
  HermesABIErrorCode err
) {
  return {static_cast<uintptr_t>((err << 2) | 1)};
}

inline bool isError(HermesABIPropNameIDListPtrOrError p) {
  return p.ptr_or_error & 1;
}

inline HermesABIErrorCode getError(HermesABIPropNameIDListPtrOrError p) {
  assert(isError(p));
  return (HermesABIErrorCode)(p.ptr_or_error >> 2);
}

inline HermesABIPropNameIDList *getPropNameIDListPtr(
  HermesABIPropNameIDListPtrOrError p
) {
  assert(!isError(p));
  return (HermesABIPropNameIDList *)p.ptr_or_error;
}

inline HermesABIValue createUndefinedValue() {
  HermesABIValue val;
  val.kind = HermesABIValueKindUndefined;
  return val;
}

inline HermesABIValue createNullValue() {
  HermesABIValue val;
  val.kind = HermesABIValueKindNull;
  return val;
}

inline HermesABIValue createBoolValue(bool b) {
  HermesABIValue val;
  val.kind = HermesABIValueKindBoolean;
  val.data.boolean = b;
  return val;
}

inline HermesABIValue createNumberValue(double d) {
  HermesABIValue val;
  val.kind = HermesABIValueKindNumber;
  val.data.number = d;
  return val;
}

inline HermesABIValue createObjectValue(HermesABIManagedPointer *ptr) {
  HermesABIValue val;
  val.kind = HermesABIValueKindObject;
  val.data.pointer = ptr;
  return val;
}

inline HermesABIValue createObjectValue(const HermesABIObject &obj) {
  return createObjectValue(obj.pointer);
}

inline HermesABIValue createStringValue(HermesABIManagedPointer *ptr) {
  HermesABIValue val;
  val.kind = HermesABIValueKindString;
  val.data.pointer = ptr;
  return val;
}

inline HermesABIValue createStringValue(const HermesABIString &str) {
  return createStringValue(str.pointer);
}

inline HermesABIValue createBigIntValue(HermesABIManagedPointer *ptr) {
  HermesABIValue val;
  val.kind = HermesABIValueKindBigInt;
  val.data.pointer = ptr;
  return val;
}

inline HermesABIValue createBigIntValue(const HermesABIBigInt &bi) {
  return createBigIntValue(bi.pointer);
}

inline HermesABIValue createSymbolValue(HermesABIManagedPointer *ptr) {
  HermesABIValue val;
  val.kind = HermesABIValueKindSymbol;
  val.data.pointer = ptr;
  return val;
}

inline HermesABIValue createSymbolValue(const HermesABISymbol &sym) {
  return createSymbolValue(sym.pointer);
}

inline HermesABIValueKind getValueKind(const HermesABIValue &val) {
  return val.kind;
}

inline bool isUndefinedValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindUndefined;
}

inline bool isNullValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindNull;
}

inline bool isBoolValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindBoolean;
}

inline bool isNumberValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindNumber;
}

inline bool isObjectValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindObject;
}

inline bool isStringValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindString;
}

inline bool isBigIntValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindBigInt;
}

inline bool isSymbolValue(const HermesABIValue &val) {
  return getValueKind(val) == HermesABIValueKindSymbol;
}

inline bool getBoolValue(const HermesABIValue &val) {
  assert(isBoolValue(val));
  return val.data.boolean;
}

inline double getNumberValue(const HermesABIValue &val) {
  assert(isNumberValue(val));
  return val.data.number;
}

inline HermesABIObject getObjectValue(const HermesABIValue &val) {
  assert(isObjectValue(val));
  return createObject(val.data.pointer);
}

inline HermesABIString getStringValue(const HermesABIValue &val) {
  assert(isStringValue(val));
  return createString(val.data.pointer);
}

inline HermesABIBigInt getBigIntValue(const HermesABIValue &val) {
  assert(isBigIntValue(val));
  return createBigInt(val.data.pointer);
}

inline HermesABISymbol getSymbolValue(const HermesABIValue &val) {
  assert(isSymbolValue(val));
  return createSymbol(val.data.pointer);
}

inline HermesABIManagedPointer *getPointerValue(const HermesABIValue &val) {
  assert(getValueKind(val) & HERMES_ABI_POINTER_MASK);
  return val.data.pointer;
}

inline void releaseValue(const HermesABIValue &val) {
  if (getValueKind(val) & HERMES_ABI_POINTER_MASK)
    releasePointer(getPointerValue(val));
}

inline HermesABIValueOrError createValueOrError(HermesABIValue val) {
  HermesABIValueOrError res;
  res.value = val;
  return res;
}

inline HermesABIValueOrError createValueOrError(HermesABIErrorCode err) {
  HermesABIValueOrError res;
  res.value.kind = HermesABIValueKindError;
  res.value.data.error = err;
  return res;
}

inline bool isError(const HermesABIValueOrError &val) {
  return getValueKind(val.value) == HermesABIValueKindError;
}

inline HermesABIValue getValue(const HermesABIValueOrError &val) {
  assert(!isError(val));
  return val.value;
}

inline HermesABIErrorCode getError(const HermesABIValueOrError &val) {
  assert(isError(val));
  return val.value.data.error;
}

}  // namespace abi

namespace {

template <typename T>
class ManagedValue : public HermesABIManagedPointer {
  static void invalidate(HermesABIManagedPointer *ptr) {
    static_cast<ManagedValue<T> *>(ptr)->dec();
  }
  static constexpr HermesABIManagedPointerVTable vt{invalidate};

 public:
  ManagedValue()
      : HermesABIManagedPointer{&vt}, refCount_(0) {}

  bool isFree() const {
#if LLVM_THREAD_SANITIZER_BUILD
    return refCount_.load(std::memory_order_acquire) == 0;
#else
    return refCount_.load(std::memory_order_relaxed) == 0;
#endif
  }

  template <typename... Args>
  void emplace(Args &&...args) {
    assert(isFree() && "Emplacing already occupied value");
    refCount_.store(1, std::memory_order_relaxed);
    new (&value_) T(std::forward<Args>(args)...);
  }

  ManagedValue<T> *getNextFree() {
    assert(isFree() && "Free pointer unusable while occupied");
    return nextFree_;
  }

  void setNextFree(ManagedValue<T> *nextFree) {
    assert(isFree() && "Free pointer unusable while occupied");
    nextFree_ = nextFree;
  }

  T &value() {
    assert(!isFree() && "Value not present");
    return value_;
  }

  const T &value() const {
    assert(!isFree() && "Value not present");
    return value_;
  }

  void inc() {
    auto oldCount = refCount_.fetch_add(1, std::memory_order_relaxed);
    assert(oldCount && "Cannot resurrect a pointer");
    assert(oldCount + 1 != 0 && "Ref count overflow");
    (void)oldCount;
  }

  void dec() {
#if LLVM_THREAD_SANITIZER_BUILD
    auto oldCount = refCount_.fetch_sub(1, std::memory_order_release);
#else
    auto oldCount = refCount_.fetch_sub(1, std::memory_order_relaxed);
#endif
    assert(oldCount > 0 && "Ref count underflow");
    (void)oldCount;
  }

 private:
  std::atomic<uint32_t> refCount_;
  union {
    T value_;
    ManagedValue<T> *nextFree_;
  };
};

}  // namespace

template <typename T = vm::HermesValue>
vm::Handle<T> toHandle(HermesABIManagedPointer *value) {
  return vm::Handle<T>::vmcast(
    &static_cast<ManagedValue<vm::PinnedHermesValue> *>(value)->value()
  );
}

inline vm::Handle<vm::JSObject> toHandle(HermesABIObject obj) {
  return toHandle<vm::JSObject>(obj.pointer);
}

inline vm::Handle<vm::StringPrimitive> toHandle(HermesABIString str) {
  return toHandle<vm::StringPrimitive>(str.pointer);
}

inline vm::Handle<vm::SymbolID> toHandle(HermesABISymbol sym) {
  return toHandle<vm::SymbolID>(sym.pointer);
}

inline vm::Handle<vm::SymbolID> toHandle(HermesABIPropNameID sym) {
  return toHandle<vm::SymbolID>(sym.pointer);
}

inline vm::Handle<vm::JSArray> toHandle(HermesABIArray arr) {
  return toHandle<vm::JSArray>(arr.pointer);
}

inline vm::Handle<vm::BigIntPrimitive> toHandle(HermesABIBigInt bi) {
  return toHandle<vm::BigIntPrimitive>(bi.pointer);
}

inline vm::Handle<vm::Callable> toHandle(HermesABIFunction fn) {
  return toHandle<vm::Callable>(fn.pointer);
}

inline vm::Handle<vm::JSArrayBuffer> toHandle(HermesABIArrayBuffer ab) {
  return toHandle<vm::JSArrayBuffer>(ab.pointer);
}

vm::HermesValue toHermesValue(const HermesABIValue &val) {
  switch (abi::getValueKind(val)) {
    case HermesABIValueKindUndefined:
      return vm::HermesValue::encodeUndefinedValue();
    case HermesABIValueKindNull:
      return vm::HermesValue::encodeNullValue();
    case HermesABIValueKindBoolean:
      return vm::HermesValue::encodeBoolValue(abi::getBoolValue(val));
    case HermesABIValueKindNumber:
      return vm::HermesValue::encodeUntrustedNumberValue(
        abi::getNumberValue(val)
      );
    case HermesABIValueKindString:
    case HermesABIValueKindObject:
    case HermesABIValueKindSymbol:
    case HermesABIValueKindBigInt:
      return *toHandle<>(val.data.pointer);
    default:
      hermes_fatal("Value has an unexpected tag.");
  }
}

struct HermesABIRuntime {
  std::shared_ptr<::hermes::vm::Runtime> rt;
  ManagedChunkedList<ManagedValue<vm::PinnedHermesValue>> hermesValues;
  ManagedChunkedList<
    ManagedValue<vm::WeakRoot<vm::JSObject>>>
    weakHermesValues;

  std::string nativeExceptionMessage{};
  vm::PinnedHermesValue last_js_error;
  hbc::CompileFlags compileFlags{};
  bool is_releasing = false;

  explicit HermesABIRuntime(const hermes::vm::RuntimeConfig &runtimeConfig)
      : rt(hermes::vm::Runtime::create(runtimeConfig)),
        hermesValues(runtimeConfig.getGCConfig().getOccupancyTarget(), 0.5),
        weakHermesValues(
          runtimeConfig.getGCConfig().getOccupancyTarget(),
          0.5
        ) {
    last_js_error = vm::HermesValue::encodeUndefinedValue();
    compileFlags.emitAsyncBreakCheck =
      runtimeConfig.getAsyncBreakCheckInEval();
    compileFlags.enableGenerator = runtimeConfig.getEnableGenerator();
    compileFlags.enableAsyncGenerators =
      runtimeConfig.getEnableAsyncGenerators();
    compileFlags.enableES6BlockScoping =
      runtimeConfig.getES6BlockScoping();
    switch (runtimeConfig.getCompilationMode()) {
      case hermes::vm::CompilationMode::SmartCompilation:
        compileFlags.lazy = true;
        break;
      case hermes::vm::CompilationMode::ForceEagerCompilation:
        compileFlags.lazy = false;
        break;
      case hermes::vm::CompilationMode::ForceLazyCompilation:
        compileFlags.lazy = true;
        compileFlags.preemptiveFileCompilationThreshold = 0;
        compileFlags.preemptiveFunctionCompilationThreshold = 0;
        break;
    }
    rt->addCustomRootsFunction([this](vm::GC *, vm::RootAcceptor &acceptor) {
      hermesValues.forEach(
        [&acceptor](auto &element) { acceptor.accept(element.value()); }
      );
    });
    rt->addCustomWeakRootsFunction(
      [this](vm::GC *, vm::WeakRootAcceptor &acceptor) {
        weakHermesValues.forEach([&acceptor](auto &element) {
          acceptor.acceptWeak(element.value());
        });
      }
    );
  }

  ~HermesABIRuntime() {
    rt.reset();
    assert(hermesValues.sizeForTests() == 0 && "Dangling references.");
    assert(weakHermesValues.sizeForTests() == 0 && "Dangling references.");
  }

  HermesABIValue createValue(vm::HermesValue hv) {
    switch (hv.getETag()) {
      case vm::HermesValue::ETag::Undefined:
        return abi::createUndefinedValue();
      case vm::HermesValue::ETag::Null:
        return abi::createNullValue();
      case vm::HermesValue::ETag::Bool:
        return abi::createBoolValue(hv.getBool());
      case vm::HermesValue::ETag::Symbol:
        return abi::createSymbolValue(&hermesValues.add(hv));
      case vm::HermesValue::ETag::Str1:
      case vm::HermesValue::ETag::Str2:
        return abi::createStringValue(&hermesValues.add(hv));
      case vm::HermesValue::ETag::BigInt1:
      case vm::HermesValue::ETag::BigInt2:
        return abi::createBigIntValue(&hermesValues.add(hv));
      case vm::HermesValue::ETag::Object1:
      case vm::HermesValue::ETag::Object2:
        return abi::createObjectValue(&hermesValues.add(hv));
      default:
        assert(hv.isNumber() && "No other types are permitted in the API.");
        return abi::createNumberValue(hv.getNumber());
    }
  }

  HermesABIValueOrError createValueOrError(vm::HermesValue hv) {
    return abi::createValueOrError(createValue(hv));
  }

  template <typename T>
  HermesABIManagedPointer *createPointerImpl(vm::HermesValue hv) {
    if constexpr (!std::is_same_v<T, HermesABIWeakObject>)
      return &hermesValues.add(hv);

    return &weakHermesValues.add(
      vm::WeakRoot<vm::JSObject>(vm::vmcast<vm::JSObject>(hv), *rt)
    );
  }

#define DECLARE_HERMES_ABI_POINTER_HELPERS(name)     \
  HermesABI##name create##name(vm::HermesValue hv) { \
    return abi::create##name(                        \
      createPointerImpl<HermesABI##name>(hv)         \
    );                                               \
  }                                                  \
  HermesABI##name##OrError create##name##OrError(    \
    vm::HermesValue hv                               \
  ) {                                                \
    return abi::create##name##OrError(               \
      createPointerImpl<HermesABI##name>(hv)         \
    );                                               \
  }
  HERMES_ABI_POINTER_TYPES(DECLARE_HERMES_ABI_POINTER_HELPERS)
#undef DECLARE_HERMES_ABI_POINTER_HELPERS

  vm::Handle<> makeVMHandle(
    const HermesABIValue &val,
    vm::PinnedHermesValue *numberStorage
  ) {
    switch (abi::getValueKind(val)) {
      case HermesABIValueKindUndefined:
        return vm::Runtime::getUndefinedValue();
      case HermesABIValueKindNull:
        return vm::Runtime::getNullValue();
      case HermesABIValueKindBoolean:
        return vm::Runtime::getBoolValue(abi::getBoolValue(val));
      case HermesABIValueKindNumber:
        *numberStorage = vm::HermesValue::encodeUntrustedNumberValue(
          abi::getNumberValue(val)
        );
        return vm::Handle<>(numberStorage);
      case HermesABIValueKindString:
      case HermesABIValueKindObject:
      case HermesABIValueKindSymbol:
      case HermesABIValueKindBigInt:
        return toHandle<>(val.data.pointer);
      default:
        hermes_fatal("Value has an unexpected tag.");
    }
  }

  vm::ExecutionStatus raiseError(HermesABIErrorCode err) {
    if (err == HermesABIErrorCodeJSError)
      return vm::ExecutionStatus::EXCEPTION;

    if (err == HermesABIErrorCodeNativeException) {
      auto msg = std::exchange(nativeExceptionMessage, {});
      llvh::SmallVector<llvh::UTF16, 8> u16msg;
      if (!llvh::convertUTF8ToUTF16String(msg, u16msg))
        return rt->raiseError("<invalid utf-8 exception message>");

      static_assert(
        sizeof(llvh::UTF16) == sizeof(char16_t),
        "Cannot safely cast UTF16 to char16_t."
      );
      return rt->raiseError(
        vm::UTF16Ref{(char16_t *)u16msg.data(), u16msg.size()}
      );
    }

    return rt->raiseError("<unknown native exception>");
  }
};

class NonOwningBuffer : public hermes::Buffer {
 public:
  NonOwningBuffer(const uint8_t *data, size_t size)
      : hermes::Buffer(data, size) {}
};

struct HermesABIPreparedJavaScript {
  std::shared_ptr<hbc::BCProvider> provider;
  std::string sourceURL;
};

struct PointerRegistryEntry {
  HermesABIRuntime *rt;
  size_t ref_count;
};

static std::mutex g_registry_mutex;
static std::unordered_map<HermesABIManagedPointer *, PointerRegistryEntry>
  g_pointer_registry;

extern "C" {

void hermes_register_pointer(
  HermesABIRuntime *rt, HermesABIManagedPointer *ptr
) {
  if (rt && ptr) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto &entry = g_pointer_registry[ptr];
    entry.rt = rt;
    entry.ref_count++;
  }
}

void hermes_pointer_release(HermesABIManagedPointer *ptr) {
  if (ptr && ptr->vtable && ptr->vtable->invalidate) {
    ptr->vtable->invalidate(ptr);
  }
}

void hermes_pointer_release_safe(HermesABIManagedPointer *ptr) {
  if (!ptr) return;
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  auto it = g_pointer_registry.find(ptr);
  if (it != g_pointer_registry.end()) {
    hermes_pointer_release(ptr);
    if (--it->second.ref_count == 0) {
      g_pointer_registry.erase(it);
    }
  }
}

class HostFunctionWrapper {
  void *user_data_;
  HermesABIRuntime *rt_;
  HermesABIHostFunctionCall call_cb_;
  HermesABIHostFunctionRelease release_cb_;

 public:
  HostFunctionWrapper(
    void *user_data,
    HermesABIRuntime *rt,
    HermesABIHostFunctionCall call_cb,
    HermesABIHostFunctionRelease release_cb
  )
      : user_data_(user_data),
        rt_(rt),
        call_cb_(call_cb),
        release_cb_(release_cb) {}

  ~HostFunctionWrapper() {
    if (release_cb_ && (!rt_ || !rt_->is_releasing)) {
      release_cb_(user_data_);
    }
  }

  static vm::CallResult<vm::HermesValue> call(
    void *hfCtx,
    vm::Runtime &runtime
  ) {
    vm::NativeArgs hvArgs = runtime.getCurrentFrame().getNativeArgs();
    auto *self = static_cast<HostFunctionWrapper *>(hfCtx);
    auto *rt = self->rt_;

    llvh::SmallVector<HermesABIValue, 8> apiArgs;
    for (vm::HermesValue hv : hvArgs)
      apiArgs.push_back(rt->createValue(hv));

    HermesABIValue thisArg = rt->createValue(hvArgs.getThisArg());

    auto retOrError = self->call_cb_(
      self->user_data_, rt, &thisArg, apiArgs.data(), apiArgs.size()
    );

    for (const auto &arg : apiArgs)
      abi::releaseValue(arg);
    abi::releaseValue(thisArg);

    if (abi::isError(retOrError))
      return rt->raiseError(abi::getError(retOrError));

    auto ret = abi::getValue(retOrError);
    auto retHV = toHermesValue(ret);
    abi::releaseValue(ret);
    return retHV;
  }

  static void release(void *data) {
    delete static_cast<HostFunctionWrapper *>(data);
  }
};

class HostObjectWrapper : public vm::HostObjectProxy {
  void *user_data_;
  HermesABIRuntime *rt_;
  HermesABIHostObjectGet get_cb_;
  HermesABIHostObjectSet set_cb_;
  HermesABIHostObjectGetOwnKeys get_own_keys_cb_;
  HermesABIHostObjectRelease release_cb_;

 public:
  HostObjectWrapper(
    void *user_data,
    HermesABIRuntime *rt,
    HermesABIHostObjectGet get_cb,
    HermesABIHostObjectSet set_cb,
    HermesABIHostObjectGetOwnKeys get_own_keys_cb,
    HermesABIHostObjectRelease release_cb
  )
      : user_data_(user_data),
        rt_(rt),
        get_cb_(get_cb),
        set_cb_(set_cb),
        get_own_keys_cb_(get_own_keys_cb),
        release_cb_(release_cb) {}

  ~HostObjectWrapper() override {
    if (release_cb_ && (!rt_ || !rt_->is_releasing)) {
      release_cb_(user_data_);
    }
  }

  vm::CallResult<vm::HermesValue> get(vm::SymbolID sym) override {
    if (!get_cb_)
      return vm::HermesValue::encodeUndefinedValue();
    HermesABIPropNameID name =
      rt_->createPropNameID(vm::HermesValue::encodeSymbolValue(sym));
    auto retOrErr = get_cb_(user_data_, rt_, name);
    abi::releasePointer(name.pointer);

    if (abi::isError(retOrErr))
      return rt_->raiseError(abi::getError(retOrErr));

    auto ret = abi::getValue(retOrErr);
    auto retHV = toHermesValue(ret);
    abi::releaseValue(ret);
    return retHV;
  }

  vm::CallResult<bool> set(vm::SymbolID sym, vm::HermesValue value) override {
    if (!set_cb_)
      return false;
    HermesABIPropNameID name =
      rt_->createPropNameID(vm::HermesValue::encodeSymbolValue(sym));
    auto abiVal = rt_->createValue(value);
    auto ret = set_cb_(user_data_, rt_, name, &abiVal);
    abi::releasePointer(name.pointer);
    abi::releaseValue(abiVal);
    if (abi::isError(ret))
      return rt_->raiseError(abi::getError(ret));
    return true;
  }

  vm::ExecutionStatus getHostPropertyNames(
    vm::MutableHandle<vm::JSArray> result
  ) override {
    if (!get_own_keys_cb_)
      return vm::ExecutionStatus::RETURNED;
    auto ret = get_own_keys_cb_(user_data_, rt_);
    if (abi::isError(ret))
      return rt_->raiseError(abi::getError(ret));

    auto *abiNames = abi::getPropNameIDListPtr(ret);
    const HermesABIPropNameID *names = abiNames->props;
    size_t size = abiNames->size;
    auto &runtime = *rt_->rt;
    auto arrayRes = vm::JSArray::create(runtime, size, size);
    if (arrayRes == vm::ExecutionStatus::EXCEPTION) {
      abiNames->vtable->release(abiNames);
      return vm::ExecutionStatus::EXCEPTION;
    }

    result = std::move(*arrayRes);
    vm::JSArray::setStorageEndIndex(result, runtime, size);
    for (size_t i = 0; i < size; ++i) {
      auto shv = vm::SmallHermesValue::encodeSymbolValue(*toHandle(names[i]));
      vm::JSArray::unsafeSetExistingElementAt(*result, runtime, i, shv);
    }
    abiNames->vtable->release(abiNames);
    return vm::ExecutionStatus::RETURNED;
  }
};

struct CustomNativeState {
  HermesABIRuntime *rt;
  void *user_data;
  HermesABINativeStateRelease release_cb;
};

HermesABIRuntime *hermes_runtime_create(
  HermesABIRuntimeConfig config
) {
  hermes::vm::RuntimeConfig::Builder builder;
  builder.withEnableEval(config.enable_eval);
  builder.withES6Proxy(config.es6_proxy);
  builder.withEnableGenerator(config.enable_generator);
  builder.withEnableAsyncGenerators(config.enable_async_generators);
  builder.withES6BlockScoping(config.es6_block_scoping);
  builder.withIntl(config.intl);
  builder.withMicrotaskQueue(config.microtask_queue);

  if (config.compilation_mode == 0) {
    builder.withCompilationMode(
      hermes::vm::CompilationMode::SmartCompilation
    );
  } else if (config.compilation_mode == 1) {
    builder.withCompilationMode(
      hermes::vm::CompilationMode::ForceEagerCompilation
    );
  } else if (config.compilation_mode == 2) {
    builder.withCompilationMode(
      hermes::vm::CompilationMode::ForceLazyCompilation
    );
  }

  builder.withBytecodeWarmupPercent(config.bytecode_warmup_percent);
  builder.withOptimizedEval(config.optimized_eval);
  builder.withEnableHermesInternal(config.enable_hermes_internal);
  builder.withEnableHermesInternalTestMethods(
    config.enable_hermes_internal_test_methods
  );
  builder.withRandomizeMemoryLayout(config.randomize_memory_layout);

  return new HermesABIRuntime(builder.build());
}

void hermes_runtime_release(HermesABIRuntime *rt) {
  if (!rt) return;
  {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    for (auto it = g_pointer_registry.begin();
         it != g_pointer_registry.end();) {
      if (it->second.rt == rt) {
        it = g_pointer_registry.erase(it);
      } else {
        ++it;
      }
    }
  }
  delete rt;
}

void hermes_runtime_release_from_finalizer(HermesABIRuntime *rt) {
  if (!rt) return;
  rt->is_releasing = true;
  hermes_runtime_release(rt);
}

HermesABIObject hermes_runtime_get_global_object(HermesABIRuntime *rt) {
  return rt->createObject(rt->rt->getGlobal().getHermesValue());
}

HermesABIValue hermes_evaluate_javascript(
  HermesABIRuntime *rt,
  const uint8_t *script,
  size_t script_len,
  const char *source_url
) {
  llvh::StringRef sourceURLRef(source_url ? source_url : "");
  auto bcErr = hbc::createBCProviderFromSrc(
    std::make_unique<NonOwningBuffer>(script, script_len),
    sourceURLRef,
    /* sourceMap */ {},
    /* compileFlags */ rt->compileFlags
  );
  if (!bcErr.first) {
    rt->nativeExceptionMessage = std::move(bcErr.second);
    return {
      HermesABIValueKindError,
      {.error = HermesABIErrorCodeNativeException}
    };
  }

  auto &runtime = *rt->rt;
  vm::RuntimeModuleFlags runtimeFlags{};
  vm::GCScope gcScope(runtime);
  auto res = runtime.runBytecode(
    std::move(bcErr.first),
    runtimeFlags,
    sourceURLRef,
    vm::Runtime::makeNullHandle<vm::Environment>()
  );
  if (res == vm::ExecutionStatus::EXCEPTION) {
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};
  }
  return rt->createValue(*res);
}

HermesABIValue hermes_evaluate_bytecode(
  HermesABIRuntime *rt,
  const uint8_t *data,
  size_t size,
  const char *source_url
) {
  llvh::StringRef sourceURLRef(source_url ? source_url : "");
  auto bcErr = hbc::BCProviderFromBuffer::createBCProviderFromBuffer(
    std::make_unique<NonOwningBuffer>(data, size)
  );
  if (!bcErr.first) {
    rt->nativeExceptionMessage = std::move(bcErr.second);
    return {
      HermesABIValueKindError,
      {.error = HermesABIErrorCodeNativeException}
    };
  }

  auto &runtime = *rt->rt;
  vm::RuntimeModuleFlags runtimeFlags{};
  vm::GCScope gcScope(runtime);
  auto res = runtime.runBytecode(
    std::move(bcErr.first),
    runtimeFlags,
    sourceURLRef,
    vm::Runtime::makeNullHandle<vm::Environment>()
  );
  if (res == vm::ExecutionStatus::EXCEPTION) {
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};
  }
  return rt->createValue(*res);
}

void hermes_runtime_set_js_error_value(
  HermesABIRuntime *rt, HermesABIValue error_value
) {
  rt->rt->setThrownValue(toHermesValue(error_value));
}

void hermes_runtime_set_native_exception_message(
  HermesABIRuntime *rt, const char *message
) {
  rt->nativeExceptionMessage = message ? message : "";
}

HermesABIValue hermes_runtime_get_and_clear_js_error_value(
  HermesABIRuntime *rt
) {
  auto thrownValue = rt->rt->getThrownValue();
  auto ret = thrownValue.isEmpty() ? abi::createUndefinedValue()
                                   : rt->createValue(thrownValue);
  rt->rt->clearThrownValue();
  return ret;
}

char *hermes_runtime_get_and_clear_native_exception_message(
  HermesABIRuntime *rt
) {
  if (rt->nativeExceptionMessage.empty()) return nullptr;
  char *res = strdup(rt->nativeExceptionMessage.c_str());
  rt->nativeExceptionMessage.clear();
  rt->nativeExceptionMessage.shrink_to_fit();
  return res;
}

HermesABIObjectOrError hermes_object_create(HermesABIRuntime *rt) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  return rt->createObjectOrError(
    vm::JSObject::create(runtime).getHermesValue()
  );
}

HermesABIObject hermes_object_clone(
  HermesABIRuntime *rt, HermesABIObject obj
) {
  if (!obj.pointer) return {nullptr};
  static_cast<ManagedValue<vm::PinnedHermesValue> *>(obj.pointer)->inc();
  return obj;
}

HermesABIValue hermes_object_get_property_from_value(
  HermesABIRuntime *rt, HermesABIObject obj, HermesABIValue key
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::PinnedHermesValue numberStorage;
  auto res = vm::JSObject::getComputed_RJS(
    toHandle(obj), runtime, rt->makeVMHandle(key, &numberStorage)
  );
  if (res == vm::ExecutionStatus::EXCEPTION) {
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};
  }
  return rt->createValue(res->get());
}

HermesABIVoidOrError hermes_object_set_property_from_value(
  HermesABIRuntime *rt,
  HermesABIObject obj,
  HermesABIValue key,
  HermesABIValue value
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::PinnedHermesValue numberStorageKey;
  vm::PinnedHermesValue numberStorageVal;
  auto res = vm::JSObject::putComputed_RJS(
               toHandle(obj),
               runtime,
               rt->makeVMHandle(key, &numberStorageKey),
               rt->makeVMHandle(value, &numberStorageVal),
               vm::PropOpFlags().plusThrowOnError()
  )
               .getStatus();
  if (res == vm::ExecutionStatus::EXCEPTION)
    return abi::createVoidOrError(HermesABIErrorCodeJSError);
  return abi::createVoidOrError();
}

HermesABIBoolOrError hermes_object_has_property_from_value(
  HermesABIRuntime *rt, HermesABIObject obj, HermesABIValue key
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::PinnedHermesValue numberStorage;
  auto res = vm::JSObject::hasComputed(
    toHandle(obj), runtime, rt->makeVMHandle(key, &numberStorage)
  );
  if (res == vm::ExecutionStatus::EXCEPTION)
    return abi::createBoolOrError(HermesABIErrorCodeJSError);
  return abi::createBoolOrError(*res);
}

HermesABIValue hermes_object_get_property_from_propnameid(
  HermesABIRuntime *rt, HermesABIObject obj, HermesABIPropNameID name
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto res = vm::JSObject::getNamedOrIndexed(
    toHandle(obj), runtime, *toHandle(name)
  );
  if (res == vm::ExecutionStatus::EXCEPTION)
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};
  return rt->createValue(res->get());
}

HermesABIVoidOrError hermes_object_set_property_from_propnameid(
  HermesABIRuntime *rt,
  HermesABIObject obj,
  HermesABIPropNameID name,
  HermesABIValue value
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::PinnedHermesValue numberStorage;
  auto res = vm::JSObject::putNamedOrIndexed(
               toHandle(obj),
               runtime,
               *toHandle(name),
               rt->makeVMHandle(value, &numberStorage),
               vm::PropOpFlags().plusThrowOnError()
  )
               .getStatus();
  if (res == vm::ExecutionStatus::EXCEPTION)
    return abi::createVoidOrError(HermesABIErrorCodeJSError);
  return abi::createVoidOrError();
}

HermesABIBoolOrError hermes_object_has_property_from_propnameid(
  HermesABIRuntime *rt, HermesABIObject obj, HermesABIPropNameID name
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto res =
    vm::JSObject::hasNamedOrIndexed(toHandle(obj), runtime, *toHandle(name));
  if (res == vm::ExecutionStatus::EXCEPTION)
    return abi::createBoolOrError(HermesABIErrorCodeJSError);
  return abi::createBoolOrError(*res);
}

HermesABIArrayOrError hermes_object_get_property_names(
  HermesABIRuntime *rt, HermesABIObject obj
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  uint32_t beginIndex;
  uint32_t endIndex;
  auto objHandle = toHandle(obj);

  auto propsRes =
    vm::getForInPropertyNames(runtime, objHandle, beginIndex, endIndex);
  if (propsRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createArrayOrError(HermesABIErrorCodeJSError);

  vm::Handle<vm::ArrayStorageSmall> props = *propsRes;
  size_t length = endIndex - beginIndex;

  auto retRes = vm::JSArray::create(runtime, length, length);
  if (retRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createArrayOrError(HermesABIErrorCodeJSError);

  struct : public vm::Locals {
    vm::PinnedValue<vm::JSArray> ret;
    vm::PinnedValue<> nameHnd;
  } lv;
  vm::LocalsRAII lraii(runtime, &lv);

  lv.ret.castAndSetHermesValue<vm::JSArray>(retRes->getHermesValue());
  vm::JSArray::setStorageEndIndex(lv.ret, runtime, length);

  for (size_t i = 0; i < length; ++i) {
    vm::SmallHermesValue name = props->at(beginIndex + i);
    if (name.isString()) {
      vm::JSArray::unsafeSetExistingElementAt(*lv.ret, runtime, i, name);
    } else if (name.isSymbol()) {
      vm::StringPrimitive *asString =
        runtime.getStringPrimFromSymbolID(name.getSymbol());
      auto strName =
        vm::SmallHermesValue::encodeStringValue(asString, runtime);
      vm::JSArray::unsafeSetExistingElementAt(*lv.ret, runtime, i, strName);
    } else {
      assert(name.isNumber());
      lv.nameHnd =
        vm::HermesValue::encodeTrustedNumberValue(name.getNumber(runtime));
      auto asStrRes = vm::toString_RJS(runtime, lv.nameHnd);
      if (asStrRes == vm::ExecutionStatus::EXCEPTION)
        return abi::createArrayOrError(HermesABIErrorCodeJSError);
      auto strName =
        vm::SmallHermesValue::encodeStringValue(asStrRes->get(), runtime);
      vm::JSArray::unsafeSetExistingElementAt(*lv.ret, runtime, i, strName);
    }
  }

  return rt->createArrayOrError(lv.ret.getHermesValue());
}

bool hermes_object_strict_equals(
  HermesABIRuntime *rt, HermesABIObject a, HermesABIObject b
) {
  return toHandle(a) == toHandle(b);
}

bool hermes_object_is_function(
  HermesABIRuntime *rt, HermesABIObject obj
) {
  return vm::vmisa<vm::Callable>(toHandle(obj).getHermesValue());
}

bool hermes_object_is_array(
  HermesABIRuntime *rt, HermesABIObject obj
) {
  return vm::vmisa<vm::JSArray>(toHandle(obj).getHermesValue());
}

bool hermes_object_is_arraybuffer(
  HermesABIRuntime *rt, HermesABIObject obj
) {
  return vm::vmisa<vm::JSArrayBuffer>(toHandle(obj).getHermesValue());
}

HermesABIBoolOrError hermes_instance_of(
  HermesABIRuntime *rt, HermesABIObject obj, HermesABIFunction ctor
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto res = vm::instanceOfOperator_RJS(
    runtime, toHandle(obj), toHandle(ctor)
  );
  if (res == vm::ExecutionStatus::EXCEPTION)
    return abi::createBoolOrError(HermesABIErrorCodeJSError);
  return abi::createBoolOrError(*res);
}

HermesABIObjectOrError hermes_object_create_from_host_object(
  HermesABIRuntime *rt,
  void *user_data,
  HermesABIHostObjectGet get_cb,
  HermesABIHostObjectSet set_cb,
  HermesABIHostObjectGetOwnKeys get_own_keys_cb,
  HermesABIHostObjectRelease release_cb
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto objRes = vm::HostObject::createWithoutPrototype(
    runtime,
    std::make_unique<HostObjectWrapper>(
      user_data, rt, get_cb, set_cb, get_own_keys_cb, release_cb
    )
  );
  if (objRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createObjectOrError(HermesABIErrorCodeJSError);
  return rt->createObjectOrError(*objRes);
}

HermesABIVoidOrError hermes_object_set_native_state(
  HermesABIRuntime *rt,
  HermesABIObject obj,
  void *user_data,
  HermesABINativeStateRelease release_cb
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);

  struct : public vm::Locals {
    vm::PinnedValue<vm::NativeState> ns;
  } lv;
  vm::LocalsRAII lraii(runtime, &lv);

  auto finalize = [](vm::GC &, vm::NativeState *ns) {
    auto *self = static_cast<CustomNativeState *>(ns->context());
    if (self) {
      if (self->release_cb && (!self->rt || !self->rt->is_releasing)) {
        self->release_cb(self->user_data);
      }
      delete self;
    }
  };

  auto *state = new CustomNativeState{rt, user_data, release_cb};
  lv.ns = vm::NativeState::create(runtime, state, finalize);

  auto h = toHandle(obj);
  if (h->isProxyObject()) {
    rt->nativeExceptionMessage = "Native state is unsupported on Proxy";
    return abi::createVoidOrError(HermesABIErrorCodeNativeException);
  } else if (h->isHostObject()) {
    rt->nativeExceptionMessage = "Native state is unsupported on HostObject";
    return abi::createVoidOrError(HermesABIErrorCodeNativeException);
  }

  auto res = vm::JSObject::defineOwnProperty(
    h,
    runtime,
    vm::Predefined::getSymbolID(vm::Predefined::InternalPropertyNativeState),
    vm::DefinePropertyFlags::getDefaultNewPropertyFlags(),
    lv.ns
  );
  if (res == vm::ExecutionStatus::EXCEPTION) {
    return abi::createVoidOrError(HermesABIErrorCodeJSError);
  }
  if (!*res) {
    rt->nativeExceptionMessage = "Failed to set native state.";
    return abi::createVoidOrError(HermesABIErrorCodeNativeException);
  }
  return abi::createVoidOrError();
}

void *hermes_object_get_native_state_data(
  HermesABIRuntime *rt, HermesABIObject obj
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto h = toHandle(obj);

  if (h->isProxyObject() || h->isHostObject())
    return nullptr;

  vm::NamedPropertyDescriptor desc;
  bool exists = vm::JSObject::getOwnNamedDescriptor(
    h,
    runtime,
    vm::Predefined::getSymbolID(vm::Predefined::InternalPropertyNativeState),
    desc
  );

  if (!exists)
    return nullptr;

  vm::NoAllocScope scope(runtime);
  vm::NativeState *ns = vm::vmcast<vm::NativeState>(
    vm::JSObject::getNamedSlotValueUnsafe(*h, runtime, desc)
      .getObject(runtime)
  );
  assert(ns->context() && "State cannot be null.");
  auto *state = static_cast<CustomNativeState *>(ns->context());
  return state ? state->user_data : nullptr;
}

HermesABIValue hermes_function_call(
  HermesABIRuntime *rt,
  HermesABIFunction fn,
  HermesABIValue js_this,
  HermesABIValue *args,
  size_t arg_count
) {
  if (arg_count > std::numeric_limits<uint32_t>::max()) {
    rt->nativeExceptionMessage = "Too many arguments to call";
    return {
      HermesABIValueKindError,
      {.error = HermesABIErrorCodeNativeException}
    };
  }

  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::Handle<vm::Callable> funcHandle = toHandle(fn);

  vm::ScopedNativeCallFrame newFrame{
    runtime,
    static_cast<uint32_t>(arg_count),
    funcHandle.getHermesValue(),
    vm::HermesValue::encodeUndefinedValue(),
    toHermesValue(js_this)
  };
  if (LLVM_UNLIKELY(newFrame.overflowed())) {
    (void)runtime.raiseStackOverflow(
      ::hermes::vm::Runtime::StackOverflowKind::NativeStack
    );
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};
  }

  for (uint32_t i = 0; i != arg_count; ++i)
    newFrame->getArgRef(i) = toHermesValue(args[i]);

  auto callRes = vm::Callable::call(funcHandle, runtime);
  if (callRes == vm::ExecutionStatus::EXCEPTION)
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};

  return rt->createValue(callRes->get());
}

HermesABIValue hermes_function_call_as_constructor(
  HermesABIRuntime *rt,
  HermesABIFunction fn,
  HermesABIValue *args,
  size_t arg_count
) {
  if (arg_count > std::numeric_limits<uint32_t>::max()) {
    rt->nativeExceptionMessage = "Too many arguments to call";
    return {
      HermesABIValueKindError,
      {.error = HermesABIErrorCodeNativeException}
    };
  }

  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);

  struct : public vm::Locals {
    vm::PinnedValue<vm::JSObject> objHandle;
  } lv;
  vm::LocalsRAII lraii(runtime, &lv);

  vm::Handle<vm::Callable> funcHandle = toHandle(fn);

  auto thisRes =
    vm::Callable::createThisForConstruct_RJS(funcHandle, runtime, funcHandle);
  lv.objHandle.castAndSetHermesValue<vm::JSObject>(thisRes->getHermesValue());

  vm::ScopedNativeCallFrame newFrame{
    runtime,
    static_cast<uint32_t>(arg_count),
    funcHandle.getHermesValue(),
    funcHandle.getHermesValue(),
    lv.objHandle.getHermesValue()
  };
  if (LLVM_UNLIKELY(newFrame.overflowed())) {
    (void)runtime.raiseStackOverflow(
      ::hermes::vm::Runtime::StackOverflowKind::NativeStack
    );
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};
  }

  for (uint32_t i = 0; i != arg_count; ++i)
    newFrame->getArgRef(i) = toHermesValue(args[i]);

  auto callRes = vm::Callable::call(funcHandle, runtime);
  if (callRes == vm::ExecutionStatus::EXCEPTION)
    return {HermesABIValueKindError, {.error = HermesABIErrorCodeJSError}};

  auto res = callRes->get();
  return rt->createValue(
    res.isObject() ? res : lv.objHandle.getHermesValue()
  );
}

HermesABIFunctionOrError hermes_function_create_from_host(
  HermesABIRuntime *rt,
  HermesABIPropNameID name,
  unsigned int length,
  void *user_data,
  HermesABIHostFunctionCall call_cb,
  HermesABIHostFunctionRelease release_cb
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto *hfw = new HostFunctionWrapper(user_data, rt, call_cb, release_cb);

  auto funcRes = vm::FinalizableNativeFunction::createWithoutPrototype(
    runtime,
    hfw,
    HostFunctionWrapper::call,
    HostFunctionWrapper::release,
    *toHandle(name),
    length
  );
  if (funcRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createFunctionOrError(HermesABIErrorCodeJSError);
  return rt->createFunctionOrError(*funcRes);
}

HermesABIStringOrError hermes_create_string_from_utf8(
  HermesABIRuntime *rt,
  const char *str
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  size_t length = str ? strlen(str) : 0;
  auto strRes = vm::StringPrimitive::createEfficient(
    runtime,
    llvh::makeArrayRef(
      reinterpret_cast<const uint8_t *>(str ? str : ""), length
    ),
    /* IgnoreInputErrors */ true
  );
  if (strRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createStringOrError(HermesABIErrorCodeJSError);
  return rt->createStringOrError(*strRes);
}

char *hermes_string_to_utf8(
  HermesABIRuntime *rt,
  HermesABIString str
) {
  if (!str.pointer) return nullptr;
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto view = vm::StringPrimitive::createStringView(runtime, toHandle(str));

  std::string utf8;
  if (LLVM_LIKELY(view.isASCII())) {
    utf8.assign(view.castToCharPtr(), view.length());
  } else {
    hermes::convertUTF16ToUTF8WithReplacements(
      utf8, {view.castToChar16Ptr(), view.length()}
    );
  }

  char *res = static_cast<char *>(malloc(utf8.size() + 1));
  if (res) {
    memcpy(res, utf8.data(), utf8.size());
    res[utf8.size()] = '\0';
  }
  return res;
}

HermesABIString hermes_string_clone(
  HermesABIRuntime *rt,
  HermesABIString str
) {
  if (!str.pointer) return {nullptr};
  static_cast<ManagedValue<vm::PinnedHermesValue> *>(str.pointer)->inc();
  return str;
}

bool hermes_string_strict_equals(
  HermesABIRuntime *rt,
  HermesABIString a,
  HermesABIString b
) {
  return toHandle(a)->equals(*toHandle(b));
}

char *hermes_symbol_to_utf8(
  HermesABIRuntime *rt,
  HermesABISymbol sym
) {
  if (!sym.pointer) return nullptr;
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto view =
    runtime.getIdentifierTable().getStringView(runtime, *toHandle(sym));

  std::string utf8 = "Symbol(";
  if (LLVM_LIKELY(view.isASCII())) {
    utf8.append(view.castToCharPtr(), view.length());
  } else {
    std::string cvtBuf;
    hermes::convertUTF16ToUTF8WithReplacements(
      cvtBuf, {view.castToChar16Ptr(), view.length()}
    );
    utf8.append(cvtBuf);
  }
  utf8.push_back(')');

  char *res = static_cast<char *>(malloc(utf8.size() + 1));
  if (res) {
    memcpy(res, utf8.data(), utf8.size());
    res[utf8.size()] = '\0';
  }
  return res;
}

}  // extern "C"

extern "C" {

HermesABISymbol hermes_symbol_clone(
  HermesABIRuntime *rt,
  HermesABISymbol sym
) {
  if (!sym.pointer) return {nullptr};
  static_cast<ManagedValue<vm::PinnedHermesValue> *>(sym.pointer)->inc();
  return sym;
}

bool hermes_symbol_strict_equals(
  HermesABIRuntime *rt,
  HermesABISymbol a,
  HermesABISymbol b
) {
  return toHandle(a) == toHandle(b);
}

HermesABIPropNameIDOrError hermes_propnameid_create_from_string(
  HermesABIRuntime *rt,
  HermesABIString str
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto cr =
    vm::stringToSymbolID(runtime, vm::createPseudoHandle(*toHandle(str)));
  if (cr == vm::ExecutionStatus::EXCEPTION)
    return abi::createPropNameIDOrError(HermesABIErrorCodeJSError);
  return rt->createPropNameIDOrError(cr->getHermesValue());
}

HermesABIPropNameIDOrError hermes_propnameid_create_from_symbol(
  HermesABIRuntime *rt,
  HermesABISymbol sym
) {
  return rt->createPropNameIDOrError(toHandle(sym).getHermesValue());
}

char *hermes_propnameid_to_utf8(
  HermesABIRuntime *rt,
  HermesABIPropNameID name
) {
  if (!name.pointer) return nullptr;
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto view =
    runtime.getIdentifierTable().getStringView(runtime, *toHandle(name));

  std::string utf8;
  if (LLVM_LIKELY(view.isASCII())) {
    utf8.assign(view.castToCharPtr(), view.length());
  } else {
    hermes::convertUTF16ToUTF8WithReplacements(
      utf8, {view.castToChar16Ptr(), view.length()}
    );
  }

  char *res = static_cast<char *>(malloc(utf8.size() + 1));
  if (res) {
    memcpy(res, utf8.data(), utf8.size());
    res[utf8.size()] = '\0';
  }
  return res;
}

HermesABIPropNameID hermes_propnameid_clone(
  HermesABIRuntime *rt,
  HermesABIPropNameID name
) {
  if (!name.pointer) return {nullptr};
  static_cast<ManagedValue<vm::PinnedHermesValue> *>(name.pointer)->inc();
  return name;
}

bool hermes_propnameid_equals(
  HermesABIRuntime *rt,
  HermesABIPropNameID a,
  HermesABIPropNameID b
) {
  if (!a.pointer || !b.pointer) return false;
  return *toHandle(a) == *toHandle(b);
}

static void release_propnameid_list(HermesABIPropNameIDList *self) {
  if (self) {
    if (self->props) {
      delete[] self->props;
    }
    delete self;
  }
}

static const HermesABIPropNameIDListVTable g_propnameid_list_vtable = {
  release_propnameid_list
};

HermesABIPropNameIDList *hermes_propnameid_list_create(
  const HermesABIPropNameID *props,
  size_t size
) {
  auto *list = new HermesABIPropNameIDList;
  list->vtable = &g_propnameid_list_vtable;
  list->size = size;
  if (size > 0) {
    auto *new_props = new HermesABIPropNameID[size];
    for (size_t i = 0; i < size; ++i) {
      new_props[i] = props[i];
    }
    list->props = new_props;
  } else {
    list->props = nullptr;
  }
  return list;
}

HermesABIBigIntOrError hermes_bigint_create_from_int64(
  HermesABIRuntime *rt,
  int64_t value
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto biRes = vm::BigIntPrimitive::fromSigned(runtime, value);
  if (biRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createBigIntOrError(HermesABIErrorCodeJSError);
  return rt->createBigIntOrError(*biRes);
}

bool hermes_bigint_is_int64(
  HermesABIRuntime *rt,
  HermesABIBigInt bi
) {
  return toHandle(bi)->isTruncationToSingleDigitLossless(
    /* signedTruncation */ true
  );
}

int64_t hermes_bigint_as_int64(
  HermesABIRuntime *rt,
  HermesABIBigInt bi
) {
  return static_cast<int64_t>(toHandle(bi)->truncateToSingleDigit());
}

HermesABIStringOrError hermes_bigint_to_string(
  HermesABIRuntime *rt,
  HermesABIBigInt bi,
  int radix
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto strRes = vm::BigIntPrimitive::toString(
    runtime, vm::createPseudoHandle(*toHandle(bi)), radix
  );
  if (strRes == vm::ExecutionStatus::EXCEPTION)
    return abi::createStringOrError(HermesABIErrorCodeJSError);
  return rt->createStringOrError(*strRes);
}

HermesABIBigInt hermes_bigint_clone(
  HermesABIRuntime *rt,
  HermesABIBigInt bi
) {
  if (!bi.pointer) return {nullptr};
  static_cast<ManagedValue<vm::PinnedHermesValue> *>(bi.pointer)->inc();
  return bi;
}

bool hermes_bigint_strict_equals(
  HermesABIRuntime *rt,
  HermesABIBigInt a,
  HermesABIBigInt b
) {
  return toHandle(a)->compare(*toHandle(b)) == 0;
}

HermesABIArrayOrError hermes_array_create(
  HermesABIRuntime *rt,
  size_t length
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto result = vm::JSArray::create(runtime, length, length);
  if (result == vm::ExecutionStatus::EXCEPTION)
    return abi::createArrayOrError(HermesABIErrorCodeJSError);
  return rt->createArrayOrError(result->getHermesValue());
}

size_t hermes_array_get_length(
  HermesABIRuntime *rt,
  HermesABIArray arr
) {
  auto &runtime = *rt->rt;
  return vm::JSArray::getLength(*toHandle(arr), runtime);
}

HermesABIArrayBufferOrError hermes_arraybuffer_create_from_external_data(
  HermesABIRuntime *rt,
  uint8_t *data,
  size_t size,
  void *user_data,
  HermesABIMutableBufferRelease release_cb
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);

  struct : public vm::Locals {
    vm::PinnedValue<vm::JSArrayBuffer> arrayBuffer;
  } lv;
  vm::LocalsRAII lraii(runtime, &lv);

  lv.arrayBuffer = vm::JSArrayBuffer::create(
    runtime,
    vm::Handle<vm::JSObject>::vmcast(
      &runtime.arrayBufferPrototype
    )
  );

  auto deleter = [rt, release_cb, user_data](void *) {
    if (release_cb && (!rt || !rt->is_releasing)) {
      release_cb(user_data);
    }
  };

  vm::JSArrayBuffer::setExternalDataBlock(
    runtime,
    lv.arrayBuffer,
    data,
    size,
    std::shared_ptr<void>(reinterpret_cast<void *>(1), deleter)
  );

  return rt->createArrayBufferOrError(lv.arrayBuffer.getHermesValue());
}

HermesABIUint8PtrOrError hermes_arraybuffer_get_data(
  HermesABIRuntime *rt,
  HermesABIArrayBuffer ab
) {
  auto handle = toHandle(ab);
  if (!handle->attached()) {
    rt->nativeExceptionMessage =
      "Cannot get data block of detached ArrayBuffer.";
    return {true, {.error = HermesABIErrorCodeNativeException}};
  }
  return {false, {.val = handle->getDataBlock()}};
}

HermesABISizeTOrError hermes_arraybuffer_get_size(
  HermesABIRuntime *rt,
  HermesABIArrayBuffer ab
) {
  auto handle = toHandle(ab);
  if (!handle->attached()) {
    rt->nativeExceptionMessage =
      "Cannot get size of detached ArrayBuffer.";
    return {true, {.error = HermesABIErrorCodeNativeException}};
  }
  return {false, {.val = handle->size()}};
}

HermesABIWeakObjectOrError hermes_weak_object_create(
  HermesABIRuntime *rt,
  HermesABIObject obj
) {
  return rt->createWeakObjectOrError(toHandle(obj).getHermesValue());
}

HermesABIValue hermes_weak_object_lock(
  HermesABIRuntime *rt,
  HermesABIWeakObject wo
) {
  auto &runtime = *rt->rt;
  const auto &wr =
    static_cast<ManagedValue<vm::WeakRoot<vm::JSObject>> *>(wo.pointer)
      ->value();
  if (const auto ptr = wr.get(runtime, runtime.getHeap()))
    return rt->createValue(vm::HermesValue::encodeObjectValue(ptr));
  return abi::createUndefinedValue();
}

HermesABIPreparedJavaScriptOrError
hermes_runtime_prepared_javascript_create(
  HermesABIRuntime *rt,
  const uint8_t *utf8_source,
  size_t source_length,
  const char *source_url
) {
  llvh::StringRef sourceURLRef(source_url ? source_url : "");
  auto bcErr = hbc::createBCProviderFromSrc(
    std::make_unique<NonOwningBuffer>(utf8_source, source_length),
    sourceURLRef,
    /* sourceMap */ {},
    /* compileFlags */ rt->compileFlags
  );
  if (!bcErr.first) {
    rt->nativeExceptionMessage = std::move(bcErr.second);
    return {static_cast<uintptr_t>(
      (HermesABIErrorCodeNativeException << 2) | 1
    )};
  }

  auto *wrapper = new HermesABIPreparedJavaScript{
    std::move(bcErr.first), source_url ? source_url : ""
  };
  return {reinterpret_cast<uintptr_t>(wrapper)};
}

HermesABIValueOrError
hermes_runtime_prepared_javascript_evaluate(
  HermesABIRuntime *rt,
  HermesABIPreparedJavaScript *prepared
) {
  HermesABIValueOrError res;
  if (!prepared) {
    res.value = {
      HermesABIValueKindError,
      {.error = HermesABIErrorCodeNativeException}
    };
    return res;
  }
  auto &runtime = *rt->rt;
  vm::RuntimeModuleFlags runtimeFlags{};
  vm::GCScope gcScope(runtime);
  std::shared_ptr<hbc::BCProvider> providerCopy =
    prepared->provider;
  auto evalRes = runtime.runBytecode(
    std::move(providerCopy),
    runtimeFlags,
    prepared->sourceURL,
    vm::Runtime::makeNullHandle<vm::Environment>()
  );
  if (evalRes == vm::ExecutionStatus::EXCEPTION) {
    res.value = {
      HermesABIValueKindError,
      {.error = HermesABIErrorCodeJSError}
    };
    return res;
  }
  res.value = rt->createValue(*evalRes);
  return res;
}

void hermes_preparedjavascript_release(
  HermesABIPreparedJavaScript *prepared
) {
  delete prepared;
}

uint64_t hermes_value_get_unique_id(
  HermesABIRuntime *rt,
  HermesABIValue val
) {
  auto &runtime = *rt->rt;
  vm::HermesValue hv = toHermesValue(val);
  return runtime.getHeap().getSnapshotID(hv).getValueOr(0);
}

HermesABIValueOrError hermes_object_from_id(
  HermesABIRuntime *rt,
  uint64_t id
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::GCCell *ptr =
    static_cast<vm::GCCell *>(runtime.getHeap().getObjectForID(id));
  if (ptr && vm::vmisa<vm::JSObject>(ptr)) {
    return rt->createValueOrError(vm::HermesValue::encodeObjectValue(ptr));
  }
  return abi::createValueOrError(abi::createNullValue());
}

bool hermes_runtime_drain_microtasks(
  HermesABIRuntime *rt,
  int max_microtasks_hint
) {
  auto &runtime = *rt->rt;
  if (runtime.hasMicrotaskQueue()) {
    auto drainRes = runtime.drainJobs();
    if (drainRes == vm::ExecutionStatus::EXCEPTION)
      return false;
  }
  runtime.clearKeptObjects();
  return true;
}

HermesABIValueOrError hermes_value_create_from_json_utf8(
  HermesABIRuntime *rt,
  const uint8_t *json_bytes,
  size_t length
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  llvh::ArrayRef<uint8_t> ref(json_bytes, length);
  vm::CallResult<vm::HermesValue> res =
    vm::runtimeJSONParseRef(runtime, ::hermes::UTF16Stream(ref));
  if (res == vm::ExecutionStatus::EXCEPTION) {
    return abi::createValueOrError(HermesABIErrorCodeJSError);
  }
  return rt->createValueOrError(*res);
}

HermesABIStringData hermes_string_get_data(
  HermesABIRuntime *rt,
  HermesABIString str
) {
  if (!str.pointer) {
    return {false, nullptr, 0};
  }
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto view = vm::StringPrimitive::createStringView(runtime, toHandle(str));
  if (view.isASCII()) {
    return {true, view.castToCharPtr(), view.length()};
  } else {
    return {false, view.castToChar16Ptr(), view.length()};
  }
}

HermesABIStringData hermes_propnameid_get_data(
  HermesABIRuntime *rt,
  HermesABIPropNameID prop
) {
  if (!prop.pointer) {
    return {false, nullptr, 0};
  }
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  auto view =
    runtime.getIdentifierTable().getStringView(runtime, *toHandle(prop));
  if (view.isASCII()) {
    return {true, view.castToCharPtr(), view.length()};
  } else {
    return {false, view.castToChar16Ptr(), view.length()};
  }
}

HermesABIValueOrError hermes_array_value_get_at_index(
  HermesABIRuntime *rt,
  HermesABIArray arr,
  size_t index
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::PinnedHermesValue numberStorage;
  numberStorage = vm::HermesValue::encodeUntrustedNumberValue(index);
  auto res = vm::JSObject::getComputed_RJS(
    toHandle(arr), runtime, vm::Handle<>(&numberStorage)
  );
  if (res == vm::ExecutionStatus::EXCEPTION) {
    return abi::createValueOrError(HermesABIErrorCodeJSError);
  }
  return rt->createValueOrError(res->get());
}

HermesABIVoidOrError hermes_array_value_set_at_index(
  HermesABIRuntime *rt,
  HermesABIArray arr,
  size_t index,
  HermesABIValue value
) {
  auto &runtime = *rt->rt;
  vm::GCScope gcScope(runtime);
  vm::PinnedHermesValue numberStorageKey;
  numberStorageKey = vm::HermesValue::encodeUntrustedNumberValue(index);
  vm::PinnedHermesValue numberStorageVal;
  auto res = vm::JSObject::putComputed_RJS(
               toHandle(arr),
               runtime,
               vm::Handle<>(&numberStorageKey),
               rt->makeVMHandle(value, &numberStorageVal),
               vm::PropOpFlags().plusThrowOnError()
  )
               .getStatus();
  if (res == vm::ExecutionStatus::EXCEPTION)
    return abi::createVoidOrError(HermesABIErrorCodeJSError);
  return abi::createVoidOrError();
}

}  // extern "C"
