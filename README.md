A simple Proof-of-Work scheme based on the subset-sum-problem

# Installation

```
git clone --recursive git@github.com:nanocurrency/ssp-pow.git
```

## Dependencies

* boost >=1.67


## Build

```
mkdir build
cd build
cmake ..
make
./ssp_pow_driver --help
```
## Run benchmark

```
./ssp_pow_driver --driver cpp --threads 16
```

# Algorithms
Description of Algortihms

## cpp_driver.cpp
```
build$ ./ssp_pow_driver --driver cpp --threads 16
Initializing...
Starting...
cdf4a3e0=H0(022e8719)+320b5c20=H1(01bb40ea)=0340000000000000 solution ms: 11388
e7032b4d=H0(01f62747)+18fcd4b3=H1(01853b90)=e230000000000000 solution ms: 10531
bc93388c=H0(0083cb74)+436cc774=H1(023e7644)=1440000000000000 solution ms: 6680
0fa63119=H0(0302cae0)+f059cee7=H1(0328f5f5)=7c20000000000000 solution ms: 15450
d9c9381f=H0(09a4c1f1)+2636c7e1=H1(025d6c04)=3f10000000000000 solution ms: 36286
1bdf31c1=H0(01e33156)+e420ce3f=H1(02bc8433)=67e0000000000000 solution ms: 12187
a798b35f=H0(024df0bd)+58674ca1=H1(018727a2)=4ba0000000000000 solution ms: 13734
3b409164=H0(02beff6c)+c4bf6e9c=H1(00d237ab)=b520000000000000 solution ms: 14783
df8cba66=H0(08a4a8a4)+2073459a=H1(023fd452)=2160000000000000 solution ms: 34043
fc5b6204=H0(0055a2c0)+03a49dfc=H1(03c90c96)=b910000000000000 solution ms: 7402
e172e6f7=H0(0173ce5a)+1e8d1909=H1(02a2faa0)=0830000000000000 solution ms: 10805
5b3b0535=H0(03cc5b92)+a4c4facb=H1(02e75ea8)=3c40000000000000 solution ms: 18306
b6b7f976=H0(07d30a5f)+4948068a=H1(0119a636)=0690000000000000 solution ms: 31406
f68c4c74=H0(024fd102)+0973b38c=H1(005f435e)=36b0000000000000 solution ms: 13579
02d7a8c1=H0(0c3ff368)+fd28573f=H1(02b73045)=ce80000000000000 solution ms: 45393
4ffbbed4=H0(01473351)+b004412c=H1(00f951d5)=a030000000000000 solution ms: 10465
Average solution time: 18277
```

# Performance
The following
*  i7-8550U CPU @ 1.80GHz × 8

Configuration paramters

* 4 threads
* 16 Nonces
* 

|         Driver | Average Solution Time (mS) | Memory (MB)| Commit Hash|
-------------------------------------------------------------------------
| cpp_driver.cpp | 18277                      |      260M  | 419678c2   |
|
