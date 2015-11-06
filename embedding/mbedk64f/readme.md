### About

This folder contains files to run JerryScript in mbed / FRDM-K64Fboard.

#### How to build JerryScript
cd to JerryScript root folder where `Makefile` exist

```
make -f embedding/mbedk64f/Makefile.mbedk64f clean
make -f embedding/mbedk64f/Makefile.mbedk64f
```

#### Output files

Two files will be generated at `embedding/mbedk64f/libjerry`
* libjerrycore.a
* libfdlibm.a

#### Building mbed

```
make -f embedding/mbedk64f/Makefile.mbedk64f yotta
```

Or, cd to `embedding/mbedk64f` where `Makefile.mbedk64f` exist

```
cd embedding/mbedk64f
yotta target frdm-k64f-gcc
yotta build
```

#### Flashing to k64f

Copy `embedding/mbedk64f/build/frdm-k64f-gcc/source/mbedk64f.bin` file to mounted folder. For example,

```
cp embedding/mbedk64f/build/frdm-k64f-gcc/source/mbedk64f.bin /media/user/MBED/.
```

When LED near the USB port flashing stops, press RESET button on the board.
