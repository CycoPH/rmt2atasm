Relocatable RMT player for atasm assembler
==========================================

Set of samples and code to include Raster-Music-Tracker songs with
programs written in atasm assenbler.


How to convert a song to the relocatable source
-----------------------------------------------

To convert any RMT song to the relocatable format for inclusion in your program,
you need to:

- Export the song from RMT as a stripped file, select "File", "Export as...",
  file type of "RMT stripped song file".
  You can select any memory location, other options are according to your
  needs. Remember to copy the RMT FEATures presented, and write them to the
  file `rmt_feat.asm`, as this file is needed to assemble the player.
  Edit the `rmt_feat.asm` file and replace all the `equ` with `=`
  
- Use the included C program `rmt2atasm` to convert the RMT file to a relocatable
  assembly file, use as:
  ```
  rmt2atasm my_song.rmt > tune.asm
  ```


Main player and sample song files
---------------------------------

The file `atasm/rmtplayr.asm` is the full RMT player source, converted to atasm syntax.


Samples
-------

There are two sample folders (Test1 and Test2)

Build scripts are provided.
