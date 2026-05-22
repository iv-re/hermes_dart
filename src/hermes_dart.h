#ifndef HERMES_DART_H
#define HERMES_DART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct HermesABIRuntime;
struct HermesABIManagedPointer;

struct HermesABIManagedPointerVTable {
  void (*invalidate)(struct HermesABIManagedPointer *self);
};

struct HermesABIManagedPointer {
  const struct HermesABIManagedPointerVTable *vtable;
};

enum HermesABIErrorCode {
  HermesABIErrorCodeNativeException,
  HermesABIErrorCodeJSError,
};

#define DECLARE_HERMES_ABI_POINTER_TYPE(name) \
  struct HermesABI##name {                    \
    struct HermesABIManagedPointer *pointer;  \
  };                                          \
  struct HermesABI##name##OrError {           \
    uintptr_t ptr_or_error;                   \
  };

DECLARE_HERMES_ABI_POINTER_TYPE(Object)
DECLARE_HERMES_ABI_POINTER_TYPE(Array)
DECLARE_HERMES_ABI_POINTER_TYPE(String)
DECLARE_HERMES_ABI_POINTER_TYPE(BigInt)
DECLARE_HERMES_ABI_POINTER_TYPE(Symbol)
DECLARE_HERMES_ABI_POINTER_TYPE(Function)
DECLARE_HERMES_ABI_POINTER_TYPE(ArrayBuffer)
DECLARE_HERMES_ABI_POINTER_TYPE(PropNameID)
DECLARE_HERMES_ABI_POINTER_TYPE(WeakObject)

#undef DECLARE_HERMES_ABI_POINTER_TYPE

struct HermesABIVoidOrError {
  uintptr_t void_or_error;
};

struct HermesABIBoolOrError {
  uintptr_t bool_or_error;
};

struct HermesABIUint8PtrOrError {
  bool is_error;
  union {
    uint8_t *val;
    uint16_t error;
  } data;
};

struct HermesABISizeTOrError {
  bool is_error;
  union {
    size_t val;
    uint16_t error;
  } data;
};

struct HermesABIPropNameIDListPtrOrError {
  uintptr_t ptr_or_error;
};

#define HERMES_ABI_POINTER_MASK (1u << (sizeof(unsigned int) * 8u - 1u))

enum HermesABIValueKind {
  HermesABIValueKindUndefined = 0,
  HermesABIValueKindNull = 1,
  HermesABIValueKindBoolean = 2,
  HermesABIValueKindError = 3,
  HermesABIValueKindNumber = 4,
  HermesABIValueKindSymbol = 5 | HERMES_ABI_POINTER_MASK,
  HermesABIValueKindBigInt = 6 | HERMES_ABI_POINTER_MASK,
  HermesABIValueKindString = 7 | HERMES_ABI_POINTER_MASK,
  HermesABIValueKindObject = 9 | HERMES_ABI_POINTER_MASK,
};

struct HermesABIValue {
  enum HermesABIValueKind kind;
  union {
    bool boolean;
    double number;
    struct HermesABIManagedPointer *pointer;
    enum HermesABIErrorCode error;
  } data;
};

struct HermesABIValueOrError {
  struct HermesABIValue value;
};

struct HermesABIPropNameIDList;

struct HermesABIPropNameIDListVTable {
  void (*release)(struct HermesABIPropNameIDList *);
};

struct HermesABIPropNameIDList {
  const struct HermesABIPropNameIDListVTable *vtable;
  const struct HermesABIPropNameID *props;
  size_t size;
};

#ifdef __cplusplus
extern "C" {
#endif

// Callback typedefs

typedef struct HermesABIValueOrError (*HermesABIHostFunctionCall)(
  void *user_data,
  struct HermesABIRuntime *rt,
  const struct HermesABIValue *this_arg,
  const struct HermesABIValue *args,
  size_t arg_count
);

typedef void (*HermesABIHostFunctionRelease)(void *user_data);

typedef struct HermesABIValueOrError (*HermesABIHostObjectGet)(
  void *user_data,
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID name
);

typedef struct HermesABIVoidOrError (*HermesABIHostObjectSet)(
  void *user_data,
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID name,
  const struct HermesABIValue *value
);

typedef struct HermesABIPropNameIDListPtrOrError (*HermesABIHostObjectGetOwnKeys)(
  void *user_data,
  struct HermesABIRuntime *rt
);

typedef void (*HermesABIHostObjectRelease)(void *user_data);

typedef void (*HermesABIMutableBufferRelease)(void *user_data);

typedef void (*HermesABINativeStateRelease)(void *user_data);

struct HermesABIRuntimeConfig {
  bool enable_eval;
  bool es6_proxy;
  bool enable_generator;
  bool enable_async_generators;
  bool es6_block_scoping;
  bool intl;
  bool microtask_queue;
  int32_t compilation_mode;
  uint32_t bytecode_warmup_percent;
  bool optimized_eval;
  bool enable_hermes_internal;
  bool enable_hermes_internal_test_methods;
  bool randomize_memory_layout;
};

struct HermesABIPreparedJavaScript;

struct HermesABIPreparedJavaScriptOrError {
  uintptr_t ptr_or_error;
};

struct HermesABIStringData {
  bool is_ascii;
  const void *data;
  size_t length;
};

// Runtime

struct HermesABIPreparedJavaScriptOrError
hermes_runtime_prepared_javascript_create(
  struct HermesABIRuntime *rt,
  const uint8_t *utf8_source,
  size_t source_length,
  const char *source_url
);

struct HermesABIValueOrError
hermes_runtime_prepared_javascript_evaluate(
  struct HermesABIRuntime *rt,
  struct HermesABIPreparedJavaScript *prepared
);

void hermes_preparedjavascript_release(
  struct HermesABIPreparedJavaScript *prepared
);

uint64_t hermes_value_get_unique_id(
  struct HermesABIRuntime *rt,
  struct HermesABIValue val
);

struct HermesABIValueOrError hermes_object_from_id(
  struct HermesABIRuntime *rt,
  uint64_t id
);

bool hermes_runtime_drain_microtasks(
  struct HermesABIRuntime *rt,
  int max_microtasks_hint
);

struct HermesABIValueOrError hermes_value_create_from_json_utf8(
  struct HermesABIRuntime *rt,
  const uint8_t *json_bytes,
  size_t length
);

struct HermesABIStringData hermes_string_get_data(
  struct HermesABIRuntime *rt,
  struct HermesABIString str
);

struct HermesABIStringData hermes_propnameid_get_data(
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID prop
);

struct HermesABIValueOrError hermes_array_value_get_at_index(
  struct HermesABIRuntime *rt,
  struct HermesABIArray arr,
  size_t index
);

struct HermesABIVoidOrError hermes_array_value_set_at_index(
  struct HermesABIRuntime *rt,
  struct HermesABIArray arr,
  size_t index,
  struct HermesABIValue value
);

struct HermesABIRuntime *hermes_runtime_create(
  struct HermesABIRuntimeConfig config
);

void hermes_runtime_release(struct HermesABIRuntime *rt);
void hermes_runtime_release_from_finalizer(struct HermesABIRuntime *rt);

struct HermesABIObject hermes_runtime_get_global_object(
  struct HermesABIRuntime *rt
);

struct HermesABIValue hermes_evaluate_javascript(
  struct HermesABIRuntime *rt,
  const uint8_t *script,
  size_t script_len,
  const char *source_url
);

struct HermesABIValue hermes_evaluate_bytecode(
  struct HermesABIRuntime *rt,
  const uint8_t *data,
  size_t size,
  const char *source_url
);

// Runtime error handling

void hermes_runtime_set_js_error_value(
  struct HermesABIRuntime *rt,
  struct HermesABIValue error_value
);

void hermes_runtime_set_native_exception_message(
  struct HermesABIRuntime *rt,
  const char *message
);

struct HermesABIValue hermes_runtime_get_and_clear_js_error_value(
  struct HermesABIRuntime *rt
);

char *hermes_runtime_get_and_clear_native_exception_message(
  struct HermesABIRuntime *rt
);

// Memory

void hermes_pointer_release(struct HermesABIManagedPointer *ptr);
void hermes_pointer_release_safe(struct HermesABIManagedPointer *ptr);

void hermes_register_pointer(
  struct HermesABIRuntime *rt,
  struct HermesABIManagedPointer *ptr
);

// Objects

struct HermesABIObjectOrError hermes_object_create(struct HermesABIRuntime *rt);

struct HermesABIObject hermes_object_clone(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

struct HermesABIValue hermes_object_get_property_from_value(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIValue key
);

struct HermesABIVoidOrError hermes_object_set_property_from_value(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIValue key,
  struct HermesABIValue value
);

struct HermesABIBoolOrError hermes_object_has_property_from_value(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIValue key
);

struct HermesABIValue hermes_object_get_property_from_propnameid(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIPropNameID name
);

struct HermesABIVoidOrError hermes_object_set_property_from_propnameid(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIPropNameID name,
  struct HermesABIValue value
);

struct HermesABIBoolOrError hermes_object_has_property_from_propnameid(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIPropNameID name
);

struct HermesABIArrayOrError hermes_object_get_property_names(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

bool hermes_object_strict_equals(
  struct HermesABIRuntime *rt,
  struct HermesABIObject a,
  struct HermesABIObject b
);

bool hermes_object_is_function(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

bool hermes_object_is_array(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

bool hermes_object_is_arraybuffer(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

struct HermesABIBoolOrError hermes_instance_of(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  struct HermesABIFunction ctor
);

// Host objects

struct HermesABIObjectOrError hermes_object_create_from_host_object(
  struct HermesABIRuntime *rt,
  void *user_data,
  HermesABIHostObjectGet get_cb,
  HermesABIHostObjectSet set_cb,
  HermesABIHostObjectGetOwnKeys get_own_keys_cb,
  HermesABIHostObjectRelease release_cb
);

// Native state

struct HermesABIVoidOrError hermes_object_set_native_state(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj,
  void *user_data,
  HermesABINativeStateRelease release_cb
);

void *hermes_object_get_native_state_data(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

// Functions

struct HermesABIValue hermes_function_call(
  struct HermesABIRuntime *rt,
  struct HermesABIFunction fn,
  struct HermesABIValue js_this,
  struct HermesABIValue *args,
  size_t arg_count
);

struct HermesABIValue hermes_function_call_as_constructor(
  struct HermesABIRuntime *rt,
  struct HermesABIFunction fn,
  struct HermesABIValue *args,
  size_t arg_count
);

struct HermesABIFunctionOrError hermes_function_create_from_host(
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID name,
  unsigned int length,
  void *user_data,
  HermesABIHostFunctionCall call_cb,
  HermesABIHostFunctionRelease release_cb
);

// Strings

struct HermesABIStringOrError hermes_create_string_from_utf8(
  struct HermesABIRuntime *rt,
  const char *str
);

char *hermes_string_to_utf8(
  struct HermesABIRuntime *rt,
  struct HermesABIString str
);

struct HermesABIString hermes_string_clone(
  struct HermesABIRuntime *rt,
  struct HermesABIString str
);

bool hermes_string_strict_equals(
  struct HermesABIRuntime *rt,
  struct HermesABIString a,
  struct HermesABIString b
);

// Symbols

char *hermes_symbol_to_utf8(
  struct HermesABIRuntime *rt,
  struct HermesABISymbol sym
);

struct HermesABISymbol hermes_symbol_clone(
  struct HermesABIRuntime *rt,
  struct HermesABISymbol sym
);

bool hermes_symbol_strict_equals(
  struct HermesABIRuntime *rt,
  struct HermesABISymbol a,
  struct HermesABISymbol b
);

// PropNameIDs

struct HermesABIPropNameIDOrError hermes_propnameid_create_from_string(
  struct HermesABIRuntime *rt,
  struct HermesABIString str
);

struct HermesABIPropNameIDOrError hermes_propnameid_create_from_symbol(
  struct HermesABIRuntime *rt,
  struct HermesABISymbol sym
);

char *hermes_propnameid_to_utf8(
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID name
);

struct HermesABIPropNameID hermes_propnameid_clone(
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID name
);

bool hermes_propnameid_equals(
  struct HermesABIRuntime *rt,
  struct HermesABIPropNameID a,
  struct HermesABIPropNameID b
);

struct HermesABIPropNameIDList *hermes_propnameid_list_create(
  const struct HermesABIPropNameID *props,
  size_t size
);

// BigInts

struct HermesABIBigIntOrError hermes_bigint_create_from_int64(
  struct HermesABIRuntime *rt,
  int64_t value
);

bool hermes_bigint_is_int64(
  struct HermesABIRuntime *rt,
  struct HermesABIBigInt bi
);

int64_t hermes_bigint_as_int64(
  struct HermesABIRuntime *rt,
  struct HermesABIBigInt bi
);

struct HermesABIStringOrError hermes_bigint_to_string(
  struct HermesABIRuntime *rt,
  struct HermesABIBigInt bi,
  int radix
);

struct HermesABIBigInt hermes_bigint_clone(
  struct HermesABIRuntime *rt,
  struct HermesABIBigInt bi
);

bool hermes_bigint_strict_equals(
  struct HermesABIRuntime *rt,
  struct HermesABIBigInt a,
  struct HermesABIBigInt b
);

// Arrays

struct HermesABIArrayOrError hermes_array_create(
  struct HermesABIRuntime *rt,
  size_t length
);

size_t hermes_array_get_length(
  struct HermesABIRuntime *rt,
  struct HermesABIArray arr
);

// ArrayBuffers

struct HermesABIArrayBufferOrError hermes_arraybuffer_create_from_external_data(
  struct HermesABIRuntime *rt,
  uint8_t *data,
  size_t size,
  void *user_data,
  HermesABIMutableBufferRelease release_cb
);

struct HermesABIUint8PtrOrError hermes_arraybuffer_get_data(
  struct HermesABIRuntime *rt,
  struct HermesABIArrayBuffer ab
);

struct HermesABISizeTOrError hermes_arraybuffer_get_size(
  struct HermesABIRuntime *rt,
  struct HermesABIArrayBuffer ab
);

// Weak objects

struct HermesABIWeakObjectOrError hermes_weak_object_create(
  struct HermesABIRuntime *rt,
  struct HermesABIObject obj
);

struct HermesABIValue hermes_weak_object_lock(
  struct HermesABIRuntime *rt,
  struct HermesABIWeakObject wo
);

#ifdef __cplusplus
}
#endif

#endif
