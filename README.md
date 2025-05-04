# umbatts
ultra minimal battery status

This a VERY simple program that only display an icon in the systray representing the battery status, a bit in the spirit of "fdpowermon", but written in C, with ultra minimal footprint and maximal optimisation in mind ^^  
If it detects backlight control available, it will display a slider to adjust brightness.
  
I wrote this little program to have a simple battery status icon (when a battery is detected) for my "q4rescue" project (a live media rescue toolkit based on q4os Trinity; you can found it here by the way if you're interested: https://sourceforge.net/projects/q4rescue/ )  
  
It is designed to work with my kdeten_light icon set (from my q4osXpack project), relevant icons in 'icons' folder. You can modify them or the python script used in Makefile to use your own iconset of course :)  
  
-dependencies: gtk2 ; acpi  
  
-compile it with :   
make
                         
  
then you can run the binary : ./umbatts  
  
+---+

