### About

This folder contains files to run JerryScript in mbed / FRDM-K64F board.

#### Installing yotta

You need to install yotta before proceeding.
Please visit http://yottadocs.mbed.com/#installing

#### How to build JerryScript

```
make -f embedding/mbedk64f/Makefile.mbedk64f clean
make -f embedding/mbedk64f/Makefile.mbedk64f jerry
```

#### JerryScript output files

Two files will be generated at `embedding/mbedk64f/libjerry`
* libjerrycore.a
* libfdlibm.a

#### Building mbed binary

```
make -f embedding/mbedk64f/Makefile.mbedk64f yotta
```

Or, cd to `embedding/mbedk64f` where `Makefile.mbedk64f` exist

```
cd embedding/mbedk64f
yotta target frdm-k64f-gcc
yotta build
```

#### Build at once

Omit target name to build both jerry library and mbed binary.

```
make -f embedding/mbedk64f/Makefile.mbedk64f
```

#### Flashing to k64f


```
make -f embedding/mbedk64f/Makefile.mbedk64f flash
```

It assumes default mound folder is `/media/(user)/MBED`

If its mounted to other play give it with `TARGET_DIR` variable, for example,
```
TARGET_DIR=/mnt/media/MBED make -f embedding/mbedk64f/Makefile.mbedk64f flash
```

Or copy `embedding/mbedk64f/build/frdm-k64f-gcc/source/jerry.bin` file to mounted folder.

When LED near the USB port flashing stops, press RESET button on the board to execute.
