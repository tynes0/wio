# Atlas Desk validation

The release validation path is:

1. build the project with the packaged Wio 0.11 toolchain;
2. run with `WIO_ATLAS_SMOKE=1`;
3. require a zero exit code after asynchronous indexing;
4. require `.wio-build/atlas-desk-smoke.png` to be produced;
5. inspect the image for the populated dashboard rather than an empty frame.

The system's close handler also cancels an unfinished scan, so the smoke path
qualifies both normal task completion and orderly application shutdown.
