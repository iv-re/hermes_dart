import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_error.dart';
import 'package:hermes_dart/src/js_object.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_prepared_javascript.dart';
import 'package:hermes_dart/src/js_runtime_config.dart';
import 'package:hermes_dart/src/js_value.dart';
import 'package:meta/meta.dart';

typedef _HermesRuntimeReleaseFn =
    NativeFunction<Void Function(Pointer<HermesABIRuntime>)>;

final Map<int, WeakReference<JSRuntime>> _runtimesRegistry = {};

class JSRuntime implements Finalizable {
  factory JSRuntime(Pointer<HermesABIRuntime> ptr) {
    final cached = _runtimesRegistry[ptr.address]?.target;
    if (cached != null) return cached;

    final rt = JSRuntime._(ptr);
    _runtimesRegistry[ptr.address] = WeakReference(rt);
    return rt;
  }

  JSRuntime._(this.ptr);

  JSRuntime.create([
    JSRuntimeConfig config = const JSRuntimeConfig.hardened(),
  ]) : ptr = _createRuntime(config) {
    _finalizer.attach(this, ptr.cast(), detach: this, externalSize: 512);
    _runtimesRegistry[ptr.address] = WeakReference(this);
  }

  static Pointer<HermesABIRuntime> _createRuntime(JSRuntimeConfig config) {
    final struct = Struct.create<HermesABIRuntimeConfig>()
      ..enable_eval = config.enableEval
      ..es6_proxy = config.es6Proxy
      ..enable_generator = config.enableGenerator
      ..enable_async_generators = config.enableAsyncGenerators
      ..es6_block_scoping = config.es6BlockScoping
      ..intl = config.intl
      ..microtask_queue = config.microtaskQueue
      ..compilation_mode = config.compilationMode.value
      ..bytecode_warmup_percent = config.bytecodeWarmupPercent
      ..optimized_eval = config.optimizedEval
      ..enable_hermes_internal = config.enableHermesInternal
      ..enable_hermes_internal_test_methods =
          config.enableHermesInternalTestMethods
      ..randomize_memory_layout = config.randomizeMemoryLayout;
    return hermes_runtime_create(struct);
  }

  static NativeFinalizer _createFinalizer() {
    final ptr = Native.addressOf<_HermesRuntimeReleaseFn>(
      hermes_runtime_release_from_finalizer,
    );

    return NativeFinalizer(ptr.cast());
  }

  static final NativeFinalizer _finalizer = _createFinalizer();

  @internal
  final Pointer<HermesABIRuntime> ptr;
  bool _released = false;

  bool get isReleased => _released;

  final Map<String, Object> _memoizedCache = {};
  final List<Pointer<Uint8>> _bytecodeBuffers = [];

  JSObject? _globalThis;

  JSObject get global {
    return _globalThis ??= JSObject(
      JSPointer(
        this,
        hermes_runtime_get_global_object(ptr).pointer,
      ),
    );
  }

  @internal
  T memoize<T>(String key, T Function() builder) {
    final cached = _memoizedCache[key];
    if (cached != null) return cached as T;

    final value = builder();
    _memoizedCache[key] = value as Object;
    return value;
  }

  JSValue evaluateJavascript(
    String script, {
    String sourceUrl = 'script.js',
  }) => using((arena) {
    final nativeScript = script.toNativeUtf8(allocator: arena);
    final nativeSource = sourceUrl.toNativeUtf8(allocator: arena);

    final result = hermes_evaluate_javascript(
      ptr,
      nativeScript.cast(),
      nativeScript.length,
      nativeSource.cast(),
    );

    return JSValue.fromABI(this, result);
  });

  JSValue evaluateBytecode(
    Uint8List bytecode, {
    String sourceUrl = 'bytecode.hbc',
  }) => using((arena) {
    final nativeSource = sourceUrl.toNativeUtf8(allocator: arena);

    final data = malloc<Uint8>(bytecode.length);
    data.asTypedList(bytecode.length).setAll(0, bytecode);
    _bytecodeBuffers.add(data);

    final result = hermes_evaluate_bytecode(
      ptr,
      data,
      bytecode.length,
      nativeSource.cast(),
    );

    return JSValue.fromABI(this, result);
  });

  /// Prepares JavaScript source code for execution.
  JSPreparedJavaScript prepareJavaScript(
    String source, {
    String sourceUrl = 'script.js',
  }) => using((arena) {
    final nativeSourceUrl = sourceUrl.toNativeUtf8(allocator: arena);
    final nativeSource = source.toNativeUtf8(allocator: arena);
    final res = hermes_runtime_prepared_javascript_create(
      ptr,
      nativeSource.cast(),
      nativeSource.length,
      nativeSourceUrl.cast(),
    );
    final rawPtr = unwrapPtr(res.ptr_or_error, (p) => p);
    return JSPreparedJavaScript(rawPtr.cast());
  });

  /// Evaluates prepared JavaScript bytecode.
  JSValue evaluatePreparedJavaScript(JSPreparedJavaScript prepared) {
    final result = hermes_runtime_prepared_javascript_evaluate(
      ptr,
      prepared.ptr,
    );
    return JSValue.fromABI(this, result.value);
  }

  /// Drains the microtask queue.
  bool drainMicrotasks({int maxMicrotasksHint = -1}) {
    return hermes_runtime_drain_microtasks(ptr, maxMicrotasksHint);
  }

  JSValue getAndClearJSErrorValue() {
    return JSValue.fromABI(
      this,
      hermes_runtime_get_and_clear_js_error_value(ptr),
    );
  }

  String getAndClearNativeExceptionMessage() {
    final ptr = hermes_runtime_get_and_clear_native_exception_message(this.ptr);
    if (ptr == nullptr) return '';

    try {
      return ptr.cast<Utf8>().toDartString();
    } finally {
      malloc.free(ptr);
    }
  }

  // TODO: generic for Pointer?
  T unwrapPtr<T>(int ptrOrError, T Function(Pointer ptr) fn) {
    if ((ptrOrError & 1) != 0) {
      handleErrorCode(ptrOrError >> 2);
    }
    return fn(Pointer.fromAddress(ptrOrError));
  }

  bool unwrapBool(int boolOrError) {
    unwrapVoid(boolOrError);
    return (boolOrError >> 2) != 0;
  }

  void unwrapVoid(int ptrOrError) {
    if ((ptrOrError & 1) != 0) {
      handleErrorCode(ptrOrError >> 2);
    }
  }

  void handleErrorCode(int errorCode) {
    if (errorCode ==
        HermesABIErrorCode.HermesABIErrorCodeNativeException.value) {
      throw JSNativeException(getAndClearNativeExceptionMessage());
    } else {
      throw JSException(getAndClearJSErrorValue());
    }
  }

  void setJSErrorValue(JSValue value) {
    hermes_runtime_set_js_error_value(ptr, value.ptr);
  }

  void setNativeExceptionMessage(String message) => using((arena) {
    final nativeMsg = message.toNativeUtf8(allocator: arena);
    hermes_runtime_set_native_exception_message(ptr, nativeMsg.cast());
  });

  void release() {
    if (_released) return;
    _memoizedCache.clear();
    _finalizer.detach(this);
    _runtimesRegistry.remove(ptr.address);
    hermes_runtime_release(ptr);
    _bytecodeBuffers
      ..forEach(malloc.free)
      ..clear();
    _released = true;
  }
}
