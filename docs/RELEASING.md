# Releasing TermForms

The `Release` workflow (`.github/workflows/release.yml`) builds an archive
for macOS (Apple Silicon), Linux x86-64 and Windows x64 on every push to
`main`, smoke-tests each (unpacked, its tfc builds and runs
`examples/hello.bas`), and keeps them as the run's artifacts. A tag
`v<version>` also publishes them as a GitHub Release, marked pre-release.

To cut a release:

1. Set the version in all three places, alike:
   - `CMakeLists.txt`: `project(TermForms VERSION x.y.z ...)`
   - `runtime/include/termforms/tf_version.h` (the `runtime.version` test
     fails if it differs)
   - `editors/vscode/package.json`: `"version"`
   Then regenerate the golden files (their header names the version) and
   review that the diff is only that.
2. Run the full suite and the format check; commit to `main`; wait for CI.
3. Tag and push the tag:
   ```sh
   git tag v0.9.0
   git push origin v0.9.0
   ```
   The workflow checks that the tag matches `project(VERSION)`, builds and
   tests the three archives, and publishes the release with them.
4. Check the release page and download one archive as a user would.

Archives: `termforms-<version>-<platform>.tar.gz` (macOS, Linux) and
`.zip` (Windows), laid out by `tools/package.py`, with
`tools/release/README.md` as their README. tfc finds the runtime beside
its `bin`, so an archive works wherever it is unpacked.
Each holds the VS Code extension as `editors/termforms-basic-<version>.vsix`
(`tools/vsix.py`, which needs no Node; the smoke test checks it is
there), its version from `editors/vscode/package.json`.
