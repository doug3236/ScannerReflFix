# ScannerReflFix

# NOTE - deprecated, see scanner_refl_fix for expanded features


Model and remove large area glare from Epson V850 reflection scans

This program is designed to address a major issue of the Epson V850 scanner caused by re-reflected light.
In short, very light portions of a scanned image reflect large amounts of light back to the frosted surfaces
of the illumination structures that are about 10mm to the top and and bottom of the slot which is being scanned.
When the image is all or near "white" that light, which is reflected from the paper to the frosted surfaces then
back to the paper can be almost 20% of the light originally illuminating the paper.

This can be easily observed by examining the RGB values of a scan with a small (4mm diameter) white patch inside
a 50mm black region and comparing it to the RGB values in the center of a large white area. This can easily cause
a shift in L* (CIELAB) as great as 6. For instance a L* of the white with a black surround may read 90 while the same
large area white reads 96.

This is c++17 code. It reads in a "raw" scanner file from the Epson Scan utility, models
the re-reflected light from the scanner's top/bottom illuminating surfaces, and removes it.

Included are C++17 files which use standard C++ but use the libtiff API and require libtiff which is
widely available.

The program will retain any attached ICC profiles but there is also a command option to attach an ICC profile
as the raw Epson scan tiff files do not have a profile attached.

Example usage:
scannerreffix -P scanner9800-4pg.icm Scanner273_33x29_96.tif Scanner273_33x29_96f.tif

The image is one of those I used to create a model of the re-reflected light and one can easily see the impact of
the re-reflected light as the RGB values increase as the area of the 6mm white patches increases. The corrected image
shows little variation by comparison.

This will read the first tiff file and write a corrected file using the second name and attach the icc
profile "scanner9800-4pg.icm"  This profile is included in the repo. It has one table which will interpret the
image in Absolute Colorimetric. This was made using Argyll open source software using a 4k patch set. It's
printer specific but produces highly accurate scans with deltaE00's averaging well under 1 when scanning prints
made from the same printer. However, metameric failure shift will occur when scanning other objects with different
spectral content and will produce large deltaE00 variations. This is also a problem using standard IT8 charts for profiling
since other scanned media will virtually always differ spectrally. However, this fixup program is effective with standard
IT8 profiles as well.

Commands:<br>
Version 1.1<br>
Usage: scannerreflfix [ zero or more options] infile.tif outfile.tif

    -A                   Correct Image Already in Adobe RGB<br>
    -F 8|16              Force 8 or 16 bit tif output<br>
    -W                   Maximize white (Like Relative Col with tint retention)<br>
    -P profile           Attach profile <profile.icc><br>
    -S edge_refl         ave refl outside of scanned area (0 to 1, default: .85)<br>
                         Test options<br>
    -I                   Save intermediate files<br>
    -T                   Show line numbers and accumulated time.<br>
    -N                   Don't Restore gain after subtracting reflection (Diagnostic only)<br>
    -R                   Simulated scanner by adding reflected light<br>
    -Z                   Average multiple input files with No Refl. Correction<br>
```    
  
