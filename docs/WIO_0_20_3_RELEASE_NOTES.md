# Wio 0.20.3 native adapter requirements

Wio 0.20.3 gives native VM adapters an inspectable, versioned requirements
identity. It separates ordinary VM function bodies from the inputs that the
native bridge actually compiles. It does not enable adapter cache reuse.

## Generate a report

Native-backed project VM builds write a report beside the bridge:

```sh
wio project build --backend vm
wio project describe
```

The JSON description exposes `vm.nativeRequirements`. The report is produced
when the project bridge is compiled, not by an analysis-only check. A
standard-covered executable continues to use the precompiled standard producer
and does not need a project bridge report.

For direct inspection without invoking a C++ compiler:

```sh
wio main.wio --emit-bytecode --bytecode-output app.wiob \
  --native-requirements-output native.requirements.json
```

The same output option is accepted by canonical `--native-bridge-only` builds.
It is rejected for unrelated emit modes and legacy backend builds. The report
path must be distinct from the source, compiler and selected backend outputs.
Failed analysis or native compilation does not replace an existing report.

## Identity scope

The existing native bridge planner selects the dependency closure. A private
normalization view removes sparse type/function/global numbering and source
spans, while retaining ordered fields, ABI passing/marshalling, callback
contracts, native symbols/headers, generic instantiations, dispatch/lifecycle,
retained reflection metadata and bodies required by the C++ producer. Canonical
wire serialization and dynamic Any payload requirements feed a schema-tagged
SHA-256 digest. The original Lowered WIR and generated C++ are not rewritten.

Ordinary Wio entry/helper body edits do not change the identity when their
native requirements stay the same. A new native binding, layout or Any payload
does. Wio code compiled into the adapter, such as retained constructors,
field initializers and native-object methods, also changes the identity when
its implementation changes. Runtime-retained metadata remains part of the
contract and can conservatively invalidate an identity.

The report lists native binding keys, retained type keys, dynamic Any payloads,
embedded Wio functions and native imports. Its `schemaVersion` is 1 and its
`algorithm` is `sha256`.

## Cache boundary

Every report declares `cacheReuseEnabled: false` and
`dependencyClosureIncluded: false`. The digest does not cover native file
contents, transitive includes, compiler/archiver identity, target flags or the
generator/runtime implementation. It is neither a complete artifact cache key
nor a cryptographic authenticity guarantee. Current bridge rebuild behavior is
preserved, including rebuilds after Wio-only source edits.

Patch 0.20.4 adds native dependency validity; patch 0.20.5 integrates safe adapter
reuse. Neither gate is closed by the presence of this report. Heavy runtime and
compile-time optimization remain in the later approved release families.

Product metadata aligns at 0.20.3. Module ABI descriptor 11, bytecode format 1.1,
the default native backend and the separate SDK feature-parity gate are unchanged.
