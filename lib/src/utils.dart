import 'dart:ffi';
import 'package:hermes_dart/hermes_dart.g.dart';

/// Extension helpers on native structures.
extension HermesABIStringDataExt on HermesABIStringData {
  /// Converts the native string data to a Dart [String] without copying.
  String toDartString() {
    if (data == nullptr || length == 0) return '';
    if (is_ascii) {
      return String.fromCharCodes(
        data.cast<Uint8>().asTypedList(length),
      );
    } else {
      return String.fromCharCodes(
        data.cast<Uint16>().asTypedList(length),
      );
    }
  }
}
