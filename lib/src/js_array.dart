import 'dart:collection';
import 'dart:ffi';

import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_function.dart';
import 'package:hermes_dart/src/js_object.dart';
import 'package:hermes_dart/src/js_pointer.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:hermes_dart/src/js_value.dart';

/// Represents a JavaScript `Array`.
///
/// Use [JSArray.create] to create a new array or [JSValue.asArray] to access
/// an existing one.
///
/// Example:
/// ```dart
/// final array = JSArray.create(rt)
///   ..add(JSValue.number(42, rt: rt));
/// print(array[0].asNumber); // 42
/// ```
extension type JSArray(JSPointer jsPointer) implements Finalizable {
  /// Creates a new array with the given [length].
  factory JSArray.create(JSRuntime rt, [int length = 0]) {
    final result = hermes_array_create(rt.ptr, length);
    final arr = rt.unwrapPtr(result.ptr_or_error, (ptr) {
      return ptr.cast<HermesABIManagedPointer>();
    });

    return JSArray(JSPointer(rt, arr));
  }

  HermesABIArray get ptr => jsPointer.asArray;

  JSRuntime get _rt => jsPointer.rt;

  JSObject get asObject => JSObject(jsPointer);

  JSValue get asValue => JSValue.fromObject(asObject);

  /// The number of elements in the array.
  int get length {
    final arrayPtr = Struct.create<HermesABIArray>()..pointer = ptr.pointer;
    return hermes_array_get_length(_rt.ptr, arrayPtr);
  }

  /// Sets the number of elements in the array.
  set length(int value) {
    asObject['length'] = JSValue.number(_rt, value.toDouble());
  }

  /// Returns the element at [index].
  JSValue operator [](int index) {
    final result = hermes_array_value_get_at_index(_rt.ptr, ptr, index);
    return JSValue.fromABI(_rt, result.value);
  }

  /// Sets the element at [index] to [value].
  void operator []=(int index, JSValue value) {
    final result = hermes_array_value_set_at_index(
      _rt.ptr,
      ptr,
      index,
      value.ptr,
    );
    _rt.unwrapVoid(result.void_or_error);
  }

  /// Appends [value] to the end of the array.
  void add(JSValue value) {
    this[length] = value;
  }

  /// Returns a live, zero-copy [List<JSValue>] adapter view of this array.
  ///
  /// Any modifications to the returned list will be reflected in the underlying
  /// JavaScript array, and vice versa.
  List<JSValue> get asList => _JSArrayList(this);

  /// Increments the native reference count and returns a new handle to the
  /// same array.
  JSArray retain() {
    final cloned = hermes_object_clone(_rt.ptr, jsPointer.asObject);

    return JSArray(JSPointer(_rt, cloned.pointer));
  }
}

extension JSPointerArrayExt on JSPointer {
  HermesABIArray get asArray => Struct.create()..pointer = handle;
}

class _JSArrayList with ListMixin<JSValue> {
  _JSArrayList(JSArray array)
    : _array = array,
      _arrayValue = array.asValue,
      _pushFn = array.jsPointer.rt.memoize(
        'Array.prototype.push',
        () => array
            .jsPointer
            .rt
            .global['Array']
            .asObject['prototype']
            .asObject['push']
            .asFunctionUnsafe,
      ),
      _popFn = array.jsPointer.rt.memoize(
        'Array.prototype.pop',
        () => array
            .jsPointer
            .rt
            .global['Array']
            .asObject['prototype']
            .asObject['pop']
            .asFunctionUnsafe,
      ),
      _spliceFn = array.jsPointer.rt.memoize(
        'Array.prototype.splice',
        () => array
            .jsPointer
            .rt
            .global['Array']
            .asObject['prototype']
            .asObject['splice']
            .asFunctionUnsafe,
      );

  final JSArray _array;
  final JSValue _arrayValue;
  final JSFunction _pushFn;
  final JSFunction _popFn;
  final JSFunction _spliceFn;

  @override
  int get length => _array.length;

  @override
  set length(int newLength) {
    _array.length = newLength;
  }

  @override
  JSValue operator [](int index) => _array[index];

  @override
  void operator []=(int index, JSValue value) {
    _array[index] = value;
  }

  @override
  void add(JSValue element) {
    _array.add(element);
  }

  @override
  void addAll(Iterable<JSValue> iterable) {
    _pushFn(iterable.toList(), _arrayValue);
  }

  @override
  JSValue removeLast() {
    if (length == 0) {
      throw StateError('Cannot remove from an empty list');
    }
    return _popFn([], _arrayValue);
  }

  @override
  JSValue removeAt(int index) {
    if (index < 0 || index >= length) {
      throw RangeError.index(index, this);
    }
    final value = this[index];
    _spliceFn([
      JSValue.number(_array._rt, index.toDouble()),
      JSValue.number(_array._rt, 1),
    ], _arrayValue);
    return value;
  }

  @override
  void insert(int index, JSValue element) {
    if (index < 0 || index > length) {
      throw RangeError.range(index, 0, length);
    }
    _spliceFn([
      JSValue.number(_array._rt, index.toDouble()),
      JSValue.number(_array._rt, 0),
      element,
    ], _arrayValue);
  }

  @override
  void clear() {
    _array.length = 0;
  }
}
