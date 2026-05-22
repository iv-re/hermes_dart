import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/js_value.dart';

/// Represents a JavaScript `Symbol`.
///
/// Symbols are unique primitive values often used as property keys.
extension type JSSymbol(JSPointer jsPointer) implements Finalizable {
  /// Creates a new JavaScript symbol with an optional [description].
  ///
  /// Example:
  /// ```dart
  /// final sym = JSSymbol.create(rt, 'id');
  /// ```
  factory JSSymbol.create(JSRuntime rt, [String? description]) {
    final symbolCtor = rt.memoize(
      'Symbol',
      () => rt.global['Symbol'].asFunctionUnsafe,
    );

    if (description != null) {
      final strVal = JSValue.string(rt, description, attachFinalizer: false);

      try {
        final result = symbolCtor.call([strVal]);
        return result.asSymbol;
      } finally {
        strVal.release();
      }
    } else {
      final result = symbolCtor.call([]);
      return result.asSymbol;
    }
  }

  HermesABISymbol get ptr => jsPointer.asSymbol;

  JSRuntime get _rt => jsPointer.rt;

  /// Returns the string representation of this symbol.
  String getDescription() {
    final ptr = hermes_symbol_to_utf8(_rt.ptr, this.ptr);

    try {
      return ptr.cast<Utf8>().toDartString();
    } finally {
      malloc.free(ptr);
    }
  }

  bool strictEquals(JSSymbol other) {
    return hermes_symbol_strict_equals(_rt.ptr, ptr, other.ptr);
  }

  /// Increments the native reference count and returns a new handle to the
  /// same symbol.
  JSSymbol retain() {
    final cloned = hermes_symbol_clone(_rt.ptr, ptr);

    return JSSymbol(JSPointer(_rt, cloned.pointer));
  }
}

extension JSPointerSymbolExt on JSPointer {
  HermesABISymbol get asSymbol => Struct.create()..pointer = handle;
}
