# CacheARV
Cache based side-channel attack in RISC-V. 

## About 

Graduation project. The repository was tested on a RISC-V processor SiFive m74.

## Usage

```shell
mkdir build
cd build
cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/riscv64-unknown-linux-gnu-gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/riscv64-unknown-linux-gnu-g++ ../ -G "Unix Makefiles"
make
```

## Reference

https://github.com/0xADE1A1DE/Mastik
