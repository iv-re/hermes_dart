// ignore_for_file: avoid_redundant_argument_values

import 'package:hermes_dart/hermes_dart.dart';
import 'package:test/test.dart';

void main() {
  group('JSRuntimeConfig', () {
    test('hardened config disables eval and proxy', () {
      const config = JSRuntimeConfig.hardened();
      final rt = JSRuntime.create(config);
      try {
        // eval should fail and throw an exception
        expect(
          () => rt.evaluateJavascript('eval("1 + 1")'),
          throwsA(anything),
        );

        // Proxy should be undefined
        final proxyVal = rt.evaluateJavascript('typeof Proxy');
        expect(proxyVal.asDartString, equals('undefined'));
      } finally {
        rt.release();
      }
    });

    test('custom config enables eval and proxy', () {
      const config = JSRuntimeConfig(
        enableEval: true,
        es6Proxy: true,
      );
      final rt = JSRuntime.create(config);
      try {
        // eval should succeed
        final res = rt.evaluateJavascript('eval("1 + 2")');
        expect(res.asNumber, equals(3.0));

        // Proxy should be available
        final proxyVal = rt.evaluateJavascript('typeof Proxy');
        expect(proxyVal.asDartString, equals('function'));
      } finally {
        rt.release();
      }
    });

    test('generator configuration test', () {
      final rtHard = JSRuntime.create(const JSRuntimeConfig.hardened());
      final rtCustom = JSRuntime.create(
        const JSRuntimeConfig(
          enableGenerator: true,
        ),
      );
      try {
        expect(
          () => rtHard.evaluateJavascript('function* foo() {}'),
          throwsA(anyOf(isA<JSException>(), isA<JSNativeException>())),
        );
        expect(
          rtCustom.evaluateJavascript('function* foo() {}').isUndefined,
          isTrue,
        );
      } finally {
        rtHard.release();
        rtCustom.release();
      }
    });

    test('HermesInternal test methods configuration', () {
      final rtHard = JSRuntime.create(const JSRuntimeConfig.hardened());
      final rtCustom = JSRuntime.create(
        const JSRuntimeConfig(
          enableHermesInternal: true,
          enableHermesInternalTestMethods: true,
        ),
      );
      try {
        expect(
          rtHard
              .evaluateJavascript('typeof HermesInternal.detachWeak')
              .asString
              .string,
          equals('undefined'),
        );
        expect(
          rtHard
              .evaluateJavascript('typeof HermesInternal.getCallStack')
              .asString
              .string,
          equals('undefined'),
        );
        expect(
          rtCustom
              .evaluateJavascript('typeof HermesInternal.getCallStack')
              .asString
              .string,
          equals('function'),
        );
      } finally {
        rtHard.release();
        rtCustom.release();
      }
    });

    test('compilation mode is applied to source compilation', () {
      final rtEager = JSRuntime.create(
        const JSRuntimeConfig(
          compilationMode: .forceEager,
          enableHermesInternal: true,
          enableHermesInternalTestMethods: true,
        ),
      );

      final rtLazy = JSRuntime.create(
        const JSRuntimeConfig(
          compilationMode: .forceLazy,
          enableHermesInternal: true,
          enableHermesInternalTestMethods: true,
        ),
      );

      const script = '''
        function foo() {};
        HermesInternal.isLazy(foo)
      ''';

      try {
        expect(rtEager.evaluateJavascript(script).asBoolean, isFalse);
        expect(rtLazy.evaluateJavascript(script).asBoolean, isTrue);
      } finally {
        rtEager.release();
        rtLazy.release();
      }
    });
  });
}
