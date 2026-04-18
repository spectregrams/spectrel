<h1 align="center">
  Spectrel: Process, Explore and Capture Transient Radio Emissions (Lite)
</h1>

<div align="center">
  <img src="gallery/rtlsdr_example_a.png" height="200px">
  <img src="gallery/spectre.png" height="200px">
  <img src="gallery/rtlsdr_example_b.png" height="200px">
</div>

## Getting started

_Spectrel_ is (yet another) SDR-agnostic program for recording radio signals and spectrograms. It offers a lightweight alternative to [_Spectre_](https://github.com/jcfitzpatrick12/spectre) written in pure C. Powered by [SoapySDR](https://github.com/pothosware/SoapySDR) and the [FFTW](https://www.fftw.org/) library.

> :warning: We're in pre-alpha - please expect some instability.

### Supported Platforms
_Spectrel_ is expected to be compatible with most Linux distributions.

### Prerequisites

- [FFTW3](https://www.fftw.org/)
- [SoapySDR](https://github.com/pothosware/SoapySDR)
- The drivers for the hardware you want to have support for. 

### Installation

1. **Clone the repository**  
    ```bash
    git clone https://github.com/jcfitzpatrick12/spectrel.git && cd spectrel
    ```

2. **Build the binary**  
    Assuming you are in the root directory for this repository:  
    ```bash
    sudo make install
    ```

3. **Good to go!**  
    You can now run _Spectrel_ using:  
    ```bash
    spectrel -r <receiver> -f <frequency> -s <sample_rate> -b <bandwidth> -g <gain> -T <duration> [-d directory]  [-w window_size] [-h window_hop] [-B buffer_size]
    ```
    The spectrograms are streamed to a file named `<timestamp>_<receiver>.cf64`, where `<time_stamp>` is the current system time formatted in the ISO 8601 standard and `<receiver>` is the SDR driver name. Each sample corresponds to a complex DFT amplitude (64 bits per component), stored in column (spectrum) major ordering.

    **OPTIONS**

    **-r** *receiver*  
    The SDR driver name. Examples: "rtlsdr", "hackrf"

    **-f** *frequency*  
    Center frequency in Hz

    **-s** *sample_rate*  
    Sample rate in Hz

    **-b** *bandwidth*  
    Bandwidth in Hz

    **-g** *gain*  
    Gain setting in dB

    **-T** *duration*  
    Recording duration in seconds

    **-d** *directory*  
    Output directory (default: current working directory)

    **-w** *window_size*  
    FFT window size (default: 1024 samples)

    **-h** *window_hop*  
    FFT window hop size (default: 512 samples)

    **-B** *buffer_size*  
    Buffer size (default: 16384 samples)

### Examples

Record spectrograms for 20 seconds at 95.8MHz using an RTL-SDR:  
```
spectrel -r rtlsdr -f 95800000 -s 250000 -b 250000 -g 30 -T 20
```

Record spectrograms for 10 seconds at 445MHz, at the maximum sample rate for a Hack RF One (20MHz) into a custom directory:  
```
spectrel -r hackrf -f 445000000 -s 20000000 -b 20000000 -g 20 -T 10 -d ./recordings
```

Record spectrograms for 0.25 seconds at 32MHz, at the maximum sample rate for the RX-888 MK II (64MHz):
```
spectrel -r SDDC -f 32000000 -s 64000000 -b 64000000 -g 56 -T 0.25
```
