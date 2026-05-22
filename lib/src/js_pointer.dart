import 'dart:ffi';

import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:hermes_dart/src/js_runtime.dart';
import 'package:meta/meta.dart';

typedef _HermesManagedPointerReleaseFn =
    NativeFunction<Void Function(Pointer<HermesABIManagedPointer>)>;

class JSPointer implements Finalizable {
  JSPointer(
    this.rt,
    Pointer<HermesABIManagedPointer> handle, {
    this.externalSize = 128,
    this.attachFinalizer = true,
  }) : _handle = handle {
    if (attachFinalizer) {
      hermes_register_pointer(rt.ptr, _handle);
      _finalizer.attach(
        this,
        _handle.cast<Void>(),
        detach: this,
        externalSize: externalSize,
      );
    }
  }

  static NativeFinalizer _createFinalizer() {
    final ptr = Native.addressOf<_HermesManagedPointerReleaseFn>(
      hermes_pointer_release_safe,
    );

    return NativeFinalizer(ptr.cast());
  }

  static final NativeFinalizer _finalizer = _createFinalizer();

  @internal
  final JSRuntime rt;

  final Pointer<HermesABIManagedPointer> _handle;

  @internal
  final int? externalSize;

  final bool attachFinalizer;
  bool _released = false;

  bool get isReleased => _released;

  @internal
  Pointer<HermesABIManagedPointer> get handle {
    if (_released) {
      throw StateError('Attempted to use a released JSPointer.');
    }
    return _handle;
  }

  void detachFinalizer() => _finalizer.detach(this);

  /// Manually release the underlying pointer immediately.
  void release() {
    if (_released) return;
    _released = true;
    if (attachFinalizer) {
      detachFinalizer();
      hermes_pointer_release_safe(_handle);
    } else {
      hermes_pointer_release(_handle);
    }
  }
}
