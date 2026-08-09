# Wio Documentation Index

This directory is the main documentation map for Wio.

The current docs strategy is:

- keep the root `README.md` fast and practical
- keep this file as the documentation hub
- keep major areas in dedicated, website-ready companion documents
- keep formal contracts separate from tutorials and troubleshooting

That lets the docs work well both:

- directly inside the repository
- and later as the content base for a docs website if we want to ship one

---

## 1. Start Here

If you are brand new to Wio, read in this order:

1. [Getting started](./WIO_GETTING_STARTED.md)
2. [CLI reference](./WIO_CLI_REFERENCE.md)
3. [Project system](./WIO_PROJECT_SYSTEM.md)
4. [v1 freeze snapshot](./WIO_V1_FREEZE.md)

That path gives you:

- first install/build steps
- first commands
- first project flow
- the current `v1` stability reading

---

## 2. Tutorials And Practical Guides

- [Getting started](./WIO_GETTING_STARTED.md)
- [Interop guide](./WIO_INTEROP_GUIDE.md)
- [Examples guide](./WIO_EXAMPLES.md)
- [Troubleshooting](./WIO_TROUBLESHOOTING.md)
- [FAQ](./WIO_FAQ.md)

These documents answer practical workflow questions such as:

- how do I get Wio running?
- how do I call native C++?
- how does a host call Wio?
- which example should I open first?
- where do generated files go?
- why does Wio make certain product/design choices?

---

## 3. Reference Documents

### 3.1 Language And Runtime

- [Wio 0.8 nullability and lifetime specification](./spec/WIO_LANGUAGE_SPEC_0_8.md)
- [Wio 0.9 generics and constraints specification](./spec/WIO_LANGUAGE_SPEC_0_9.md)
- [Wio 0.10 const generics and native components specification](./spec/WIO_LANGUAGE_SPEC_0_10.md)
- [Wio 0.11 attributes, matching, and applications specification](./spec/WIO_LANGUAGE_SPEC_0_11.md)
- [Wio standard library contract 0.11](./spec/WIO_STD_SPEC_0_11.md)
- [Language reference](./WIO_LANGUAGE_DRAFT.md)
- [Language evolution plan](./WIO_LANGUAGE_EVOLUTION_PLAN.md)
- [Reference and view lifetimes](./REFERENCE_LIFETIMES.md)
- [Runtime type model](./WIO_RUNTIME_TYPE_MODEL.md)

Use these when the question is about:

- syntax
- semantics
- `ref` / `view` / `deref`
- generics and packs
- object/component/interface meaning
- runtime type boundaries such as `any`, `Box`, and `opaque`
- proposed application/system semantics and future syntax decisions

### 3.2 Tooling And Project System

- [CLI reference](./WIO_CLI_REFERENCE.md)
- [Project system](./WIO_PROJECT_SYSTEM.md)
- [Self-hosted CLI architecture](./WIO_SELF_HOSTED_CLI.md)

Use these when the question is about:

- commands
- manifests
- build/run/package/install behavior
- CLI bootstrap and Wio/Argonaut-Wio migration
- cache/output policy
- packaged toolchain usage

### 3.3 Standard Library And SDK

- [Standard library](./WIO_STD.md)
- [Host SDK](./WIO_SDK.md)

Use these when the question is about:

- what std ships
- what is stable vs experimental
- exported object/component behavior
- enum/flagset wrappers
- dynamic host reflection
- hot reload behavior

---

## 4. Release, Quality, And Compatibility

- [v1 freeze snapshot](./WIO_V1_FREEZE.md)
- [Versioning and compatibility](./WIO_COMPATIBILITY.md)
- [Performance and memory notes](./WIO_PERFORMANCE.md)
- [Traceability](./WIO_TRACEABILITY.md)

These are the "how should I read the product right now?" documents.

Use them when you want to answer:

- is this part of `v1`?
- is this stable or experimental?
- how do I think about performance?
- which tests prove a feature is real?

---

## 5. Documentation Growth Policy

As Wio grows, keep this split:

- `README.md` at repo root:
  fast orientation, common commands, and entry links
- `docs/README.md`:
  navigation and document map
- major contracts:
  language, project system, std, SDK, runtime model, freeze, compatibility
- companion docs:
  getting started, CLI reference, guides, troubleshooting, FAQ, examples

When a document starts trying to do too many jobs at once, prefer this move:

1. create a focused companion document
2. keep the parent document as the overview/contract
3. link the companion document from here

That is the same structure we would want if these docs later move into a
website or Vercel-hosted docs surface.
