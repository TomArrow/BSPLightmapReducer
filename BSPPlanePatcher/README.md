
# BSPPlanePatcher

This tool can make modded de- & recompiled bsp files play smoother on servers running the original map by patching stored surface plane equations.

## Usage

BSPPlanePatcher is a command line (CLI) tool. You can most comfortably run it using a .bat script. A .bat script is a normal text file with a .bat file ending. 

### Example
Here is a simple working example of how to patch your modded map *moddedmap.bsp* using the original map *originalmap.bsp* and save that fixed map as *fixedmap.bsp*.

1. Put all the .bsp files and *BSPPlanePatcher.exe* in a fresh folder.
2. Create a new .txt text file and rename it to *randomScriptName.bat* (or any other filename with .bat ending). If you cannot see and change the file ending, [enable file extension display](#showFileExt).
3. Open this .bat file in a text editor and type ```BSPPlanePatcher.exe "moddedmap.bsp" "originalmap.bsp" "fixedmap.bsp"``` in the first line.
4. Replace ``moddedmap.bsp`` with the actual name of your own modded bsp.
5. Replace ``originalmap.bsp`` with the actual name of the original map .bsp.
6. Replace ``fixedmap.bsp`` with the name of the bsp you want to get in the end (doesn't really matter, u can name it anything and rename later).
7. Type ```pause``` in the second line. *(This is optional and allows you to see the console output instead of closing after it's done)*
8. Save and double click your .bat file.  This will run the reframing of your demo.
9. You should now have *fixedmap.bsp* in your folder. 
10. You are done.

## FAQ
### <a name="showFileExt"></a>I cannot change the textfile extension to .bat because I don't see the .txt extension

If you use Windows 10, in Explorer, go to the View tab and look for the "File name extensions" checkbox in the "Show/Hide" subcategory and tick it.

On older Windows versions, go into the folder and press the Alt button on your keyboard. This will bring up some menu options on top. Go to Tools->Folder Options. Navigate to the "View" tab and search for the "Hide extensions for known file types" checkbox. Untick it. Press the "Apply" button at the bottom and then "OK".
