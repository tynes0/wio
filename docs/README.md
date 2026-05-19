# Wio Documentation Index

This directory is the scalable documentation entrypoint for Wio.

The goal is simple:

- keep the main docs easy to discover,
- avoid turning one file into an unmaintainable wall of text,
- and make it obvious where new material should live as the language grows.

Current `v1` tooling reading:

- start with the CLI,
- treat `wio.makewio` as the canonical project manifest,
- use `scripts/wio/*.wio` for source-based helpers,
- and treat PowerShell scripts as compatibility wrappers.

## Start Here

- [Project system](C:/Users/cihan/RiderProjects/wio/docs/WIO_PROJECT_SYSTEM.md)
- [v1 freeze snapshot](C:/Users/cihan/RiderProjects/wio/docs/WIO_V1_FREEZE.md)

If you are trying to build, package, install, or run Wio projects, start with
the project-system document first.

## Core Language

- [Language reference](C:/Users/cihan/RiderProjects/wio/docs/WIO_LANGUAGE_DRAFT.md)
- [Runtime type model](C:/Users/cihan/RiderProjects/wio/docs/WIO_RUNTIME_TYPE_MODEL.md)

This is where syntax, semantics, references, generics, operator behavior, and
runtime-facing type rules belong.

## Standard Library And SDK

- [Standard library](C:/Users/cihan/RiderProjects/wio/docs/WIO_STD.md)
- [Host SDK](C:/Users/cihan/RiderProjects/wio/docs/WIO_SDK.md)

Use these when the topic is no longer “what does the language mean?” and has
become “how do I use the shipped surface?”

## Release / Quality Tracking

- [v1 freeze snapshot](C:/Users/cihan/RiderProjects/wio/docs/WIO_V1_FREEZE.md)
- [Traceability](C:/Users/cihan/RiderProjects/wio/docs/WIO_TRACEABILITY.md)

These documents should stay smaller and more directional than the main
reference.

## Documentation Growth Policy

As Wio grows, we should keep this split:

- `README.md` at repo root: fast orientation and common commands.
- `docs/README.md`: documentation map and navigation.
- `WIO_LANGUAGE_DRAFT.md`: canonical language surface.
- `WIO_PROJECT_SYSTEM.md`: CLI, manifests, package, install, and build flow.
- `WIO_STD.md`: shipped standard-library surface.
- `WIO_SDK.md`: host-facing integration surface.
- focused companion docs when one topic gets too large.

When a topic starts bloating one document, the preferred move is:

1. create a focused companion document,
2. keep the parent doc as the overview and contract,
3. link the detailed material from this index.

That gives us room to grow without making the docs harder to navigate.
