import 'dart:convert';
import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:convert/convert.dart';
import 'package:crypto/crypto.dart';
import 'package:hooks/hooks.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;

    final configuration = _getConfiguration(input);
    final dylibName = _getLibName(input.config.code.targetOS);

    final localBuildDir = input.packageRoot.resolve('build_$configuration/');

    var localLib = File.fromUri(localBuildDir.resolve(dylibName));
    if (!localLib.existsSync()) {
      localLib = File.fromUri(
        localBuildDir.resolve('Release/$dylibName'),
      );
    }

    final Uri dylib;

    if (localLib.existsSync()) {
      dylib = localLib.uri;
    } else {
      dylib = await _downloadPrebuiltBinary(
        input: input,
        configuration: configuration,
        dylibName: dylibName,
      );
    }

    output.assets.code.add(
      CodeAsset(
        package: input.packageName,
        name: '${input.packageName}.dart',
        linkMode: DynamicLoadingBundled(),
        file: dylib,
      ),
    );
  });
}

const _releaseBaseUrl =
    'https://github.com/iv-re/hermes_dart/releases/download';

Future<Uri> _downloadPrebuiltBinary({
  required BuildInput input,
  required String configuration,
  required String dylibName,
}) async {
  String readFile(String packageRelativePath) {
    final fileUri = input.packageRoot.resolve(packageRelativePath);
    return File.fromUri(fileUri).readAsStringSync();
  }

  final hash = readFile('hermes_hash').trim();
  final hashes =
      jsonDecode(readFile('prebuilt_hashes.json')) as Map<String, dynamic>;

  final downloadFileName = '${configuration}_$dylibName';
  final expectedSha256 = hashes[downloadFileName] as String?;
  if (expectedSha256 == null) {
    throw BuildError(message: 'No prebuilt hash found for: $downloadFileName');
  }

  final downloadUrl = Uri.parse(
    '$_releaseBaseUrl/hermes_dart_$hash/$downloadFileName',
  );

  final cacheDir = Directory.fromUri(
    input.outputDirectoryShared.resolve('prebuilt_cache/$hash/'),
  );
  await cacheDir.create(recursive: true);

  final cachedFile = File.fromUri(cacheDir.uri.resolve(dylibName));
  if (cachedFile.existsSync()) {
    return cachedFile.uri;
  }

  final tmpFile = File.fromUri(cacheDir.uri.resolve('$dylibName.tmp'));
  final client = HttpClient();

  try {
    final request = await client
        .getUrl(downloadUrl)
        .timeout(const Duration(seconds: 30));

    final response = await request.close();

    if (response.statusCode != 200) {
      throw BuildError(
        message:
            'Failed to download binary from '
            '$downloadUrl: ${response.statusCode}',
      );
    }

    final digestSink = AccumulatorSink<Digest>();
    final hashSink = sha256.startChunkedConversion(digestSink);
    final fileSink = tmpFile.openWrite();

    await for (final chunk in response) {
      hashSink.add(chunk);
      fileSink.add(chunk);
    }

    hashSink.close();
    await fileSink.flush();
    await fileSink.close();

    final downloadedHash = digestSink.events.single.toString();
    if (downloadedHash != expectedSha256) {
      await tmpFile.delete();
      throw BuildError(
        message:
            'SHA256 hash mismatch. '
            'Expected: $expectedSha256, '
            'Got: $downloadedHash',
      );
    }

    await tmpFile.rename(cachedFile.path);
  } finally {
    client.close();
  }

  return cachedFile.uri;
}

String _getConfiguration(BuildInput input) {
  final code = input.config.code;

  final platform = switch (code.targetOS) {
    .macOS => 'macos',
    .windows => 'windows',
    .linux => 'linux',
    // .android => 'android',
    .iOS => code.iOS.targetSdk == IOSSdk.iPhoneSimulator
        ? 'ios_sim'
        : 'ios',
    _ => throw BuildError(
      message: 'Unsupported target OS: ${code.targetOS}',
    ),
  };

  final arch = switch (code.targetArchitecture) {
    .x64 => 'x64',
    .arm64 => 'arm64',
    _ => throw BuildError(
      message:
          'Unsupported target architecture: '
          '${code.targetArchitecture}',
    ),
  };

  return '${platform}_$arch';
}

String _getLibName(OS os) => switch (os) {
  .macOS || .iOS => 'libhermes_dart.dylib',
  .windows => 'hermes_dart.dll',
  _ => 'libhermes_dart.so',
};
