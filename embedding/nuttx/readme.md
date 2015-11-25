
### How to build JerryScript for NuttX on STM32F4-Discovery w/BB

#### 1. Preface

1, Directory structure

Assume `harmony` as the root folder to the projects to build.
The tree would look like this.

```
harmony
  + jerryscript
  |  + embedding
  |      + nuttx
  + nuttx
     + nuttx
         + lib
```

2, Target board

Assume [STM32F4-Discovery with BB](http://www.st.com/web/en/catalog/tools/FM116/SC959/SS1532/LN1199/PF255417)
as the target board. Or may use any SD-Card reader that can be accessed with the file system but
you may have to disable network and enabling that SD-Card by your own.


3, Micro SD-Card memory for Script source files


#### 2. Prepare NuttX

Follow [this](https://bitbucket.org/seanshpark/nuttx/wiki/Home) page to get
NuttX source and prepare for build.

If you finished the first build, change default project from
`IoT.js` to `JerryScript` as follows


2-1) run menuconfig
```
cd nuttx/nuttx
make menuconfig
```
2-2) Select `Application Configuration` --> `Interpreters`

2-3) Check `[*] JerryScript interpreter` (Move cursor to the line and press `Space`)

2-4) `< Exit >` once on the bottom of the sceen (Press `Right arrow` and `Enter`)

2-5) Select `System Libraries and NSH Add-Ons`

2-6) Un-Check `[ ] iotjs program`

2-7) `< Exit >` till `menuconfig` ends. Save new configugation when asked.

2-7) `make` again
```
make
```

You may see this error, please continue below.

```
make: *** No rule to make target `lib/libfdlibm.a', needed by `pass2deps'.
Stop.
```

#### 3. Build JerryScript for NuttX

```
cd jerryscript
make -f ./embedding/nuttx/Makefile.nuttx
```

If you have nuttx at another path than described above, please give the absolute path
with `NUTTX` variable, for example,
```
NUTTX=/home/user/work/nuttx make -f ./embedding/nuttx/Makefile.nuttx
```

To clean the build result
```
make -f ./embedding/nuttx/Makefile.nuttx clean
```

Make will copy three library files to `nuttx/nuttx/lib` folder
```
libjerryentry.a
libjerrycore.a
libfdlibm.a
```

In nuttx, sometimes you may want to clean the build. When you do this
above library files are also removed so may have to build jerry again.


#### 4. Flashing

Connect Mini-USB for power supply and connect Micro-USB for `NSH` console.

Please refer [this]
(https://github.com/Samsung/iotjs/wiki/Build-for-NuttX#prepare-flashing-to-target-board) link.


### Running JerryScript

Prepare a micro SD-card and prepare `hello.js` like this

```
print("Hello JerryScript!");
```

Power Off(unplug both USB cables), plug the memory card to BB, and Power on again.

I use `minicom` for terminal program.

```
minicom --device=/dev/ttyACM0 --baud=115200
```


You may have to press `RESET` on the board and press `Enter` keys on the console
several times to make `nsh` prompt to appear.
```
NuttShell (NSH)
nsh>
nsh>
nsh> jerryscript /mnt/sdcard/hello.js
PARAM 1 : [/mnt/sdcard/hello.js]
Hello JerryScript!
```

Please give absolute path of the script file or may get an error.
```
nsh> cd /mnt/sdcard
nsh> jerryscript ./hello.js
PARAM 1 : [./hello.js]
Failed to fopen [./hello.js]
JERRY_STANDALONE_EXIT_CODE_FAIL
nsh>
nsh>
nsh> jerryscript /mnt/sdcard/hello.js
PARAM 1 : [/mnt/sdcard/hello.js]
Hello JerryScript!
```
