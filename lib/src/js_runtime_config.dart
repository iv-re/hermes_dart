enum JSCompilationMode {
  smart(0),
  forceEager(1),
  forceLazy(2)
  ;

  const JSCompilationMode(this.value);

  final int value;
}

class JSRuntimeConfig {
  const JSRuntimeConfig({
    this.enableEval = true,
    this.es6Proxy = true,
    this.enableGenerator = true,
    this.enableAsyncGenerators = false,
    this.es6BlockScoping = false,
    this.intl = true,
    this.microtaskQueue = false,
    this.compilationMode = JSCompilationMode.smart,
    this.bytecodeWarmupPercent = 0,
    this.optimizedEval = false,
    this.enableHermesInternal = true,
    this.enableHermesInternalTestMethods = false,
    this.randomizeMemoryLayout = false,
  });

  const JSRuntimeConfig.hardened()
    : enableEval = false,
      es6Proxy = false,
      enableGenerator = false,
      enableAsyncGenerators = false,
      es6BlockScoping = false,
      intl = true,
      microtaskQueue = false,
      compilationMode = JSCompilationMode.smart,
      bytecodeWarmupPercent = 0,
      optimizedEval = false,
      enableHermesInternal = false,
      enableHermesInternalTestMethods = false,
      randomizeMemoryLayout = true;

  final bool enableEval;
  final bool es6Proxy;
  final bool enableGenerator;
  final bool enableAsyncGenerators;
  final bool es6BlockScoping;
  final bool intl;
  final bool microtaskQueue;
  final JSCompilationMode compilationMode;
  final int bytecodeWarmupPercent;
  final bool optimizedEval;
  final bool enableHermesInternal;
  final bool enableHermesInternalTestMethods;
  final bool randomizeMemoryLayout;
}
