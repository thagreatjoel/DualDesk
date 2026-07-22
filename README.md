<div align="center">

# DUALDESK

**Two Workspaces. Two Mice. One PC.**

</div>

<div align="center">

A native Windows application that turns one computer with two monitors into two separate workspaces. Two people can work at the same time using their own keyboard, mouse, and cursor.

</div>

<br>

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue.svg)](https://www.microsoft.com/en-us/windows)
[![Release](https://img.shields.io/badge/Release-v1.0.0-brightgreen.svg)](https://github.com/yourusername/DualDesk/releases)

</div>

---

## WHAT IS DUALDESK?

Windows lets you plug in multiple mice and keyboards, but they all fight for the same cursor and the same window focus. You cannot have two people using one PC independently.

DualDesk fixes this. It gives each monitor its own workspace. Each workspace gets its own assigned mouse and keyboard. The cursors are separate. The windows stay where they belong.

---

## WHY THIS EXISTS

- Two developers can code side by side on one machine
- A teacher and student can work together
- Two people can share a powerful PC instead of buying two
- No need for expensive multi-seat software

---

## WHAT YOU NEED

| Requirement | Minimum |
|-------------|---------|
| Windows | 10 or 11 (64-bit only) |
| RAM | 4 GB |
| Free space | 100 MB |
| Monitors | 2 |
| Mice | 2 |
| Keyboards | 2 |
| Admin rights | Yes |
| Test mode | Yes (for driver) |

---

## THINGS TO KNOW BEFORE INSTALLING

> [!WARNING]
> **Windows Has Only One Cursor**
> This is a Windows limitation. To get two cursors, you need EitherMouse. It is free and the installer is included.

> [!WARNING]
> **Driver Is Not Signed**
> The driver file (dualdesk.sys) is not certified by Microsoft. Certification costs money and takes months. You need to turn on Test Mode for it to load.

> [!WARNING]
> **Test Mode Required**
> Run this command as Administrator and reboot:
> ```
> bcdedit /set testsigning on
> ```

> [!WARNING]
> **Antivirus May Complain**
> Some antivirus programs see unsigned drivers as threats. This is a false alarm. Add the DualDesk folder to your antivirus exceptions.

> [!WARNING]
> **SmartScreen Will Block It**
> Windows will show a warning. Click "More info" then "Run anyway".

> [!WARNING]
> **64-bit Only**
> This does not work on 32-bit or ARM versions of Windows.

---

## HOW TO INSTALL

1. Download and extract the zip file
2. Right-click DualDesk.exe, go to Properties, check Unblock, click Apply
3. Open Command Prompt as Administrator and run:
bcdedit /set testsigning on

text
4. Restart your computer
5. Run EitherMouse Setup.exe (this is free and gives you two cursors)
6. Run install.bat as Administrator (this installs the driver)
7. Run DualDesk.exe as Administrator

---

## HOW TO USE

1. Plug in both monitors, both mice, and both keyboards
2. Open DualDesk
3. Right-click a workspace card (Workspace A or Workspace B)
4. Click Assign Device
5. Go to the Devices tab
6. Click Assign next to the mouse you want for that workspace
7. Repeat for the second mouse
8. Start using your separate workspaces

Each mouse will stay on its own monitor. Red borders show where each workspace begins and ends.

---

## SHORTCUTS

| Key | What it does |
|-----|--------------|
| R | Reset cursors to center |
| C | Change cursor style |
| T | Turn trails on/off |
| ESC | Close DualDesk |

---

## IF SOMETHING GOES WRONG

> [!NOTE]
> **Driver not connected**
> Open Command Prompt as Administrator and run:
> ```
> sc query DualDesk
> sc start DualDesk
> ```

> [!NOTE]
> **Driver won't start**
> Test mode might not be on. Run:
> ```
> bcdedit /set testsigning on
> ```
> Restart your computer.

> [!NOTE]
> **Antivirus blocked the driver**
> Add the entire DualDesk folder to your antivirus exceptions.

> [!NOTE]
> **SmartScreen blocked the app**
> Click "More info" then "Run anyway".

> [!NOTE]
> **No red borders showing**
> Make sure you have two or more monitors connected.

> [!NOTE]
> **Cursors still cross monitors**
> Make sure each mouse is assigned to a workspace.

---

## HOW TO UNINSTALL

Run uninstall.bat as Administrator. It will stop the driver and remove it. After that, you can delete the DualDesk folder.

---

## LICENSE

MIT License. You can use, modify, and share this software freely.

---

<div align="center">

Made with C++ by the DualDesk Team

</div>
