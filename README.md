# GUPPI RAW C99 Library

This library support ingesting and creating GUPPI RAW files. Such files have one or more blocks, which are pairs of FITS formatted headers (80 character entries) followed by binary data.

```
 block         	!HEADER!___________________________________1___________________________________!
 aspect        	       !_________________1_________________!_________________2_________________!
 channel       	       !_____1_____!_____2_____!_____3_____!_____1_____!_____2_____!_____3_____!
 time          	       !_1_!_2_!_3_!_1_!_2_!_3_!_1_!_2_!_3_!_1_!_2_!_3_!_1_!_2_!_3_!_1_!_2_!_3_!
 polarization  	       !1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!1!2!
 complex samples       !SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS!
```

:

There is only one critical header-entry, the ommission of which results in an error, while further header-entries required to specify the block's shape, but have default values:

Key 		 | Value | Default
---------|-|-
BLOCSIZE | The block's data byte-size. `integer` | **Critical Requirement**
OBSNCHAN | Total number of channel, across all aspects. `integer > 0` | 1
NANTS    | Number of antenna-aspects. `integer > 0` | 1
NBEAMS   | Number of beam-aspects (overrules NANTS). `integer > 0` | 0
NPOL     | Number of polarizations. `integer > 0` | 1
NBITS    | Number of bits per complex-component. `integer > 0` | 4
DIRECTIO | Header and Data are DIRECTIO (512) aligned with padding, or not. `integer [0, 1]` | 0


**Key features are**:
- multi-aspect files (antenna or beams)
- iteration of the data in user-defined, dynamic `[n_aspect, n_chan, n_time]` chunks
- automatic O_DIRECT inference based on DIRECTIO
- standardised calculation of various block's data-shape values
- header parsing in a single-pass
- user-callback for header entry parsing

## Compilation

1. Use meson and ninja:

	`$ pip install meson ninja`

2. Configure the build directory:

	DIRECTIO support | Configuration command
	-|-
	No | `$ meson build -Dprefix=/usr/local`
	Yes | `$ meson build -Dprefix=/usr/local -Dc_args=-D_GNU_SOURCE`

3. Compile (and test) (and install)

	```
	       $ cd build
	build/ $ ninja && ninja test && ninja install
	```

## Benchmarks

Both the [`write`](./tests/write.c) and [`iterate`](./tests/iterate.c) test executables accept a `-V` option to instead benchmark writing and read-iterating through files, respectively.

The test rig is an `AMD EPYC 7352 24-Core Processor` machine, with 4 `TOSHIBA MQ01ABD1` NVMe drives in Raid0, `xfs` formatted.
It measured **roughly 8.7 GB/s when writing a 33 GB DIRECTIO file**.

The read-iterate benchmark is more complicated, owing to the fact that iteration-datashapes are configurable. The benchmark readig the is captured in [text](./benchmarks/iterate.txt) and further plotted below.
- Iteration block-shapes with a time-dimension less than that of the blocks in the file have a drastically lower throughput.  
- Iteration block-shapes with a time-dimension greater than or equal to that of the blocks in the file have a stable throughput between **8 and 10 GB/s** with DIRECTIO.

![Read-Iterate Benchmarks Plot](./benchmarks/iterate_benchmark.png)


*There is also a [`key_comparison`](./tests/key_comparison.c) executable generated that measures the current key-comparison operation to be the fastest.*