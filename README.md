# Module Builder

A lightweight Unreal Engine editor tool that allows you to quickly create and register new C++ modules for your Project or Project Plugins.

---

## What It Does

This plugin automates the process of creating a new C++ module by:

- Generating the correct folder structure
- Creating Build.cs
- Creating Module header and cpp
- Updating .uproject or .uplugin automatically

---

## Installation

1. Download the release ZIP
2. Extract the folder into:

YourProject/Plugins/

3. Open Unreal Editor
4. Enable the plugin if needed

---

## Usage

1. Open Unreal Editor
2. Go to:

Tools → Module Builder

3. Select target (Project or Plugin)
4. Enter module name
5. Click Confirm

After creation:

- Right-click .uproject → Generate Project Files
- Compile once

---

## Tested Version

Unreal Engine 5.6
Visual Studio 2022

---

## Author

Zhihao Wang
