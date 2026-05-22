import 'dart:ffi';

import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_object.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/js_value.dart';

/// A weak reference to a JavaScript object.
///
/// This allows you to hold a reference to a JS object without preventing
/// it from being garbage collected.
extension type JSWeakObject(JSPointer jsPointer) {
  /// Creates a weak reference to the given [object].
  factory JSWeakObject.create(JSRuntime rt, JSObject object) {
    final result = hermes_weak_object_create(rt.ptr, object.ptr);
    return rt.unwrapPtr(result.ptr_or_error, (ptr) {
      return JSWeakObject(JSPointer(rt, ptr.cast()));
    });
  }

  HermesABIWeakObject get ptr => jsPointer.asWeakObject;

  JSRuntime get _rt => jsPointer.rt;

  /// Attempts to lock the weak reference and return a strong reference
  /// to the object.
  ///
  /// Returns a [JSValue] that represents the object if it is still alive,
  /// or an `undefined` value if the object has been garbage collected.
  JSValue lock() {
    final result = hermes_weak_object_lock(_rt.ptr, ptr);
    return JSValue.fromABI(_rt, result);
  }
}

extension JSPointerWeakObjectExt on JSPointer {
  HermesABIWeakObject get asWeakObject => Struct.create()..pointer = handle;
}
