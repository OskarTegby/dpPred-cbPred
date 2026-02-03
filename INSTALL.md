# Instructions
1. To compile the code, you need to use the Docker image found in the Docker directory.
**NOTE:** The reason for this is that the installation requires the version of gcc to be quite old, and a lot of other dependencies are required. If you use too new versions of gcc, you will get compilation errors as an artifact because of that.
2. Run install.sh to export PIN_ROOT, PIN_HOME, SNIPER_ROOT, GRAPHITE_ROOT, and BENCHMARKS_ROOT. It also downloads and extracts the latest-compatible version of PIN into the pin_kit directory, and clones the benchmarks directory.
3. To download the benchmarks, go into benchmarks/tools/docker and run make buildhub. Then, run the copy_out.sh script to copy out the benchmarks from the image. 
4. To run Sniper, you need to again use the Docker image by using make run and going into the benchmarks directory. There, you use ./run-sniper with the flags specified in that Python script.

# Remarks
1. To run SPEC2006 or SPEC2017, you need a license.
2. You can run your Sniper code from the benchmarks dir due to SNIPER_ROOT.
