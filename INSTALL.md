# Instructions
1. Run install.sh to export PIN_ROOT, PIN_HOME, SNIPER_ROOT, GRAPHITE_ROOT, and BENCHMARKS_ROOT. It also downloads and extracts the latest-compatible version of PIN into the pin_kit directory, and clones the benchmarks directory.
2. To compile the code, you need to use the Docker image found in the Docker directory.
3. To download the benchmarks, go into benchmarks/tools/docker and run make buildhub. Then, run the copy_out.sh script to copy out the benchmarks from the image. 
4. To run Sniper, you need to again use the Docker image by using make run and going into the benchmarks directory. There, you use ./run-sniper with the flags specified in that Python script.

# To do
1. Automatically export the path to the Sniper dir so that the user doesn't need to set it themselves for the code to compile.
2. Host the expected results to download somewhere so that it's not necessary to have them transferred from another computer or user.

# Notes
The reason that we have to use the Docker image when compiling the code is that it requires the version of gcc to be quite old along with a lot of other dependencies. If you use a version of gcc that is too recent, you will get compilation errors as an because of that.

# Installation remarks
You need to first need to build the Docker image to compile the code. You do so using

$ make all

and then run the container using

$ make run

You might need to add your user to the Docker group first, which you do using

$ sudo usermod -aG docker $USER
$ newgrp docker

To ensure write rights by the Docker user, you might also need to 

$ sudo chown -R $USER:$USER /home/$USER/repos/code/dpPred-cbPred

You can verify the installation using

$ docker images | grep sniper

# General remarks
1. To run SPEC2006 or SPEC2017, you need a license.
2. You can run your Sniper code from the benchmarks dir due to SNIPER_ROOT.

