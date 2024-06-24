An example of **reading headers** and **playing** a simple **wave file** using the [`Windows Multimedia API`](https://learn.microsoft.com/en-us/windows/win32/api/_multimedia/). This is meant to **illustrate**
how a simple wave file (**PCM**) player would work. It **might fail** on more **complex files** with **metadata embedded** into the **data stream**.

## How to run
This is a **console program** that, **when run** with **no arguments** (or by **double-clicking the exe**), will ask you to choose a wave file to play, you can also specify a **file path** as
the **first argument**. The **second argument** is the **buffer size in samples** which is also **optional**, the **default** value is `2048 samples`.
You can also try opening a file using the player **from the explorer** by _right-clicking > Open With > Choose another app > More apps > Look for another app on this PC_ and then selecting the compiled exe.

![Image of the wave file selection dialog](https://github.com/pisoj/simple-wave-player/assets/87895700/880a8a8a-b96e-4c86-9682-836f7ed01ccb)

## Compiling
A precompiled version is available [here](https://github.com/pisoj/lfs/releases/download/ExampleWavePlayer/main.exe), but you can also compile
it yourself by using a C compiler like [gcc](https://www.mingw-w64.org/downloads/). Make sure you are in the directory **where you saved**
the source code and that the source code **file is named** `main.c`. Then execute:

```shell
gcc main.c -o main.exe -lwinmm -lcomdlg32
```

## How it works
1. If there is no file path specified as a command line argument, it opens the [file picker dialog](https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-getopenfilenamea?redirectedfrom=MSDN).
2. Opens the chosen file in `binary reader` mode.
3. Reads the `RIFF` chunk specifying the format, which should be `WAVE`.
4. Since a wave file can have **many types of chunks**, which are **not all implemented** by this example player, it skips thouse and searches for the `fmt` chunk which specifies the **audio data information** (sample rate, number of channels, number of bits per sample...) required for proper playback.
5. Searches for the `data` chunk containing the actual **audio samples**.
6. Initializes the [Windows Multimedia Wave Out API](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/)
7. Loads the first couple of samples into the buffer and cues it for playback.
8. **Instantly** loads the next buffer and cues it **before the previous one is done playing** to avoid pauses between two buffers.
9. **Loads the upcoming buffers** as the old ones are done playing, but always does that **before the currently playing one is done** to avoid pauses.
10. When there are **no more bytes to read** from the file, it means that there are no **more samples to play** and that the playback has reached its **end**, so **resources** can be **released**.

#### Great explanation of a wave file structure: https://ccrma.stanford.edu/courses/422-winter-2014/projects/WaveFormat/
