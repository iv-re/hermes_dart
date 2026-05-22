import 'dart:ffi';
import 'package:hermes_dart/hermes_dart.g.dart';
import 'package:meta/meta.dart';

/// A class representing compiled JavaScript bytecode.
class JSPreparedJavaScript implements Finalizable {
  @internal
  JSPreparedJavaScript(this._ptr) {
    _finalizer.attach(
      this,
      _ptr.cast(),
      detach: this,
      externalSize: 128,
    );
  }

  final Pointer<HermesABIPreparedJavaScript> _ptr;
  bool _released = false;

  static final _finalizer = NativeFinalizer(
    Native.addressOf<
          NativeFunction<Void Function(Pointer<HermesABIPreparedJavaScript>)>
        >(hermes_preparedjavascript_release)
        .cast(),
  );

  @internal
  Pointer<HermesABIPreparedJavaScript> get ptr {
    if (_released) {
      throw StateError('JSPreparedJavaScript has already been released.');
    }
    return _ptr;
  }

  /// Releases the prepared JavaScript.
  void release() {
    if (_released) return;
    _finalizer.detach(this);
    hermes_preparedjavascript_release(_ptr);
    _released = true;
  }
}
