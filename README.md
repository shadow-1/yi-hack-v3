# yi-hack-v3

This project is a collection of config files, Makefiles and scripts to allow custom firmware images to be created and deployed to extend the functionality of Xiaomi Cameras based on HiSilicon Hi3518e V200 chipset.

Currently this project supports:
* Yi 1080p Dome

However with community support, this firmware can be extended to support at least the following cameras as they are based on the same chipset:
* Yi Dome
* Yi 1080p Home v1

![Alt text](yi-cam.png?raw=true "Yi Cameras")

## Features
The supported cameras have the following features by default:
* Wifi
* Motion detection - a video file is generated if a motion have been detected in the last 60 seconds.
* Send video data over the network on Chinese servers in the cloud to allow people to view camera data from their smartphone wherever they are.
* Setup thanks to a smartphone application.
* Local video storage on a microSD card

This firmware includes:
* Telnet server - _Enabled by default._
* FTP server - _Enabled by default._
* Web server - _Enabled by default._
* Proxychains-ng - _Enabled by default. Useful if the camera is region locked._

## Cameras that are Region Locked to Mainland China
This firmware includes Proxychains-ng. This allows communication between the camera and Xiaomi server to be routed through a proxy server. If Proxychains-ng is configured with a proxy server from Mainland China, the camera will no longer be region locked.

Performance is not degraded as the cameras video/audio feed is not routed through the proxy server.

## Getting Started

### Prepare the microSD Card
Download a release firmware binaries from Github. Two firmware files are required. One firmware file is for the rootfs partition on the camera whilst the second firmware file is for the home partition on the camera.

Format a microSD card in FAT32 format and copy the two firmware files onto the camera.

**_Note: The microSD card must be formatted in FAT32. exFAT formatted microSD cards will not work._**

The memory card will contain:

* rootfs_h20m - The replacement rootfs partition.
* home_h20m - The replacement home partition.

**_Note: Both the rootfs and home partitions must be upgraded on the camera. The additional features implemented in the home partition will not work unless the rootfs partition has also been upgraded._**

### Starting the Camera
* If the camera is plugged in, unplug the camera.
* Insert the microSD card containing the firmware files.
* Plug in the camera.

The camera will start, however it will take approximately 30 seconds longer to start as it is upgrading the firmware on the camera.

**_Note: The camera will not be fully operational through the smartphone app until a valid configuration has been entered for Proxychains-ng or Proxychains-ng is disabled._**

### Configuring the Camera
The cameras web interface is directly accessible by entering the IP address of the camera into a web browser [http://IP_of_Camera/](http://IP_of_Camera/).

If your camera does not have Wifi setup. Pair the camera with your smartphone as per usual using the smartphone app.

![Alt text](web_interface.png?raw=true "Web Interface")

## Using the Camera

### Telnet Server
The telnet server is on port 23.

No authentication is needed, default user is root.

### FTP Server
The FTP server is on port 21.

No authentication is needed, you can login as anonymous.

### Startup Shell Script
On the microSD card. The following shell script is executed after the camera has booted up:

`startup.sh`

### Which Version is Installed?
The base firmware (Xiaomi firmware) version and y-hack-v3 firmware version is accessible through the webinterface on the About page.

## Development
**TODO**

## Acknowledgments

Special thanks to the following projects for their efforts on other Xiaomi cameras and giving inspiration for me to develop and publish my own custom firmware.

**fritz-smh** : https://github.com/fritz-smh/yi-hack

**niclet** : https://github.com/niclet/yi-hack-v2


