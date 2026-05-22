import 'dart:io';

import 'package:ffigen/ffigen.dart';
import 'package:logging/logging.dart';

FfiGenerator getConfig(Uri packageRoot) {
  return FfiGenerator(
    functions: Functions.includeAll,
    typedefs: Typedefs.includeAll,
    structs: Structs.includeSet({'HermesABIRuntimeConfig'}),
    output: Output(
      dartFile: packageRoot.resolve('lib/hermes_dart.g.dart'),
      style: const NativeExternalBindings(
        assetId: 'package:hermes_dart/hermes_dart.dart',
      ),
    ),
    headers: Headers(
      entryPoints: [packageRoot.resolve('src/hermes_dart.h')],
      include: (uri) {
        return uri.toFilePath() ==
            packageRoot.resolve('src/hermes_dart.h').toFilePath();
      },
      compilerOptions: [
        ...defaultCompilerOpts(Logger('ffigen')),
        '-I${packageRoot.resolve('hermes/API').toFilePath()}',
        '-I${packageRoot.resolve('hermes/include').toFilePath()}',
        '-I${packageRoot.resolve('src/dart').toFilePath()}',
      ],
    ),
  );
}

void main() {
  getConfig(Platform.script.resolve('../')).generate();
}
