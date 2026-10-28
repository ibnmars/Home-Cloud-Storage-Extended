# Home Cloud Storage - Extended

An addon for [Home Cloud Storage](https://www.nexusmods.com/skyrimspecialedition/mods/102108).

## Features

- **Cloud crafting:** the addon lets you craft with materials kept in
  [Home Cloud Storage](https://www.nexusmods.com/skyrimspecialedition/mods/102108) alongside
  the items you carry. Stored supplies are automatically available at forges,
  workbenches, grindstones, alchemy labs, enchanting tables and cooking pots.
  Your crafting materials can stay in shared storage, so there is no need to
  return home to collect them first.
- Other containers can be connected to cloud crafting through the
  `HCSE_ContainerList` FormList using
  [FormList Manipulator](https://www.nexusmods.com/skyrimspecialedition/mods/74037).
- Integration with [Simplest Horses](https://www.nexusmods.com/skyrimspecialedition/mods/54225)
  (with or without **[Animated Whistling](https://www.nexusmods.com/skyrimspecialedition/mods/76733)**)
  lets you open shared storage
  with **Left Shift + the horse hotkey**, while the normal hotkey
  (**H** by default) opens the saddlebag.

  With [Dynamic Activation Key](https://www.nexusmods.com/skyrimspecialedition/mods/96273),
  its configured modifier is used when available; otherwise, the shortcut uses
  Left Shift.
- Integration with [Oblivion Interaction Icons](https://www.nexusmods.com/skyrimspecialedition/mods/52743)
  adds the standard container interaction icon to the Storage prompt through
  [Dynamic String Distributor](https://www.nexusmods.com/skyrimspecialedition/mods/107676).
- The shared container is renamed from **Cloud Container** to **Storage**
  for a more immersive feel.

## Requirements

- [Home Cloud Storage](https://www.nexusmods.com/skyrimspecialedition/mods/102108)
- [Skyrim Script Extender (SKSE)](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Compatibility

The addon is built with [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG)
and should work on all Skyrim SE, AE and VR versions.

Incompatible with **[Skyrim Crafting Inventory Extender (SCIE)](https://www.nexusmods.com/skyrimspecialedition/mods/170497)**.

## Building from source

Requires Windows, Git, Visual Studio 2022 C++ build tools with a Windows SDK,
CMake 3.23+, Ninja and Python 3.10+. CMake and Ninja can be supplied by the
Visual Studio C++ CMake tools component.

From the repository root in PowerShell:

```powershell
git clone https://github.com/microsoft/vcpkg.git build-tools/vcpkg
git -C build-tools/vcpkg checkout ee12231b20c95013c6638d845d04c91559a1d1ff
./build-tools/vcpkg/bootstrap-vcpkg.bat -disableMetrics
./tools/Build.ps1 -Preset release
```

The build script sets up the Visual Studio environment and builds the pinned
dependencies, using the bundled inputs when building from the Sources archive.
An existing bootstrapped vcpkg checkout can be supplied with
`-VcpkgRoot "path/to/vcpkg"`.

The DLL is written to `build/release/HomeCloudStorageExtended.dll`. Release
builds run the tests before creating the ready-to-install FOMOD archive and matching
Sources archive in `dist/`. Missing dependency inputs are downloaded from
their pinned URLs and checked against the recorded SHA256 hashes.

Use `-Preset debug` for a Debug DLL in `build/debug/`, or `-Preset tests` to
build the independent rules tests without the game libraries. Automatic
release archives are created only by the Release preset.
Use `-RunTests` to run the tests with the Debug or independent tests preset.

## Credits and license

- **[Edryu](https://www.nexusmods.com/skyrimspecialedition/users/12845070)** for
  [Home Cloud Storage](https://www.nexusmods.com/skyrimspecialedition/mods/102108).
- **[ohfor](https://github.com/ohfor)** for [SCIE](https://github.com/ohfor/scie), whose crafting code
  this addon is based on.
- **[alandtse](https://github.com/alandtse) and
  [contributors](https://github.com/alandtse/CommonLibSSE-NG/graphs/contributors)** for
  [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG).
- **[rbtRvlt](https://www.nexusmods.com/skyrimspecialedition/users/3484640)** for
  [Oblivion Interaction Icons](https://www.nexusmods.com/skyrimspecialedition/mods/52743).

Licensed under [GPL-3.0-or-later](LICENSE) with the additional permissions in
[EXCEPTIONS.md](EXCEPTIONS.md). [SCIE](https://github.com/ohfor/scie)-derived code retains its
[MIT notice](licenses/scie/LICENSE.txt); third-party notices are in [licenses](licenses/).
