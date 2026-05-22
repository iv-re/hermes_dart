import 'dart:ffi';

import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_object.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/js_string.dart';
import 'package:hermes_dart/src/js_symbol.dart';
import 'package:hermes_dart/src/utils.dart';
import 'package:meta/meta.dart';

/// Represents a unique identifier for a JavaScript property.
///
/// [JSPropNameId] can be created from a string or a symbol and is used for
/// efficient property lookups on [JSObject].
///
/// Example:
/// ```dart
/// final propId = JSPropId.fromString('id', rt: rt);
/// obj.setPropertyByPropId(propId, JSValue.number(1, rt: rt));
/// ```
extension type JSPropNameId(JSPointer _pointer) implements Finalizable {
  /// Creates a property ID from a Dart [String].
  factory JSPropNameId.fromString(JSRuntime rt, String name) {
    final jsStr = JSString.fromString(rt, name, attachFinalizer: false);
    try {
      final result = hermes_propnameid_create_from_string(rt.ptr, jsStr.ptr);
      final ptr = rt.unwrapPtr(result.ptr_or_error, (ptr) {
        return ptr.cast<HermesABIManagedPointer>();
      });

      return JSPropNameId(JSPointer(rt, ptr));
    } finally {
      jsStr.jsPointer.release();
    }
  }

  /// Creates a property ID from a [JSSymbol].
  factory JSPropNameId.fromSymbol(JSRuntime rt, JSSymbol symbol) {
    final result = hermes_propnameid_create_from_symbol(rt.ptr, symbol.ptr);
    final ptr = rt.unwrapPtr(result.ptr_or_error, (ptr) {
      return ptr.cast<HermesABIManagedPointer>();
    });

    return JSPropNameId(JSPointer(rt, ptr));
  }

  @internal
  factory JSPropNameId.fromABI(
    JSRuntime rt,
    HermesABIPropNameID abi, {
    bool attachFinalizer = true,
  }) {
    return JSPropNameId(
      JSPointer(rt, abi.pointer, attachFinalizer: attachFinalizer),
    );
  }

  HermesABIPropNameID get ptr => _pointer.asPropNameId;

  JSRuntime get _rt => _pointer.rt;

  /// Returns the string representation of this property ID.
  String get string {
    return hermes_propnameid_get_data(_rt.ptr, ptr).toDartString();
  }

  /// Returns true if this property ID is equal to [other].
  bool equals(JSPropNameId other) {
    return hermes_propnameid_equals(_rt.ptr, ptr, other.ptr);
  }

  /// Increments the native reference count and returns a new handle to the
  /// same property ID.
  JSPropNameId retain() {
    final cloned = hermes_propnameid_clone(_rt.ptr, ptr);
    return JSPropNameId(JSPointer(_rt, cloned.pointer));
  }
}

extension JSPointerPropNameIdExt on JSPointer {
  HermesABIPropNameID get asPropNameId => Struct.create()..pointer = handle;
}
