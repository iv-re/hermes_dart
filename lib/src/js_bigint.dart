import 'dart:ffi';

import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/js_string.dart';
import 'package:hermes_dart/src/js_value.dart';

/// Represents a JavaScript `BigInt`.
///
/// Use [JSBigInt.fromInt] or [JSBigInt.fromBigInt] to create a new `BigInt`,
/// or [JSValue.asBigInt] to access an existing one.
extension type JSBigInt(JSPointer jsPointer) {
  /// Creates a `BigInt` from a 64-bit integer.
  factory JSBigInt.fromInt(JSRuntime rt, int value) {
    final result = hermes_bigint_create_from_int64(rt.ptr, value);
    final bi = rt.unwrapPtr(result.ptr_or_error, (ptr) {
      return ptr.cast<HermesABIManagedPointer>();
    });

    return JSBigInt(JSPointer(rt, bi));
  }

  /// Creates a `BigInt` from a Dart [BigInt].
  factory JSBigInt.fromBigInt(JSRuntime rt, BigInt value) {
    if (value.isValidInt) {
      return JSBigInt.fromInt(rt, value.toInt());
    }

    final biCtor = rt.memoize(
      'BigInt',
      () => rt.global['BigInt'].asFunctionUnsafe,
    );
    final strVal = JSValue.string(
      rt,
      value.toString(),
      attachFinalizer: false,
    );
    try {
      final res = biCtor.call([strVal]);
      return res.asBigInt;
    } finally {
      strVal.release();
    }
  }

  HermesABIBigInt get ptr => jsPointer.asBigInt;

  JSRuntime get _rt => jsPointer.rt;

  /// Returns true if this `BigInt` fits in a 64-bit integer.
  bool isInt() => hermes_bigint_is_int64(_rt.ptr, ptr);

  /// Returns this `BigInt` as a 64-bit integer.
  int asInt() {
    if (!isInt()) {
      throw StateError('BigInt does not fit in int64');
    }
    return hermes_bigint_as_int64(_rt.ptr, ptr);
  }

  /// Returns a string representation of this `BigInt` in the given [radix].
  String toRadixString({int radix = 10}) {
    final result = hermes_bigint_to_string(_rt.ptr, ptr, radix);
    final jsStr = _rt.unwrapPtr(result.ptr_or_error, (ptr) {
      return JSString(JSPointer(_rt, ptr.cast()));
    });

    return jsStr.string;
  }

  /// Converts this `BigInt` to a Dart [BigInt].
  BigInt toBigInt() {
    if (isInt()) {
      return BigInt.from(asInt());
    }
    return BigInt.parse(toRadixString());
  }

  bool strictEquals(JSBigInt other) {
    return hermes_bigint_strict_equals(_rt.ptr, ptr, other.ptr);
  }

  /// Increments the native reference count and returns a new handle to the
  /// same BigInt.
  JSBigInt retain() {
    final cloned = hermes_bigint_clone(_rt.ptr, ptr);

    return JSBigInt(JSPointer(_rt, cloned.pointer));
  }
}

extension JSPointerBigIntExt on JSPointer {
  HermesABIBigInt get asBigInt => Struct.create()..pointer = handle;
}
