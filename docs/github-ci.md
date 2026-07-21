# GitHub CI Packaging

This repository includes a GitHub Actions workflow at `.github/workflows/build.yml`. The workflow builds the same C++20/CMake/Qt codebase on Linux, macOS, and Windows runners, installs Qt for the desktop UI, and uploads platform-specific runnable packages plus installers.

The release layout follows the practical LocalSend pattern: one shared codebase, native OS runners, platform-specific package formats, and consistent asset names.

## Triggering the Workflow

The workflow runs automatically on:

- every push to GitHub
- every pull request
- manual runs from the GitHub Actions tab

To trigger it manually:

1. Open the GitHub repository in a browser.
2. Go to **Actions**.
3. Select the **build** workflow.
4. Click **Run workflow**.
5. Choose the branch, usually `main`, and start the run.

## Downloading Installers

After the workflow finishes:

1. Open the completed workflow run.
2. Scroll to **Artifacts**.
3. Download the artifact for the target platform.

Expected artifacts:

| Artifact | Runner | Installer/package inside artifact |
|----------|--------|-----------------------------------|
| `MyBarrier-linux-x86-64` | `ubuntu-24.04` | `MyBarrier-*-linux-x86-64.deb`, `.tar.gz`, `.zip`, and `.AppImage` |
| `MyBarrier-macos-arm-64` | `macos-15` | `MyBarrier-*-macos-arm-64.dmg` with `MyBarrier.app`, plus archives |
| `MyBarrier-windows-x86-64` | `windows-2022` | `MyBarrier-*-windows-x86-64.exe` and `.zip` |

Each artifact also contains a runnable directory under `dist/` that includes both the CLI and the interactive desktop UI. Use the CPack installer, AppImage, or runnable directory for testing on the deployed platform.

If a platform job fails, the workflow uploads a `*-diagnostics` artifact with `configure.log`, `build.log`, or `package.log` from the failing runner.

## Runtime Logs

The packaged desktop app opens a `Run Log` dock by default on Linux, macOS, and Windows. It records GUI startup, the resolved CLI path, daemon lifecycle output, UI-triggered CLI commands, stdout/stderr, and command exit codes. This makes the current interactive behavior visible without asking users to launch the CLI from a terminal.

## Why CI Is Needed

The Ubuntu development host can build the Linux executable and `.deb` package when Qt is installed locally. macOS installers require the Apple toolchain and DMG tooling from a macOS runner. Windows installers require a Windows executable plus NSIS, which the workflow installs on the Windows runner before running CPack. The workflow installs Qt from Ubuntu packages on Linux so Debian dependency generation can see Qt runtime packages, and uses `jurplel/install-qt-action@v4` on macOS and Windows. Every CI job configures CMake with `-DMYBARRIER_BUILD_GUI=ON`.

Linux AppImage generation is CI-oriented: the workflow installs `libfuse2` and `squashfs-tools`, prepares an `AppDir` via `cmake --install`, then runs `AppImageCrafters/build-appimage` with `packaging/appimage/AppImageBuilder_x86_64.yml`. AppImage packaging is non-blocking so `.deb`, `.tar.gz`, `.zip`, macOS, and Windows artifacts can still upload if AppImage tooling fails.

## Local Packaging Commands

Linux/macOS:

```bash
scripts/package-current.sh
```

Windows PowerShell:

```powershell
.\scripts\package-current.ps1
```

Direct CPack command:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMYBARRIER_BUILD_GUI=ON
cmake --build build --config Release -j
cmake --build build --target package --config Release
```

Linux AppImage command on a Qt-enabled Linux host with `appimage-builder` installed:

```bash
scripts/package-appimage.sh
```
