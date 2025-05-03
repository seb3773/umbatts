# umbatts
ultra minimal battery status

This a VERY simple program that only display an icon in the systray representing the battery status, a bit in the spirit of "fdpowermon", but written in C, with ultra minimal footprint and maximal optimisation in mind ^^  
If it detects backlight control available, it will display a slider to adjust brightness.
  
I wrote this little program to have a simple battery status icon (when a battery is detected) for my "q4rescue" project (a live media rescue toolkit based on q4os Trinity; you can found it here by the way if you're interested: https://sourceforge.net/projects/q4rescue/ )  
  
It is designed to work with my kdeten_light icon set (from my q4osXpack project), relevant icons in 'icons' folder. You can adapt the code to use your own iconset of course :)  
  
-dependencies: gtk2 ; acpi  
  
-compile it with :   
gcc -O2 -fstrict-aliasing -flto -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -ffast-math -fvisibility=hidden -fuse-ld=gold -Wl,--gc-sections,--build-id=none,-O1 -s -o umbatts umbatts.c `pkg-config --cflags --libs gtk+-2.0` -Wno-deprecated-declarations && strip ./umbatts

                                  
  
then you can run the binary : ./umbatts  
  
+---+

