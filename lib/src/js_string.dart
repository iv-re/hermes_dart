import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/utils.dart';

/// Represents a JavaScript string.
///
/// Example:
/// ```dart
/// final jsStr = JSString.fromString('hello', rt: rt);
/// print(jsStr.string); // 'hello'
/// ```
extension type JSString(JSPointer jsPointer) implements Finalizable {
  /// Creates a new JavaScript string from a Dart [string]
  factory JSString.fromString(
    JSRuntime rt,
    String string, {
    bool attachFinalizer = true,
  }) {
    final cString = string.toNativeUtf8();

    try {
      final result = hermes_create_string_from_utf8(rt.ptr, cString.cast());

      final ptr = rt.unwrapPtr(
        result.ptr_or_error,
        (ptr) => ptr.cast<HermesABIManagedPointer>(),
      );

      return JSString(
        JSPointer(
          rt,
          ptr,
          attachFinalizer: attachFinalizer,
        ),
      );
    } finally {
      malloc.free(cString);
    }
  }

  JSRuntime get _rt => jsPointer.rt;

  HermesABIString get ptr => jsPointer.asString;

  /// Returns this JavaScript string as a Dart [String].
  String get string {
    return hermes_string_get_data(_rt.ptr, ptr).toDartString();
  }

  /// Returns true if this string is equal to [other].
  bool strictEquals(JSString other) {
    return hermes_string_strict_equals(_rt.ptr, ptr, other.ptr);
  }

  /// Increments the native reference count and returns a new handle to the
  /// same JSString.
  JSString retain() {
    final cloned = hermes_string_clone(_rt.ptr, ptr);

    return JSString(JSPointer(_rt, cloned.pointer));
  }
}

extension JSPointerStringExt on JSPointer {
  HermesABIString get asString => Struct.create()..pointer = handle;
}
