# VRExport Reshade Addon
This is a Reshade addon that aims to export full resolution SBS resources for VR viewers like [VRScreenCap](https://github.com/artumino/VRScreenCap).

It currently supports:
- Frame sequential inputs through the use of a modified version of *3DToElse*, which enables the use of **Flugan's [Geo3D](http://patreon.com/Flugan)**
- **BlueSkyDefender's SuperDepth3D** in its VR version

# Usage
1. Install Reshade with addon support (beware that this version is intended for games without anticheats)
2. Extract the files from the [latest release](https://github.com/artumino/ReshadeVRExport/releases) inside the game's directory (be sure to override 3DToElse)
3. If needed, enable 3DToElse.fx with input format as "Frame Sequential"
4. Open one of the supported viewers

# Note
[KatangaVR](https://store.steampowered.com/app/1127310/HelixVision/) support in DX11 games is currently being worked on.
The pre-release version of [VRScreenCap](https://github.com/artumino/VRScreenCap/releases/tag/0.3.0-dev3) is currently required for DX12 games to work.

# Special Thanks
Special thanks to Flugan for the development of Geo3D.

[BlueSkyDefender](https://github.com/BlueSkyDefender) for the creation of [SuperDepth3D](https://depth3d.info), 3DToElse, and an innumerable number of other awesome shaders.