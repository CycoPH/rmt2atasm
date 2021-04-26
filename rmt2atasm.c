/*
 * rtm2atasm: convert an Raster Music Tracker file to a relocatable asm file.
 * Copyright (C) 2019-2020 Daniel Serpell
 * Modified for atasm assembler by Peter Hinz (2021)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#define _CRT_SECURE_NO_WARNINGS 1
#define MIN_RMT_LENGTH 40

#include <stdio.h>
#include <stdlib.h>

static int get_word(FILE* f)
{
	int c1, c2;
	if (0 > (c1 = getc(f)) || 0 > (c2 = getc(f)))
		return -1;
	return (c1 & 0xFF) | ((c2 << 8) & 0xFF00);
}

static int rword(unsigned char* data, int i)
{
	return data[i] | (data[i + 1] << 8);
}

static int read_rmt(const char* fname)
{
	int err = -1;
	FILE* f = fopen(fname, "rb");
	if (!f)
	{
		perror(fname);
		return err;
	}
	// Get the header FFFF and the start and end 16bit values
	int start, end;
	if (get_word(f) < 0 ||
		(start = get_word(f)) < 0 ||
		(end = get_word(f)) < 0 ||
		end < start)
	{
		fprintf(stderr, "%s: invalid RMT file\n", fname);
		goto err_close;
	}
	int len = end + 1 - start;
	if (len < MIN_RMT_LENGTH) {
		fprintf(stderr, "RMT file is too short (only %d), need a minimum of %d\n", len, MIN_RMT_LENGTH);
		goto err_close;
	}
	unsigned char* buf = (unsigned char*)malloc(len);
	if (!buf)
	{
		perror(fname);
		goto err_close;
	}
	if (1 != fread(buf, len, 1, f))
	{
		fprintf(stderr, "%s: short RMT file\n", fname);
		goto err_fbuf;
	}
	if (buf[0] != 'R' || buf[1] != 'M' || buf[2] != 'T' ||
		(buf[3] != '4' && buf[3] != '8'))
	{
		fprintf(stderr, "%s: invalid signature, not an RMT file\n", fname);
		goto err_fbuf;
	}
	// Start generating the assembler code
	// Output to the console

	// Header struct
	// =============
	//
	// offset type desc
	// ------ ---- ----
	// 00 DWORD header string 'RMT4' or 'RMT8'
	// 04 BYTE track len($ 00 means 256)
	// 05 BYTE song speed
	// 06 BYTE player freq
	// 07 BYTE format version number($ 01 for player routine 1.x compatible format)
	// 08 WORD pointer to instrument pointers table
	// 0a WORD pointer to track pointers table(lo)
	// 0c WORD pointer to track pointers table(hi)
	// 0e WORD pointer to tracks list(SONG)

	printf("; RMT%c file converted from %s with rmt2atasm\n"
		"; Original size: $%04x bytes @ $%04x\n", buf[3], fname, len, start);

	// Read parameters from file:
	int numTracks = buf[3] - '0';				// RMTx - x is 4 or 8
	int offsetInstrumentPtrTable = rword(buf, 8) - start;	// This is where the instruments are stored (always directly after this table)
	int offsetTrackPtrTableLow = rword(buf, 10) - start;	// Track ptrs are broken up into lo and hi bytes
	int offsetTrackPtrTableHigh = rword(buf, 12) - start;
	int offsetSong = rword(buf, 14) - start;				// The song tracks start here
	
	// The instrument table always starts directly after the header!
	if (offsetInstrumentPtrTable != 0x10)
	{
		fprintf(stderr, "%s: malformed file (instrument table = $%04x)\n", fname, offsetInstrumentPtrTable);
		goto err_fbuf;
	}
	int numInstruments = offsetTrackPtrTableLow - offsetInstrumentPtrTable;
	if (numInstruments < 0 || (numInstruments & 1) || offsetTrackPtrTableLow >= len)
	{
		fprintf(stderr, "%s: malformed file (num instruments = $%04x)\n", fname, numInstruments);
		goto err_fbuf;
	}
	int numtrk = offsetTrackPtrTableHigh - offsetTrackPtrTableLow;
	if (numtrk < 0 || numtrk > 0xFF || offsetTrackPtrTableHigh >= len)
	{
		fprintf(stderr, "%s: malformed file (num tracks = $%04x)\n", fname, numtrk);
		goto err_fbuf;
	}

	// Read all tracks addresses searching for the lowest address
	int first_track = 0xFFFF;
	for (int i = 0; i < numtrk; i++)
	{
		int x = buf[offsetTrackPtrTableLow + i] + (buf[offsetTrackPtrTableHigh + i] << 8);
		if (x)
		{
			x -= start;
			if (x < 0 || x >= offsetSong)
			{
				fprintf(stderr, "%s: malformed file (track %d = $%04x [0:$%x])\n",
					fname, i, x, offsetSong);
				goto err_fbuf;
			}
			if (x < first_track)
				first_track = x;
		}
	}
	// Read all instrument addresses searching for the lowest address
	int first_instr = 0xFFFF;
	for (int i = 0; i < numInstruments; i += 2)
	{
		int x = rword(buf, offsetInstrumentPtrTable + i);
		if (x)
		{
			x -= start;
			if (x < 0 || x >= first_track)
			{
				fprintf(stderr, "%s: malformed file (instrument %d = $%04x [0:$%x])\n",
					fname, i, x, first_track);
				goto err_fbuf;
			}
			if (x < first_instr)
				first_instr = x;
		}
	}

	if (first_instr < 0 || first_instr >= len ||
		first_track < 0 || first_track >= len)
	{
		fprintf(stderr, "%s: malformed file (first track/instr = $%04x/$%04x)\n", fname,
			first_track, first_instr);
		if (first_instr = 0xFFFF)
			fprintf(stderr, "No instrument data!\n");
		if (first_track)
		goto err_fbuf;
	}
	if (offsetTrackPtrTableHigh + numtrk != first_instr)
	{
		fprintf(stderr, "%s: malformed file (track/instr = $%04x/$%04x)\n", fname,
			offsetTrackPtrTableHigh + numtrk, first_instr);
		goto err_fbuf;
	}
	if (first_track < first_instr)
	{
		fprintf(stderr, "%s: malformed file (track < instr = $%04x/$%04x)\n", fname,
			first_track, first_instr);
		goto err_fbuf;
	}

	// Write assembly output
	printf(
		"    .local\n"
		"RMT_SONG_DATA\n"
		"?start\n"
		"    .byte \"RMT%c\"\n"
		, buf[3]);
	printf("?song_info\n"
		"    .byte $%02x            ; Track length = %d\n"
		"    .byte $%02x            ; Song speed\n"
		"    .byte $%02x            ; Player Frequency\n"
		"    .byte $%02x            ; Format version\n"
		, buf[4], buf[4]// max track length
		, buf[5]		// song speed
		, buf[6]		// player frequency
		, buf[7]		// format version
	);
	printf("; ptrs to tables\n");
	printf(
		"?ptrInstrumentTbl\n    .word ?InstrumentsTable       ; start + $%04x\n"
		"?ptrTracksTblLo\n    .word ?TracksTblLo            ; start + $%04x\n"
		"?ptrTracksTblHi\n    .word ?TracksTblHi            ; start + $%04x\n"
		"?ptrSong\n    .word ?SongData               ; start + $%04x\n",
		offsetInstrumentPtrTable, offsetTrackPtrTableLow, offsetTrackPtrTableHigh, offsetSong);

	// List of ptrs to instruments
	printf("\n; List of ptrs to instruments\n");
	int* instr_pos = (int*)calloc(65536, sizeof(int));
	printf("?InstrumentsTable");
	for (int i = 0; i < numInstruments; i += 2)
	{
		int loc = rword(buf, i + offsetInstrumentPtrTable) - start;
		if (i % 16 == 0)
			printf("\n    .word ");
		else
			printf(", ");
		if (loc >= first_instr && loc < first_track && loc < len)
		{
			instr_pos[loc] = (i >> 1) + 1;
			printf("?Instrument_%d", i >> 1);
		}
		else if (loc == -start)
			printf("  $0000");
		else
		{
			fprintf(stderr, "%s: malformed file (instr %d = $%04x [%x:%x])\n", fname,
				i, loc, first_instr, first_track);
			goto err_finstr;
		}
	}
	printf("\n");
	// List of tracks:
	int* track_pos = (int*)calloc(65536, sizeof(int));
	printf("\n"
		"?TracksTblLo");
	for (int i = 0; i < numtrk; i++)
	{
		int loc = buf[i + offsetTrackPtrTableLow] + (buf[i + offsetTrackPtrTableHigh] << 8) - start;
		if (i % 8 == 0)
			printf("\n    .byte ");
		else
			printf(", ");
		if (loc >= first_track && loc < offsetSong && loc < len)
		{
			track_pos[loc] = i + 1;
			printf("<?Track_%02x", i);
			// printf("(start + $%04x)", loc);
		}
		else if (loc == -start)
			printf("$0000");
		else
		{
			fprintf(stderr, "%s: malformed file (track %d = $%04x [%x:%x)\n", fname,
				i, loc, first_track, offsetSong);
			goto err_ftrack;
		}
	}
	printf("\n"
		"?TracksTblHi");
	for (int i = 0; i < numtrk; i++)
	{
		int loc = buf[i + offsetTrackPtrTableLow] + (buf[i + offsetTrackPtrTableHigh] << 8) - start;
		if (i % 8 == 0)
			printf("\n    .byte ");
		else
			printf(", ");
		if (loc >= first_track && loc < offsetSong && loc < len)
			printf(">?Track_%02x", i);
		// printf("(start + $%04x)", loc);
		else if (loc == -start)
			printf("$0000");
		else
		{
			fprintf(stderr, "%s: malformed file (track %d = $%04x [%x:%x)\n", fname,
				i, loc, first_track, offsetSong);
			goto err_ftrack;
		}
	}
	// Print instruments
	printf("\n\n; Instrument data");
	for (int i = first_instr, l = 0; i < first_track; i++, l++)
	{
		if (instr_pos[i])
		{
			printf("\n?Instrument_%d", instr_pos[i] - 1);
			instr_pos[i] = 0;
			l = 0;
		}
		if (l % 16 == 0)
			printf("\n    .byte ");
		else
			printf(", ");
		printf("$%02x", buf[i]);
	}
	for (int i = 0; i < 65536; i++)
	{
		if (instr_pos[i] != 0)
			fprintf(stderr, "%s: missing instrument data for %d at $%04x\n",
				fname, instr_pos[i], i);
	}
	// Print tracks
	printf("\n\n; Track data");
	for (int i = first_track, l = 0; i < offsetSong; i++, l++)
	{
		if (track_pos[i])
		{
			printf("\n?Track_%02x", track_pos[i] - 1);
			track_pos[i] = 0;
			l = 0;
		}
		if (l % 16 == 0)
			printf("\n    .byte ");
		else
			printf(", ");
		printf("$%02x", buf[i]);
	}
	for (int i = 0; i < 65536; i++)
	{
		if (track_pos[i] != 0)
			fprintf(stderr, "%s: missing track data for %d at $%04x\n",
				fname, track_pos[i], i);
	}
	// Print SONG
	printf("\n\n; Song data\n?SongData");
	int jmp = 0, l = 0;
	for (int i = offsetSong; i < len; i++, l++)
	{
		if (jmp == -2)
		{
			jmp = 0x10000 + buf[i];
			continue;
		}
		else if (jmp > 0)
		{
			jmp = (0xFFFF & (jmp | (buf[i] << 8))) - start;
			if (0 == ((jmp - offsetSong) % numTracks) && jmp >= offsetSong && jmp < len)
			{
				int lnum = (jmp - offsetSong) / numTracks;
				printf(", <?line_%02x, >?line_%02x", lnum, lnum);
			}
			else
			{
				fprintf(stderr, "%s: malformed file (song jump bad $%04x [%x:%x])\n", fname,
					jmp, offsetSong, len);
				printf(", <($%x+?SongData), >($%x+?SongData)", jmp, jmp);
			}
			jmp = 0;
			// Allows terminating song on last JUMP
			if (i + 1 == len && numTracks == 8)
				l += 4;

			continue;
		}
		else if (jmp == -1)
			jmp = -2;

		if (l % numTracks == 0)
		{
			printf("\n?line_%02x:  .byte ", l / numTracks);
		}
		else
			printf(", ");
		printf("$%02x", buf[i]);
		if (buf[i] == 0xfe)
		{
			if ((l % numTracks) != 0)
				fprintf(stderr, "%s: malformed file (misplaced jump)\n", fname);
			else
				jmp = -1;
		}
	}
	printf("\n");
	if (jmp)
		fprintf(stderr, "%s: malformed file (song jump incomplete)\n", fname);
	else if (0 != l % numTracks)
		fprintf(stderr, "%s: malformed file (song incomplete - %d %d)\n", fname, numTracks, len - offsetSong);
	else
		err = 0;

err_ftrack:
	free(track_pos);
err_finstr:
	free(instr_pos);
err_fbuf:
	free(buf);
err_close:
	fclose(f);
	return err;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "Convert a Raster Music Tracker .RMT file into relocatable ATASM assmbler code.\n");
		fprintf(stderr, "The generated output will be dumpted to the console.\n");
		fprintf(stderr, "Invalid number of arguments.\n\n"
			"Usage: %s [file.rmt]\n", argv[0]);
		return 1;
	}
	if (read_rmt(argv[1]))
		return 1;
	return 0;

}
