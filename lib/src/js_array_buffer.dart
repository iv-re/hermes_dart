import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_object.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/js_value.dart';
import 'package:meta/meta.dart';

final Map<int, void Function()> _bufferReleaseRegistry = {};
int _nextBufferId = 1;

/// Represents a JavaScript `ArrayBuffer`.
///
/// Create a buffer with [JSArrayBuffer.create] or [JSArrayBuffer.fromBytes],
/// or access one created in JavaScript via [JSValue.asArrayBuffer].
///
/// The [data] getter returns a [Uint8List] view directly over the native
/// memory — reads and writes go straight to the underlying buffer with no
/// copying.
///
/// Example:
/// ```dart
/// // Create an empty buffer and fill it.
/// final ab = JSArrayBuffer.create(rt, 16);
/// ab.data.fillRange(0, 16, 0xFF);
///
/// // Create from existing bytes (copies data in).
/// final ab2 = JSArrayBuffer.fromBytes(rt, Uint8List.fromList([1, 2, 3]));
///
/// // Access a buffer created in JS.
/// final ab3 = rt.evaluateJavascript('new ArrayBuffer(16)').asArrayBuffer;
/// print(ab3.byteLength); // 16
/// ab3.data[0] = 42;
/// ```
extension type JSArrayBuffer(JSPointer jsPointer) implements Finalizable {
  /// Creates a new `ArrayBuffer` of [size] bytes, initialized to zero.
  ///
  /// Memory is allocated and freed automatically.
  ///
  /// Example:
  /// ```dart
  /// final ab = JSArrayBuffer.create(rt, 1024);
  /// ab.data[0] = 42;
  /// ab.data.fillRange(0, 1024, 0xFF);
  /// ```
  factory JSArrayBuffer.create(JSRuntime rt, int size) {
    final nativePtr = calloc<Uint8>(size);
    return JSArrayBuffer.fromPointer(
      rt,
      nativePtr,
      size,
      onRelease: () => calloc.free(nativePtr),
    );
  }

  /// Creates a new `ArrayBuffer` from a [Uint8List], copying the data into
  /// native memory.
  ///
  /// Example:
  /// ```dart
  /// final bytes = Uint8List.fromList([1, 2, 3, 4]);
  /// final ab = JSArrayBuffer.fromBytes(rt, bytes);
  /// print(ab.data); // [1, 2, 3, 4]
  /// ```
  factory JSArrayBuffer.fromBytes(JSRuntime rt, Uint8List bytes) {
    final size = bytes.length;
    final nativePtr = calloc<Uint8>(size);
    nativePtr.asTypedList(size).setAll(0, bytes);
    return JSArrayBuffer.fromPointer(
      rt,
      nativePtr,
      size,
      onRelease: () => calloc.free(nativePtr),
    );
  }

  @internal
  factory JSArrayBuffer.fromPointer(
    JSRuntime rt,
    Pointer<Uint8> data,
    int size, {
    void Function()? onRelease,
  }) {
    Pointer<Void> userData = nullptr;

    if (onRelease != null) {
      final id = _nextBufferId++;
      _bufferReleaseRegistry[id] = onRelease;
      userData = Pointer.fromAddress(id);
    }

    final result = hermes_arraybuffer_create_from_external_data(
      rt.ptr,
      data,
      size,
      userData,
      onRelease != null ? _bufferReleaseCallable.nativeFunction : nullptr,
    );

    final ptr = rt.unwrapPtr(result.ptr_or_error, (ptr) {
      return ptr.cast<HermesABIManagedPointer>();
    });

    return JSArrayBuffer(JSPointer(rt, ptr, externalSize: size + 128));
  }

  HermesABIArrayBuffer get ptr => jsPointer.asArrayBuffer;

  JSRuntime get _rt => jsPointer.rt;

  /// The length of the buffer in bytes.
  int get byteLength {
    final result = hermes_arraybuffer_get_size(_rt.ptr, ptr);

    if (result.is_error) {
      _rt.handleErrorCode(result.data.error);
    }

    return result.data.val;
  }

  /// A [Uint8List] view over the underlying buffer data.
  ///
  /// This is a zero-copy view — reading from and writing to this list
  /// directly accesses the native memory backing the `ArrayBuffer`.
  ///
  /// Example:
  /// ```dart
  /// ab.data[0] = 42;
  /// ab.data.fillRange(0, ab.byteLength, 0xFF);
  /// ```
  Uint8List get data {
    final result = hermes_arraybuffer_get_data(_rt.ptr, ptr);

    if (result.is_error) {
      _rt.handleErrorCode(result.data.error);
    }

    return result.data.val.asTypedList(byteLength);
  }

  JSObject get asObject => JSObject(jsPointer);

  JSValue get asValue => JSValue.fromObject(asObject);

  /// Increments the native reference count and returns a new handle to the
  /// same `ArrayBuffer`.
  JSArrayBuffer retain() {
    final cloned = hermes_object_clone(_rt.ptr, jsPointer.asObject);

    return JSArrayBuffer(JSPointer(_rt, cloned.pointer));
  }
}

extension JSPointerArrayBufferExt on JSPointer {
  HermesABIArrayBuffer get asArrayBuffer => Struct.create()..pointer = handle;
}

final _bufferReleaseCallable =
    NativeCallable<HermesABIMutableBufferReleaseFunction>.listener(
      (Pointer<Void> userData) {
        final id = userData.address;
        final onRelease = _bufferReleaseRegistry.remove(id);
        onRelease?.call();
      },
    )..keepIsolateAlive = false;
