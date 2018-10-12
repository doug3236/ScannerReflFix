/*
Copyright (c) <2018> <doug gray>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <vector>
#include <iostream>
#include "ArgumentParse.h"
#include "tiffresults.h"
#include <array>
#include <fstream>
#include <algorithm>
#include <future>

//using namespace std;
using std::vector;
using std::string;
using std::cout;
using std::cin;
using std::endl;

string profile_name{ "" };              // optional file name of profile to attach to corrected image
int force_ouput_bits = 0;               // Force 16 bit output file. 8 bit input files default to 8 bit output files
bool adjust_to_detected_white = false;  // Scales output values so that the largest .01% of pixels are maxed (255)
bool save_intermediate_files = false;   // Saves various intermediate files for debugging
bool no_gain_restore = false;           // Just subtract reflected light estimate. Normal operation restores L* match
bool simulate_reflected_light = false;  // generate an image estimate of scanner's re-reflected light addition.
float edge_reflectance=.85f;            // average reflected light of area outside of scan crop (if black: .01)
bool print_line_and_time = false;       // print line number and time since start for each major phase of process
bool correct_image_in_aRGB = false;     // correct image from sacnner that has been converted to Adobe RGB
bool average_files_only = false;        // No reflection processing, useful for averaging multiple TIFF files

int main(int argc, char const **argv)
{
    Timer timer;
    vector<string> cmdArgs = vectorize_commands(argc, argv);

    // process options, all options must be valid and at least one file argument remaining
    try
    {
        procFlag("-A", cmdArgs, correct_image_in_aRGB);
        procFlag("-S", cmdArgs, edge_reflectance);
        procFlag("-W", cmdArgs, adjust_to_detected_white);
        procFlag("-P", cmdArgs, profile_name);
        procFlag("-R", cmdArgs, simulate_reflected_light);
        procFlag("-I", cmdArgs, save_intermediate_files);
        procFlag("-N", cmdArgs, no_gain_restore);
		procFlag("-F", cmdArgs, force_ouput_bits);
		procFlag("-T", cmdArgs, print_line_and_time);
        procFlag("-Z", cmdArgs, average_files_only);

		if (cmdArgs.size() == 1)
            throw("command line error\n");
        if (force_ouput_bits!=0 && force_ouput_bits!=8 && force_ouput_bits!=16)
            throw("-F n:   n must be either 8 or 16\n");
    }
    catch (const char *e)
    {
        cout << e << endl;
        cout << "Version 1.1:\n"
            "Usage: scannerreflfix [ zero or more options] infile.tif outfile.tif\n" <<
            "  -A                   Correct Image Already in Adobe RGB\n" <<
            "  -F 8|16              Force 8 or 16 bit tif output]\n" <<
            "  -W                   Maximize white (Like Relative Col with tint retention)\n" <<
            "  -P profile           Attach profile <profile.icc>\n" <<
            "  -S edge_refl         ave refl outside of scanned area (0 to 1, default: .85)\n\n" <<
			"                       Test options\n" <<
			"  -I                   Save intermediate files\n" <<
			"  -T                   Show line numbers and accumulated time.\n" <<
			"  -N                   Don't Restore gain after subtracting reflection (Diagnostic only)\n" <<
			"  -R                   Simulated scanner by adding reflected light\n" <<
            "  -Z                   Average multiple input files with No Refl. Correction " <<
            "scannerreflfix.exe models and removes re-reflected light from an area\n"
			"approx 1\" around scanned RGB values for the Epson V850 scanner.\n";
            exit(0);
    }

    // Create an image with simulated reflected light added from standard tif image
    // useful for simulating the effect of the re-reflected scanner light
    if (simulate_reflected_light) {
        cout <<"Simulating reflected light for V800/V850\n";
    }
    else if (!average_files_only)
        cout <<"Correcting reflected light for V800/V850\n";
    else
        cout << "No File Processing\n";
    try {
		// get first argument (uncorrected from image)
        int argCnt=(int)cmdArgs.size();
        ArrayRGB image_in = TiffRead(cmdArgs[1].c_str(), correct_image_in_aRGB ? 2.2f : 1.7f);

        // add additional images then calculate the mean
        if (average_files_only && argCnt - 2 > 0)
            cout << "Averaging " << argCnt - 2 << " files into " << cmdArgs[argCnt-1].c_str() << "\n";
        for (int i = 2; i < argCnt-1; i++)
        {
            ArrayRGB additional_image_in = TiffRead(cmdArgs[i].c_str(), correct_image_in_aRGB ? 2.2f : 1.7f);
            if (additional_image_in.v[0].size() != image_in.v[0].size())
                throw "Additional input images are not the same size";
            for (int i = 0; i < 3; i++) {
                for (int ii = 0; ii < additional_image_in.v[i].size(); ii++)
                {
                    image_in.v[i][ii] += additional_image_in.v[i][ii];
                }
            }
        }
        if (argCnt - 3 > 0)
        for (int i=0; i < 3; i++)
            for (auto& x : image_in.v[i])
                x = x / (argCnt - 2);

        if (!average_files_only)
        {
            // Get image that represents the light spread that is additive to the center's pixel location
            // x2: number of times DPI divisable by 2, x3:  number of times DPI divisable by 3
            auto[refl_area, x2, x3] = getReflArea(image_in.dpi);
            if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;


            // Add 1" margin around image_in since light is re-reflected over around an inch
            int margins = image_in.dpi;
            ArrayRGB in_expanded{ image_in.nr + 2 * margins, image_in.nc + 2 * margins, image_in.dpi, image_in.from_16bits, image_in.gamma };
            in_expanded.fill(edge_reflectance, edge_reflectance, edge_reflectance);             // assume border "white" reflected 85% of kight.
            in_expanded.copy(image_in, margins, margins);   // insert into expanded image with 1" margins

            // for getting estimated reflected light spread
            if (save_intermediate_files)
            {
                cout << "Saving reflarray.tif, image of additional reflected light in gamma = 2.2" << endl;
                refl_area.gamma = 2.2f;      // write gamma for compatibility with aRGB
                TiffWrite("reflArray.tif", refl_area, "", false);
            }
            if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;

            // Create downsized image to calculate reflected light from
            // This does not require or need high resolution.
            ArrayRGB image_reduced = in_expanded;
            int reduction = image_in.dpi / refl_area.dpi;

            // Downsize image to create a reflected light version, use 3x downsize initially for speed
            while (x3--)
                image_reduced = downsample(image_reduced, 3);
            while (x2--)
                image_reduced = downsample(image_reduced, 2);
            if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;


            // save downsampled file with added margin
            if (save_intermediate_files)
            {
                cout << "Saving imagorig.tif, reduced original file with surround in gamma=2.2" << endl;
                image_reduced.gamma = 2.2f;      // write gamma for compatibility wiht aRGB
                TiffWrite("imageorig.tif", image_reduced, "");
            }


            if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;
            ArrayRGB image_correction = generate_reflected_light_estimate(image_reduced, refl_area);
            if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;

            // save the estimated re-reflected light from the full scanned image and surround
            if (save_intermediate_files)
            {
                cout << "Saving refl_light.tif, image of estimated reflected light" << endl;
                image_correction.gamma = 2.2f;      // write gamma for compatibility with aRGB and sRGB
                TiffWrite("refl_light.tif", image_correction, "");
            }

            // Subtract re-reflected light from original
            for (int color = 0; color < 3; color++)
            {
                for (int i = 0; i < image_in.nr; i++)
                {
                    for (int ii = 0; ii < image_in.nc; ii++)
                    {
                        //auto tmp = image_in(i, ii, color)-image_correction(i/reduction, ii/reduction, color)*image_in(i, ii, color);
                        float tmp;
                        if (simulate_reflected_light) {
                            tmp = image_in(i, ii, color) + bilinear(image_correction, i, ii, reduction, color)*image_in(i, ii, color);
                            tmp *= .785f / .876f;
                        }
                        else {
                            tmp = image_in(i, ii, color) - bilinear(image_correction, i, ii, reduction, color)*image_in(i, ii, color);
                            // gain restore  adjusts gain to offset reduction from re-reflected light subtraction
                            tmp = tmp * (no_gain_restore ? 1.0f : .876f / .785f);
                        }
                        image_in(i, ii, color) = std::clamp(tmp, 0.f, 1.f);
                    }
                }
            }

            // Adjust for Relative Colorimetric w/o shift to WP (no tint change)
            // Should not be used to process scanner profiling patch scans
            if (adjust_to_detected_white)
            {
                float maxcolor = 0;
                for (int i = 0; i < 3; i++)
                {
                    vector<float> color(image_in.v[i]);
                    sort(color.begin(), color.end());
                    float high = *(color.end() - (1 + color.size() / 10000));
                    if (high > maxcolor) maxcolor = high;
                }
                image_in.scale(1 / maxcolor);
            }

        }
		if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;
		if (force_ouput_bits==16)
            image_in.from_16bits = false;
        else if (force_ouput_bits == 8)
            image_in.from_16bits = true;

        TiffWrite(cmdArgs[argCnt-1].c_str(), image_in, profile_name);
		if (print_line_and_time) cout << __LINE__ << "  " << timer.stop() << endl;

    }
    catch (const char *e)
    {
        cout << e << "\nPress enter to exit\n";
        char tmp[10];
        cin.getline(tmp, 1);
    }
    catch (const std::exception &e)
    {
        cout << e.what() << endl;
    }
    catch (...) {
        cout << "unknown exception\n";
    }

}

