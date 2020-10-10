/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// wadlib.h

//
// wad reading
//

#define	CMP_NONE		0
#define	CMP_LZSS		1

#define	TYP_NONE		0
#define	TYP_LABEL		1
#define	TYP_LUMPY		64				// 64 + grab command number

typedef struct
{
	char		identification[4];		// should be WAD2 or 2DAW
	int			numlumps;
	int			infotableofs;
} wadinfo_t;


typedef struct
{
	int			filepos;
	int			disksize;
	int			size;					// uncompressed
	char		type;
	char		compression;
	char		pad1, pad2;
	char		name[16];				// must be null terminated
} lumpinfo_t;

extern	lumpinfo_t		*lumpinfo;		// location of each lump on disk
extern	int				numlumps;
extern	wadinfo_t		header;

void	W_OpenWad (char *filename);
int		W_CheckNumForName (char *name);
int		W_GetNumForName (char *name);
int		W_LumpLength (int lump);
void	W_ReadLumpNum (int lump, void *dest);
void	*W_LoadLumpNum (int lump);
void	*W_LoadLumpName (char *name);

void CleanupName (char *in, char *out);

//
// wad creation
//
void	NewWad (char *pathname, qboolean bigendien);
void	AddLump (char *name, void *buffer, int length, int type, int compress);
void	WriteWad (void);

