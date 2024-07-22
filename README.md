# umbatts
ultra minimal battery status

This a VERY simple program that only display an icon representing the battery status, a bit in the spirit of "fdpowermon", but written in C, with ultra minimal footprint in mind ^^  
I wrote this little program to have a simple battery status icon (when a battery is detected) for my "q4rescue" project (a live media rescue toolkit based on Trinity; you can found it here by the way if you're interested: https://sourceforge.net/projects/q4rescue/ )  
It is designed to work with my kdeten_light icon set (from my q4osXpack project), relevant icons in 'icons' folder. You can adapt the script to use your own iconset of course :)  
If you want a "quit' button, just uncomment the commented lines.
  
-dependencies: gtk2 ; acpi  
  
-compile it with :   
gcc -Os -s -o umbatts umbatts.c `pkg-config --cflags --libs gtk+-2.0`                                                  
  
then you can run the binary : ./umbatts  
  
+---+

