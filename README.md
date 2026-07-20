# STATE OF THE APPLICATION
My unpatched switch died mid development of fully integrating Pokemon LGPE, so this is where I stop for now. I am now traveling with my wife, and do not have the ability to get a new unpatched switch or attempt to get it fixed currently. Someone can take on this project if they wish. For now it's dead in the water until I either get a new switch or modchip my current patched switch (after traveling).

For those that don't know, this application cannot be worked on or debugged with an emulator or non-hacked switch, it has to painstakingly be built and copied to a hacked switch every build and every debug attempt. It's quite a lot of effort.

---

# **PKSE - Pokemon Save Editor**
PKSE is a homebrew application for conveniently editing Pokemon save files on the Nintendo Switch, without having to transfer save files to your PC.

## **Features**
- Backup and restore save files integrated feature.
- Currently you can edit party and box Pokemon and item amounts.

## **Screenshots**

<img src="https://i.imgur.com/GCwSUz4.jpeg" width="300"><img src="https://i.imgur.com/KGjqY9O.jpeg" width="300">  
<img src="https://i.imgur.com/zDGBAcO.jpeg" width="300"><img src="https://i.imgur.com/teVmLmX.jpeg" width="300">  
<img src="https://i.imgur.com/jbNqmqm.jpeg" width="300"><img src="https://i.imgur.com/YHl5evr.jpeg" width="300">  
<img src="https://i.imgur.com/xnK8G5S.jpeg" width="300">

## **Title Compatibility**
### Generation 7
- Pokemon Let's Go Pikachu/Eevee (Partial implementation, non functioning)

### Generation 8
- Pokemon Sword/Shield (Fully Implemented and functioning)
- Pokemon Brilliant Diamond/Shining Pearl (Not Implemented)
- Pokemon Legends: Arceus (Not Implemented)

### Generation 9
- Pokemon Scarlet/Violet (Not Implemented)
- Pokemon Legends: Z-A (Fully Implemented and functioning)

---

## **Prerequisites**

### 1. Install Required Tools
Ensure the following tools and dependencies are installed:

#### **1.1. devkitPro**
- Download and install [devkitPro](https://devkitpro.org/wiki/Getting_Started).
- Ensure `Switch Development` is selected during installation.

#### **1.2. zlib installation** (Optional, will implement compressed logic in future versions)
- In the MSys2 shell, run ```pacman -S switch-zlib``` to install the zlib for compression support.

---

### 2. Set Up Environmental Variables
Set the `DEVKITPRO` environment variable to the installation path of devkitPro.

#### On Windows:
```bash
setx DEVKITPRO "C:\devkitPro"
```
#### On macOS/Linux:
Add the following line to your shell configuration file (~/.bashrc or ~/.zshrc):
```bash
export DEVKITPRO=/opt/devkitpro
```

Restart your terminal or run the command to apply the changes.

---

### 3. Configure Visual Studio Code

To configure IntelliSense in VS Code:

#### **3.1. Install Extensions**
- C/C++ by Microsoft
- DevkitPro Tools (if available)

#### **3.2. Create a c_cpp_properties.json File**
Create or update the file in .vscode/c_cpp_properties.json with the following content:
```json
{
  "configurations": [
    {
      "name": "Switch",
      "includePath": [
        "${workspaceFolder}/include/**",
        "${workspaceFolder}/src/**",
        "${env:DEVKITPRO}/libnx/include",
        "${env:DEVKITPRO}/portlibs/switch/include", // We should include optional libraries here
        "${env:DEVKITPRO}/devkitA64/aarch64-none-elf/include"
      ],
      "defines": [],
      "compilerPath": "${env:DEVKITPRO}/devkitA64/bin/aarch64-none-elf-g++.exe",
      "cStandard": "c11",
      "cppStandard": "c++20", // This version is necessary
      "intelliSenseMode": "linux-gcc-arm64"
    }
  ],
  "version": 4
}
```

---

## **4. Build the Project**

To build the project, open MSys2 (should have been included with the devkitpro toolset), navigate to the root directory and run:

```bash
make clean && make all
```

This will download all of the necessary sprites and generate an .nro file in the build directory, which you can deploy to your Nintendo Switch.

Or if you don't need to download the sprites (either you don't want to or you've already downloaded them) you can just run:

```bash
make clean && make
```

---

## **Troubleshooting**

### Common Issues

- **`make` not found**:  
  Ensure `make` is installed and in your `PATH`.

- **Undefined references**:  
  Verify that your `includePath` is correctly configured in `c_cpp_properties.json`.

- **libnx-related errors**:  
  Ensure `libnx` is properly installed and that `DEVKITPRO` is set correctly.

- **Permission issues on Windows**:  
  Run VS Code or your terminal as Administrator if file access errors occur.

---

## **Credits**

- PKHeX Team: core save editing logic are derived from the PKHeX project. Visit their official repository: https://github.com/kwsch/PKHeX.
- PokeAPI Team: for their work on sprites: https://github.com/PokeAPI/sprites
- libnx and devkitPro communities for Switch homebrew development tools. Visit their official website: https://devkitpro.org/wiki/Getting_Started.

## **License**

This project is licensed under the [GNU Affero General Public License v3.0](LICENSE). See `LICENSE` for details.

