# DualDesk 
 

## Installation 
since it is not yet certified from the window.. it may show some errors. it is not garenteed that this will perfectly.. this is just a demo version

> [!WARNING]
> ### Driver NOT Micrsoft Certified
> The `dualdesk.sys` driver is **certified by Microsoft** because:
> - Certification costs **500+usd / year**
> - This is a **free/open-source** project


###  Test Mode Required
You MUST enable Test Mode for the driver to load

```xml
Set Test mode
bcdedit /set testsigning on

REBOOT your computer

After installation disable Test Mode:
# bcdedit /set testsigning off
```

> [!WARNING]
> Windows Only Supports ONE Cursor
> Windows is not designed for multiple curzors, here you can install eithermouse as free 


- Download all files
- Unblock the EXE (Properties → Unblock)
- Test Mode (bcdedit /set testsigning on + Reboot)
- Install EitherMouse
- Run install.bat as Administrator
- Run DualDesk.exe as Administrator
