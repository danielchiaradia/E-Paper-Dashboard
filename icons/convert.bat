FOR %%A IN (*.svg) DO "C:\Program Files\Inkscape\bin\inkscape" %%A --export-type=png --export-filename=%%A.png -w 142 --export-background="#ffffff"

FOR %%A IN (*.png) DO py imgconvert.py -i %%A -n %%A -o %%A.h