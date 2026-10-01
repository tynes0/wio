# Wio Native Playground

This is the repository's scratch native Wio application.

- Edit `wio/main.wio` for Wio code.
- Add native declarations under `native/include`.
- Add native implementations under `native/src` and list new source files in
  `wio.makewio`.
- Select the `wio_playground` CMake target in Rider or Visual Studio and press
  Play to build and run the project through the current Wio compiler.

Generated output stays under `.wio-build` and is not added to the solution.
