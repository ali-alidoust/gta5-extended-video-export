**IMPORTANT:** Due to the way this mod works, **the original Rockstar Editor video file will NOT be saved to the disk** (actually it does, but it will be empty). The complete video will be exported to the folder specified in the .ini file.

**NOTE:** For high resolution exports without DSR, use [this](http://steamcommunity.com/sharedfiles/filedetails/?id=787616403) guide by [Kravencedesign](https://www.gta5-mods.com/users/Kravencedesign)

Source code for this mod is available on [github](https://github.com/ali-alidoust/gta5-extended-video-export).

Extended Video Export
=========================
Extended Video Export is an enhancement mod for GTA V, aimed at directors who want better export options from Rockstar editor.

Requirements
=========================
* Latest version of [ScriptHookV](http://www.dev-c.com/gtav/scripthookv/)
* Latest version of [Microsoft C++ 2015 Redistributable Package (both x86 and x64)](https://www.microsoft.com/en-us/download/details.aspx?id=48145)

How to install:
=========================
Just extract all the files to the game directory.

Configuration:
=========================
You can change the configuration by editing ExtendedVideoExport.ini file. Help about each option can be found in ExtendedVideoExport.txt

Current Features:
=========================

* **High Quality Export:**
Whenever you export a video via Rockstar Editor, this script saves it with a custom quality in the current user's video folder. The quality of the exported video can be configured using the .ini file. Even lossless exports are possible too.

* **Custom Video Encoder:**
Supported video encoders include (but not limited to) x264, x265, FFV1, VP8, VP9 and more.

* **Custom Audio Encoder:**
Supported audio encoders include FLAC, AC3, Vorbis and more.

* **ReShade/ENB Support:**
If you are using ReShade or ENB graphics mods, your exported videos will have the effects too.

* **Nvidia DSR Support:**
You can now export higher resolution videos using Nvidia DSR. Note that this will only work if you use DSR in Fullscreen mode. Windowed and Borderless Windowed modes are not supported. ATI VSR might work too, but it is not tested.

* **Custom frame rate (Experimental):**
You can now set the frame rate in the config file. Be careful that setting it too high might disable the exported file's audio. Also note that this feature has only been tested on 1.0.350.1 version of the game. If you tested on another version and it worked, please leave a comment and let me know.

* **Motion blur (Experimental):**
Videos can have high quality motion blur effect. Be careful when setting motion_blur_samples config, setting it too high will make the exporting take a very long time.

* **OpenEXR Export (High Dynamic Range):**
Exporting of floating point R16G16B16 version of the scene is now possible in OpenEXR format. When enabled, the mod will create a new folder beside the exported video that contains one .exr file for each frame. This file also contains the depth and stencil buffers but they aren't implemented the right way. These files are only usable in professional image and video manipulation programs. Enable this feature only if you know what you're doing.


Important things to note:
=========================

* The default video codec is libx264, but you can change it in the .ini file. There are a number of example codec configurations in the .ini file, you can try them or change the configuration as you like.
* Exporting a video using this mod might take much longer times than the original export, especially towards the end. You might think that the process has freezed, but most probably it's not.
* Lossless video files tend to get really large (around 1GB for 20 seconds of 1080p video @60fps in my case). These files are to be used with video editing software. They can also be played using MPC-HC and VLC, but it requires a lot of CPU power. The stuttering you may experience is because of the large size of the file and the complexity of the decoding process, the rendered files themselves have no stutter.


Changelog
=========================

**Changes in v0.2.0beta**

* OpenEXR export added
* Reimplemented frame capturing logic. This solves some crashes and blank videos.


**Changes in v0.1.6beta**

* Custom FPS and Motion Blur should now work in more recent versions of the game.


**Changes in v0.1.5beta**

* Fixed a parsing error in .ini file that made the mod always export as .mkv
* Added auto use of custom fps is it is supported by the game version


**Changes in v0.1.4beta (Experimental)**

* Added custom frame rate support
* Added motion blur support


**Changes in v0.1.3beta**

* Added option to export mp4 and avi files too.
* Fixed a number of crashes.


**Changes in v0.1.2beta**

* Fixed a bug where game freezed when exporting a video.


**Changes in v0.1.1beta**

* ReShade/ENB support added.
* High resolution export added using Nvidia DSR
* Fixed some random crashes.


**Changes in v0.1.0beta**

* Changed the way the frames are captures, so now full RGB exports are possible.
* Added configurable video and audio codecs support.
* Better memory management.
* Automatically reload the .ini file before each export.
* Different log levels.
* Experimental ENB/ReShade support removed since it was not good enough (actually it sucked).


**Changes in v0.0.5alpha**

* Added experimental ReShade/ENB support


**Changes in v0.0.4alpha**

* Added .ini configuration file support.
* Fixed a crash due to a race condition in the encoder.


**Changes in v0.0.3alpha**

* Added audio to the exported video file (also lossless).
* Better memory management


**Changes in v0.0.2alpha**

* Fixed crash in some resolutions/configurations.


**Changes in v0.0.1alpha:**

* Lossless video export

Configuration
=========================

**enable_mod**

* Description: If set to false, the script won't be run.
* Values: true, false
* Warning: Auto reload feature does not update this value, you have to restart the game for it to take effect  
* Example:
  * enable_mod = true

**auto_reload_config**

* Description: If set to true, this config file will be automatically reloaded whenever you export a new video.
  Especially useful if you want to toy with encoder settings
* Values: true, false
* Example:
  * auto_reload_config = true

**output_folder**

* Description: Videos will be exported to this folder. If left empty, current user's videos directory will be used.
* Values: [empty] or a valid path
* Warning: 
* Example:
  * output_folder = D:\MyVideos\

**log_level**

* Description: Sets the detail of the mod's logging feature. Please use "trace" level to report bugs.
* Values: error, warn, info, debug, trace
* Example:
  * log_level = trace

**[EXPORT] Section**

**format**

* Description: Output file format.
* Values: mkv, mp4, avi
* Example:
  * format = avi

**fps**

* Description: FPS value. 
* Values: It can be a floating point value (like 20.3), or a fraction (like 30000/1001)
* Warning: 
* Examples: 
  * fps = 60
  * fps = 23.976
  * fps = 30000/1001

**motion_blur_samples**

* Description: Number of motion blur samples. The higher the value, the higher the quality of motion blur, and higher exporting time. A value of zero means motion blur is disabled.
* Values: 0-255 (0 means disabled)
* Warning: Setting this to a high value will make export take a very long time.
* Example:
  * motion_blur_samples = 10

**export_openexr**

* Description: If enabled, each frame is exported as a floating point HDR OpenEXR file containing "RGBA" channels and "depth.Z" 
* Values: true, false
* Warning: Enabling this slows the exporting process significantly
* Example:
  * export_openexr = false

**[VIDEO] Section**

**encoder**

* Description: Sets the video encoder
* Values: Any FFMPEG video codec name. See <https://ffmpeg.org/ffmpeg-codecs.html#Video-Encoders> for supported encoders.
* Warning: Not all of the encoders are tested. Changing this might crash your game.
* Example:
  * encoder = libx265

**pixel_format**

* Description: Pixel format of the output video
* Values: Any format supported by the selected encoder. Most common formats are "yuv444p" and "yuv420p".
* Warning: The game might crash if the encoder does not support the pixel format.
* Example:
  * pixel_format = yuv444p

**options**

* Description: Video encoder options
* Values: Any number of key=value pairs seperated by '/'. See <https://ffmpeg.org/ffmpeg-codecs.html#Video-Encoders> for each encoder's available settings.
* Note: "b" is for bitrate
* Example:
  * options = preset=slow / b=40000000


## [AUDIO] Section

**encoder**

* Description: Sets the audio encoder.
* Values: Any FFMPEG audio codec name. See <https://ffmpeg.org/ffmpeg-codecs.html#Audio-Encoders> for supported codecs.
* Warning: Not all of the encoders are tested. Changing this might crash your game.
* Example:
  * encoder = flac

**sample_format**

* Description: Sets the audio sample format. 
* Values: Any format supported by the selected encoder. Most common formats are "s16" and "fltp".
* Warning: 
* Example:
  * sample_format = fltp

**sample_rate**

* Description: Audio sampling rate.
* Values: Any sampling rate you want. Sampling rates above 48000 are not useful since the game doesn't support them.
* Example:
  * sample_rate = 48000

**options**

* Description: 
* Values: 
* Note: "b" is for bitrate
* Example:
  * b=320000

**Example [VIDEO] presets**

**Lossless H.264 (Very slow)**

* encoder = libx264
* pixel_format = yuv420p
* options = preset=veryslow / crf=0


**Lossless H.264 (Fast)**

* encoder = libx264
* pixel_format = yuv420p
* options = preset=ultrafast / crf=0


**H.265 (HEVC) (Slow)**

* encoder = libx265
* pixel_format = yuv420p
* options = preset=slow / b=40000000


**H.265 (HEVC) (Very slow)**

* encoder = libx265
* pixel_format = yuv420p
* options = preset=veryslow / b=40000000


**Lossless FFV1 (Full RGB, Very Large Files)**

* enocder = ffv1
* pixel_format = bgr0
* options = slices=16 / slicecrc=1



**Example [AUDIO] presets**

**Lossless FLAC**

* encoder = flac
* sample_format = s16
* sample_rate = 48000
* options = 


**AC3 320Kbps**

* encoder = ac3
* sample_format = fltp
* sample_rate = 48000
* options = b=320000
