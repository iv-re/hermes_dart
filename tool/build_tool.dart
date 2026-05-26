import 'dart:convert';
import 'dart:io';

import 'package:args/command_runner.dart';
import 'package:crypto/crypto.dart';
import 'package:path/path.dart' as p;

void main(List<String> args) async {
  final runner = CommandRunner<void>('hermes_build', 'Hermes Dart build tool')
    ..addCommand(BuildCommand())
    ..addCommand(HashCommand())
    ..addCommand(CollectCommand())
    ..addCommand(ListCommand())
    ..addCommand(TrimCommand());

  try {
    await runner.run(args);
  } catch (e) {
    stderr.writeln(e);
    exit(1);
  }
}

class BuildConfig {
  BuildConfig({required this.name, required this.options});

  final String name;
  final Map<String, dynamic> options;

  String get targetOs => options['target_os'] as String;
  String get targetCpu => options['target_cpu'] as String;
  bool get iosUseSimulator => options['ios_use_simulator'] as bool? ?? false;

  static List<BuildConfig> loadAll() {
    final configFile = File('tool/build_config.json');
    if (!configFile.existsSync()) return [];
    final json = configFile.readAsStringSync();
    final configData = jsonDecode(json) as Map<String, dynamic>;
    return configData.entries
        .map(
          (e) => BuildConfig(
            name: e.key,
            options: e.value as Map<String, dynamic>,
          ),
        )
        .toList();
  }

  static List<BuildConfig> loadLocal() {
    return loadAll().where((config) {
      if (Platform.isMacOS) {
        return config.targetOs == 'macos' || config.targetOs == 'ios';
      }
      if (Platform.isWindows) return config.targetOs == 'windows';
      if (Platform.isLinux) return config.targetOs == 'linux';
      return false;
    }).toList();
  }
}

class BuildCommand extends Command<void> {
  BuildCommand() {
    argParser
      ..addOption(
        'config',
        abbr: 'c',
        help: 'Config name from build_config.json',
        allowed: BuildConfig.loadAll().map((e) => e.name),
      )
      ..addOption(
        'jobs',
        abbr: 'j',
        help: 'Number of parallel build jobs',
        defaultsTo: '4',
      )
      ..addFlag(
        'verbose',
        abbr: 'v',
        help: 'Enable verbose output',
        negatable: false,
      );
  }

  @override
  final String name = 'build';

  @override
  final String description = 'Build Hermes for a specific configuration.';

  @override
  Future<void> run() async {
    final results = argResults;
    final configName = results?['config'] as String?;
    if (configName == null) {
      stdout.writeln('Available configurations:');
      for (final c in BuildConfig.loadLocal()) {
        stdout.writeln('  - ${c.name}');
      }
      throw UsageException('Option --config is required.', usage);
    }

    final allConfigs = BuildConfig.loadAll();
    final config = allConfigs.firstWhere((e) => e.name == configName);

    final buildDir = 'build_${config.name}';
    final extraFlags = <String>[];

    if (config.targetOs == 'macos') {
      final arch = config.targetCpu == 'x64' ? 'x86_64' : 'arm64';
      extraFlags.add('-DCMAKE_OSX_ARCHITECTURES=$arch');

      final hostArch = Process.runSync('uname', [
        '-m',
      ]).stdout.toString().trim();

      if (hostArch != arch) {
        stdout.writeln('--- Cross-compiling for macOS $arch ---');

        final hostConfigName = hostArch == 'x86_64'
            ? 'macos_x64'
            : 'macos_arm64';

        final hostBuildDir = p.join(
          Directory.current.path,
          'build_$hostConfigName',
        );

        final importFile = p.join(hostBuildDir, 'ImportHostCompilers.cmake');

        if (File(importFile).existsSync()) {
          stdout.writeln('--- Using host tools from $importFile ---');
          extraFlags.add('-DIMPORT_HOST_COMPILERS=$importFile');
        } else {
          stdout
            ..writeln(
              '!!! WARNING: ImportHostCompilers.cmake not found '
              'in $hostBuildDir.',
            )
            ..writeln('!!! You should build $hostConfigName first.');
        }
      }
    }
    if (config.targetOs == 'ios') {
      final arch = config.targetCpu == 'x64' ? 'x86_64' : 'arm64';
      final sdkName = config.iosUseSimulator ? 'iphonesimulator' : 'iphoneos';
      final sdkPath = Process.runSync('xcrun', [
        '--sdk',
        sdkName,
        '--show-sdk-path',
      ]).stdout.toString().trim();

      extraFlags.addAll([
        '-DCMAKE_SYSTEM_NAME=iOS',
        '-DCMAKE_OSX_SYSROOT=$sdkPath',
        '-DCMAKE_OSX_ARCHITECTURES=$arch',
        '-DHERMES_APPLE_TARGET_PLATFORM=$sdkPath',
        '-DHERMES_IS_MOBILE_BUILD=ON',
      ]);

      final hostArch = Process.runSync('uname', [
        '-m',
      ]).stdout.toString().trim();

      stdout.writeln('--- Cross-compiling for iOS $arch ---');

      final String hostConfigName;
      if (hostArch == 'x86_64') {
        hostConfigName = 'macos_x64';
      } else {
        hostConfigName = 'macos_arm64';
      }

      final hostBuildDir = p.join(
        Directory.current.path,
        'build_$hostConfigName',
      );

      final importFile = p.join(hostBuildDir, 'ImportHostCompilers.cmake');

      if (File(importFile).existsSync()) {
        stdout.writeln('--- Using host tools from $importFile ---');
        extraFlags.add('-DIMPORT_HOST_COMPILERS=$importFile');
      } else {
        stdout
          ..writeln(
            '!!! WARNING: ImportHostCompilers.cmake not found '
            'in $hostBuildDir.',
          )
          ..writeln('!!! You should build $hostConfigName first.');
      }
    }

    if (config.targetOs == 'windows' && config.targetCpu == 'arm64') {
      extraFlags
        ..add('-A')
        ..add('ARM64')
        ..add('-DBOOST_CONTEXT_IMPLEMENTATION=winfib');

      // Check if we are on x64 host to decide
      // if we need host tools for cross-compilation
      final hostArch = Platform.environment['PROCESSOR_ARCHITECTURE']
          ?.toUpperCase();

      if (hostArch == 'AMD64' || hostArch == 'IA64') {
        // Import host tools from windows_x64 build
        final hostBuildDir = p.join(
          Directory.current.path,
          'build_windows_x64',
        );
        final importFile = p.join(hostBuildDir, 'ImportHostCompilers.cmake');
        if (File(importFile).existsSync()) {
          stdout.writeln('--- Using host tools from $importFile ---');
          extraFlags.add('-DIMPORT_HOST_COMPILERS=$importFile');
        } else {
          stdout
            ..writeln(
              '!!! WARNING: ImportHostCompilers.cmake not found '
              'in $hostBuildDir.',
            )
            ..writeln('!!! You should build windows_x64 first.');
        }
      }
    }

    if (config.targetOs == 'linux' &&
        config.targetCpu == 'arm64' &&
        Platform.isLinux) {
      // Check if we are on x64 host to decide if
      // we need cross-compilation flags
      final hostArch = Process.runSync('uname', [
        '-m',
      ]).stdout.toString().trim();

      if (hostArch != 'aarch64') {
        stdout.writeln('--- Cross-compiling for Linux ARM64 ---');
        extraFlags
          ..add('-DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc')
          ..add('-DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++')
          ..add('-DCMAKE_SYSTEM_NAME=Linux')
          ..add('-DCMAKE_SYSTEM_PROCESSOR=aarch64');

        // Hermes needs host tools to generate files during cross-compilation.
        // We use the ImportHostCompilers.cmake file generated by the x64 build.
        final hostBuildDir = p.join(Directory.current.path, 'build_linux_x64');
        final importFile = p.join(hostBuildDir, 'ImportHostCompilers.cmake');

        if (File(importFile).existsSync()) {
          stdout.writeln('--- Using host tools from $importFile ---');
          extraFlags.add('-DIMPORT_HOST_COMPILERS=$importFile');
        } else {
          stdout
            ..writeln(
              '!!! WARNING: ImportHostCompilers.cmake not found '
              'in $hostBuildDir.',
            )
            ..writeln('!!! You should build linux_x64 first.');
        }
      }
    }

    stdout.writeln('--- Configuring ${config.name} ---');
    final configureProcess = await Process.start('cmake', [
      '-B',
      buildDir,
      '-DCMAKE_BUILD_TYPE=Release',
      ...extraFlags,
    ]);

    configureProcess.stdout.transform(utf8.decoder).listen(stdout.write);
    configureProcess.stderr.transform(utf8.decoder).listen(stderr.write);

    if (await configureProcess.exitCode != 0) {
      throw Exception('CMake configuration failed');
    }

    stdout.writeln('--- Building ${config.name} ---');
    final jobs = results?['jobs'] as String? ?? '4';
    final verbose = results?['verbose'] as bool? ?? false;

    final buildProcess = await Process.start('cmake', [
      '--build',
      buildDir,
      '--config',
      'Release',
      '-j',
      jobs,
      if (verbose) '-v',
    ]);

    // Use non-blocking listeners instead of addStream
    buildProcess.stdout.transform(utf8.decoder).listen(stdout.write);
    buildProcess.stderr.transform(utf8.decoder).listen(stderr.write);

    if (await buildProcess.exitCode != 0) throw Exception('Build failed');

    stdout.writeln('--- Successfully built ${config.name} ---');
  }
}

class HashCommand extends Command<void> {
  HashCommand() {
    argParser.addFlag(
      'verify',
      help: 'Check if current hash matches state',
      negatable: false,
    );
  }

  @override
  final String name = 'hash';

  @override
  final String description = 'Generate or verify the hermes_hash.';

  @override
  void run() {
    final workspaceRoot = Directory.current.path;
    final expectedHash = _generateHash(workspaceRoot);

    if (argResults?['verify'] as bool? ?? false) {
      final hashFile = File('hermes_hash');
      if (!hashFile.existsSync()) throw Exception('hermes_hash file missing');
      final actualHash = hashFile.readAsStringSync().trim();
      if (expectedHash != actualHash) {
        throw Exception(
          'hermes_hash is out of date! Expected: $expectedHash, '
          'Got: $actualHash',
        );
      }
      stdout.writeln('hermes_hash is up to date.');
    } else {
      File('hermes_hash').writeAsStringSync(expectedHash);
      stdout.writeln('Generated hermes_hash: $expectedHash');
    }
  }

  String _generateHash(String root) {
    String gitHash(String obj) {
      final res = Process.runSync(
        'git',
        ['rev-parse', obj],
        workingDirectory: root,
      );
      return res.exitCode == 0 ? (res.stdout as String).trim() : 'unknown';
    }

    final combined =
        '${gitHash('HEAD:hermes')}'
        '${gitHash('HEAD:src')}'
        '${gitHash('HEAD:CMakeLists.txt')}';

    return sha1.convert(utf8.encode(combined)).toString();
  }
}

class CollectCommand extends Command<void> {
  @override
  final String name = 'collect';

  @override
  final String description =
      'Collect all built binaries and update prebuilt_hashes.json.';

  @override
  Future<void> run() async {
    final configs = BuildConfig.loadAll();
    final hashes = <String, String>{};

    final releaseAssetsDir = Directory('release_assets');
    if (releaseAssetsDir.existsSync()) {
      releaseAssetsDir.deleteSync(recursive: true);
    }
    releaseAssetsDir.createSync();

    for (final config in configs) {
      final buildDir = Directory('build_${config.name}');
      if (!buildDir.existsSync()) continue;

      final libName = getLibName(config.targetOs);
      var libFile = File(p.join(buildDir.path, libName));

      if (!libFile.existsSync()) {
        libFile = File(p.join(buildDir.path, 'Release', libName));
      }

      if (libFile.existsSync()) {
        final bytes = await libFile.readAsBytes();
        final hash = sha256.convert(bytes).toString();
        final assetName = '${config.name}_$libName';
        hashes[assetName] = hash;
        await libFile.copy(p.join(releaseAssetsDir.path, assetName));
        stdout.writeln('Collected: $assetName');
      }
    }

    final hashesFile = File('prebuilt_hashes.json');
    const encoder = JsonEncoder.withIndent('  ');
    hashesFile.writeAsStringSync('${encoder.convert(hashes)}\n');

    stdout.writeln(
      'Updated prebuilt_hashes.json with ${hashes.length} entries.',
    );
  }
}

class ListCommand extends Command<void> {
  ListCommand() {
    argParser.addFlag('local', help: 'Only local platforms', negatable: false);
  }

  @override
  final String name = 'list';
  @override
  final String description = 'List configurations.';

  @override
  void run() {
    final results = argResults;
    final configs = (results?['local'] as bool? ?? false)
        ? BuildConfig.loadLocal()
        : BuildConfig.loadAll();

    for (final c in configs) {
      stdout.writeln('${c.name} (${c.targetOs}/${c.targetCpu})');
    }
  }
}

class TrimCommand extends Command<void> {
  @override
  final String name = 'trim';

  @override
  final String description = 'Remove ALL artifacts except the final binary.';

  @override
  Future<void> run() async {
    final configs = BuildConfig.loadAll();
    for (final config in configs) {
      final buildDir = Directory('build_${config.name}');
      if (!buildDir.existsSync()) continue;
      await _trimDirectory(config, buildDir);
    }
    stdout.writeln('--- Aggressive trim complete ---');
  }

  Future<void> _trimDirectory(BuildConfig config, Directory dir) async {
    final libName = getLibName(config.targetOs);
    File? libFile;

    final possiblePaths = [
      p.join(dir.path, libName),
      p.join(dir.path, 'Release', libName),
      p.join(dir.path, 'hermes', 'lib', 'Release', libName),
    ];

    for (final path in possiblePaths) {
      final file = File(path);
      if (file.existsSync()) {
        libFile = file;
        break;
      }
    }

    if (libFile == null) {
      stdout.writeln(
        '!!! Warning: Binary not found in ${dir.path}, skipping trim.',
      );
      return;
    }

    final bytes = await libFile.readAsBytes();

    stdout.writeln('--- Wiping ${dir.path} ---');
    try {
      dir.deleteSync(recursive: true);
    } catch (e) {
      stdout.writeln('!!! Could not delete ${dir.path} entirely: $e');
    }

    dir.createSync(recursive: true);
    final finalFile = File(p.join(dir.path, libName));
    await finalFile.writeAsBytes(bytes);
    stdout.writeln('--- Trimmed to 1 file: ${finalFile.path} ---');
  }
}

String getLibName(String os) {
  return switch (os) {
    'macos' || 'ios' => 'libhermes_dart.dylib',
    'windows' => 'hermes_dart.dll',
    _ => 'libhermes_dart.so',
  };
}
