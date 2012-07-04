//TODO: id_heads.h
#include "id_rf.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "id_ca.h"
#include "id_mm.h"
#include "id_ti.h"

#include <stdbool.h>
#include <stdio.h>

// The refresh manager (ID_RF) is the core of the game's smooth-scrolling
// engine. It renders tiles to an offscreen bufferand then blits from
// there to a screen buffer, which then has the sprites overlayed.
// John Carmack refers to this technique as 'Virtual Tile Refresh'.

// These buffers are all larger than the physical screen, and scroll
// tile-by-tile, not pixel by pixel. The smooth-scrolling effect
// is implemented by changing which part of the buffer is displayed.

#define RF_BUFFER_WIDTH_TILES 21
#define RF_BUFFER_HEIGHT_TILES 14

#define RF_BUFFER_WIDTH_PIXELS (RF_BUFFER_WIDTH_TILES << 4)
#define RF_BUFFER_HEIGHT_PIXELS (RF_BUFFER_HEIGHT_TILES << 4)

#define RF_BUFFER_WIDTH_UNITS (RF_BUFFER_WIDTH_TILES << 8)
#define RF_BUFFER_HEIGHT_UNITS (RF_BUFFER_HEIGHT_TILES << 8)


#define RF_SCREEN_WIDTH_TILES 20
#define RF_SCREEN_HEIGHT_TILES 13

// Scroll blocks prevent the camera from moving beyond a certain row/column
#define RF_MAX_SCROLLBLOCKS 6
static int rf_horzScrollBlocks[RF_MAX_SCROLLBLOCKS];
static int rf_vertScrollBlocks[RF_MAX_SCROLLBLOCKS];
static int rf_numHorzScrollBlocks;
static int rf_numVertScrollBlocks;

static void *rf_tileBuffer;
//static void *rf_screenBuffer;
static int rf_mapWidthTiles;
static int rf_mapHeightTiles;

//Scroll variables
//static int rf_scrollXTile;
//static int rf_scrollYTile;
int rf_scrollXUnit;
int rf_scrollYUnit;
//static int rf_scrollX
static int rf_scrollXMinUnit;
static int rf_scrollYMinUnit;
static int rf_scrollXMaxUnit;
static int rf_scrollYMaxUnit;


#define RF_MAX_SPRITETABLEENTRIES 48
#define RF_NUM_SPRITE_Z_LAYERS 4

// Pool from which sprite draw entries are allocated.
RF_SpriteDrawEntry rf_spriteTable[RF_MAX_SPRITETABLEENTRIES];
RF_SpriteDrawEntry *rf_freeSpriteTableEntry;

RF_SpriteDrawEntry *rf_firstSpriteTableEntry[RF_NUM_SPRITE_Z_LAYERS];

// Animated tile management
// (This is hairy)

// A pointer (index into array when 32-bit) to this struct
// is stored in the info-plane of each animated tile.

typedef struct RF_AnimTileTimer
{
	int tileNumber;
	int timeToSwitch;
} RF_AnimTileTimer;

typedef struct RF_OnscreenAnimTile
{
	int tileX;
	int tileY;
	int tile;
	int plane;
	int timerIndex;
	struct RF_OnscreenAnimTile *next;
	struct RF_OnscreenAnimTile *prev;
} RF_OnscreenAnimTile;

#define RF_MAX_ANIMTILETIMERS 180
#define RF_MAX_ONSCREENANIMTILES 90

int rf_numAnimTileTimers;
RF_AnimTileTimer rf_animTileTimers[RF_MAX_ANIMTILETIMERS];

RF_OnscreenAnimTile rf_onscreenAnimTiles[RF_MAX_ONSCREENANIMTILES];
RF_OnscreenAnimTile *rf_firstOnscreenAnimTile, *rf_freeOnscreenAnimTile;

void RF_SetScrollBlock(int tileX, int tileY, bool vertical)
{
	if (!vertical)
	{
		if (rf_numHorzScrollBlocks == RF_MAX_SCROLLBLOCKS)
			Quit("RF_SetScrollBlock: Too many horizontal scroll blocks");
		rf_horzScrollBlocks[rf_numHorzScrollBlocks] = tileX;
		rf_numHorzScrollBlocks++;
	}
	else
	{
		if (rf_numVertScrollBlocks == RF_MAX_SCROLLBLOCKS)
			Quit("RF_SetScrollBlock: Too many vertical scroll blocks");
		rf_vertScrollBlocks[rf_numVertScrollBlocks] = tileY;
		rf_numVertScrollBlocks++;
	}
}

void RFL_SetupOnscreenAnimList()
{
	rf_freeOnscreenAnimTile = rf_onscreenAnimTiles;
	
	for (int i = 0; i < RF_MAX_ONSCREENANIMTILES - 1; ++i)
	{
		rf_onscreenAnimTiles[i].next = &rf_onscreenAnimTiles[i+1];
	}

	rf_onscreenAnimTiles[RF_MAX_ONSCREENANIMTILES - 1].next = 0;

	rf_firstOnscreenAnimTile = 0;
}

void RFL_SetupSpriteTable()
{
	rf_freeSpriteTableEntry = rf_spriteTable;

	for (int i = 0; i < RF_MAX_SPRITETABLEENTRIES - 1; ++i)
	{
		rf_spriteTable[i].next = &rf_spriteTable[i+1];
	}

	rf_spriteTable[RF_MAX_SPRITETABLEENTRIES - 1].next = 0;

	for (int i = 0; i < RF_NUM_SPRITE_Z_LAYERS; ++i)
	{
		rf_firstSpriteTableEntry[i] = 0;
	}
}

void RF_MarkTileGraphics()
{
	rf_numAnimTileTimers = 0;
	for (int tileY = 0; tileY < rf_mapHeightTiles; ++tileY)
	{
		for (int tileX = 0; tileX < rf_mapWidthTiles; ++tileX)
		{
			bool needNewTimer = true;
			int backTile = CA_mapPlanes[0][tileY*rf_mapWidthTiles+tileX];
			int foreTile = CA_mapPlanes[1][tileY*rf_mapWidthTiles+tileX];
			if (TI_BackAnimTile(backTile))
			{
				// Is the info-plane free?
				if (CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX])
					Quit("RF_MarkTileGraphics: Info plane above animated bg tile in use.");

		
				for (int i = 0; i < rf_numAnimTileTimers; ++i)
				{
					if (rf_animTileTimers[i].tileNumber == backTile)
					{
						// Add the timer index to the info-plane
						CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX] = i | 0xfe00;
						needNewTimer = false;
						break;
					}
				}

				if (needNewTimer) 
				{
					if (rf_numAnimTileTimers >= RF_MAX_ANIMTILETIMERS)
						Quit("RF_MarkTileGraphics: Too many unique animations");
					rf_animTileTimers[rf_numAnimTileTimers].tileNumber = backTile;
					rf_animTileTimers[rf_numAnimTileTimers].timeToSwitch = TI_BackAnimTime(backTile);
					CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX] = rf_numAnimTileTimers | 0xfe00;
					rf_numAnimTileTimers++;
				}
			}
			else if (TI_ForeAnimTile(foreTile))
			{
				// Is the info-plane free?
				//if (CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX])
				//	Quit("RF_MarkTileGraphics: Info plane above animated fg tile in use.");

				for (int i = 0; i < rf_numAnimTileTimers; ++i)
				{
					if (rf_animTileTimers[i].tileNumber == (foreTile | 0x8000))
					{
						// Add the timer index to the info-plane
						CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX] = i | 0xfe00;
						needNewTimer = false;
						break;
					}
				}

				if (needNewTimer) 
				{
					if (rf_numAnimTileTimers >= RF_MAX_ANIMTILETIMERS)
						Quit("RF_MarkTileGraphics: Too many unique animations");
			
					rf_animTileTimers[rf_numAnimTileTimers].tileNumber = foreTile | 0x8000;
					rf_animTileTimers[rf_numAnimTileTimers].timeToSwitch = TI_ForeAnimTime(foreTile);
					CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX] = rf_numAnimTileTimers | 0xfe00;
					rf_numAnimTileTimers++;
				}

			}

		}
	}
}


void RFL_CheckForAnimTile(int tileX, int tileY)
{
	int backTile = CA_mapPlanes[0][tileY*rf_mapWidthTiles+tileX];
	int foreTile = CA_mapPlanes[1][tileY*rf_mapWidthTiles+tileX];


	if (TI_BackAnimTile(backTile) != 0 && TI_BackAnimTime(backTile) != 0)
	{
		if (!rf_freeOnscreenAnimTile)
			Quit("RFL_CheckForAnimTile: No free spots in tilearray!");

		RF_OnscreenAnimTile *ost = rf_freeOnscreenAnimTile;
		rf_freeOnscreenAnimTile = rf_freeOnscreenAnimTile->next;
	
		ost->tileX = tileX;
		ost->tileY = tileY;
		ost->tile = backTile;
		ost->plane = 0;
		ost->timerIndex = CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX] & 0x1ff;


		ost->next = rf_firstOnscreenAnimTile;
		ost->prev = 0;
		rf_firstOnscreenAnimTile = ost;
	}

	if (TI_ForeAnimTile(foreTile) != 0 && TI_ForeAnimTime(foreTile) != 0)
	{
		if (!rf_freeOnscreenAnimTile)
			Quit("RFL_CheckForAnimTile: No free spots in tilearray!");

		RF_OnscreenAnimTile *ost = rf_freeOnscreenAnimTile;
		rf_freeOnscreenAnimTile = rf_freeOnscreenAnimTile->next;
	
		ost->tileX = tileX;
		ost->tileY = tileY;
		ost->tile = foreTile;
		ost->plane = 1;
		ost->timerIndex = CA_mapPlanes[2][tileY*rf_mapWidthTiles+tileX] & 0x1ff;

		ost->prev = 0;
		ost->next = rf_firstOnscreenAnimTile;
		rf_firstOnscreenAnimTile = ost;
	}
}

void RFL_RemoveAnimRect(int tileX, int tileY, int tileW, int tileH)
{
	RF_OnscreenAnimTile *ost, *prev;

	prev = 0;
	ost = rf_firstOnscreenAnimTile;


	while (ost)
	{
		if ((ost->tileX >= tileX && ost->tileX < (tileX+tileW)) && (ost->tileY >= tileY && ost->tileY < (tileY+tileH)))
		{
			RF_OnscreenAnimTile *obsolete = ost;
			if (prev)
				prev->next = ost->next;
			else
				rf_firstOnscreenAnimTile = ost->next;
			ost = (prev)?prev:rf_firstOnscreenAnimTile;
			obsolete->next = rf_freeOnscreenAnimTile;
			rf_freeOnscreenAnimTile = obsolete;
			continue;
		}
		prev = ost;
		ost = ost->next;
	}
}

void RFL_RemoveAnimCol(int tileX)
{
	RF_OnscreenAnimTile *ost, *prev;

	prev = 0;
	ost = rf_firstOnscreenAnimTile;

	//printf("RFL_RemoveAnimCol: %d\n", tileX);

	while (ost)
	{
		if (ost->tileX == tileX)
		{
			RF_OnscreenAnimTile *obsolete = ost;
			if (prev)
				prev->next = ost->next;
			else
				rf_firstOnscreenAnimTile = ost->next;
			ost = (prev)?prev:rf_firstOnscreenAnimTile;
			obsolete->next = rf_freeOnscreenAnimTile;
			rf_freeOnscreenAnimTile = obsolete;
			continue;
		}
		prev = ost;
		ost = ost->next;
	}
}

void RFL_RemoveAnimRow(int tileY)
{
	RF_OnscreenAnimTile *ost, *prev;

	prev = 0;
	
	ost = rf_firstOnscreenAnimTile;

	//printf("RFL_RemoveAnimRow: %d\n", tileY);
	
	while (ost)
	{
		if (ost->tileY == tileY)
		{
			RF_OnscreenAnimTile *obsolete = ost;
			if (prev)
				prev->next = ost->next;
			else
				rf_firstOnscreenAnimTile = ost->next;
			ost = (prev)?prev:rf_firstOnscreenAnimTile;
			obsolete->next = rf_freeOnscreenAnimTile;
			rf_freeOnscreenAnimTile = obsolete;
			continue;
		}
		prev = ost;
		ost = ost->next;
	}
}

void RFL_AnimateTiles()
{
	// Update the timers.

	for (int i = 0; i < rf_numAnimTileTimers; ++i)
	{
		rf_animTileTimers[i].timeToSwitch--;


		if (rf_animTileTimers[i].timeToSwitch <= 0)
		{
			if (rf_animTileTimers[i].tileNumber & 0x8000)
			{
				int tile = rf_animTileTimers[i].tileNumber & ~0x8000;
				//printf("RFL_AnimateTiles: Updating fore %d to %d\n", tile, tile + TI_ForeAnimTile(tile));
				tile += TI_ForeAnimTile(tile);
				rf_animTileTimers[i].timeToSwitch += TI_ForeAnimTime(tile);
				rf_animTileTimers[i].tileNumber = tile | 0x8000;
			}
			else
			{
				int tile = rf_animTileTimers[i].tileNumber;
				//printf("RFL_AnimateTiles: Updating back %d to %d\n", tile, tile + TI_BackAnimTile(tile));
				tile += TI_BackAnimTile(tile);
				rf_animTileTimers[i].timeToSwitch += TI_BackAnimTime(tile);
				rf_animTileTimers[i].tileNumber = tile;
			}
		}
	}

	// Update the onscreen tiles.
	for (RF_OnscreenAnimTile *ost = rf_firstOnscreenAnimTile; ost; ost = ost->next)
	{
		int tile = rf_animTileTimers[ost->timerIndex].tileNumber & ~0x8000;
		if (tile != ost->tile)
		{
			ost->tile = tile;
			int screenTileX = ost->tileX - (rf_scrollXUnit >> 8);
			int screenTileY = ost->tileY - (rf_scrollYUnit >> 8);

			if (screenTileX < 0 || screenTileX > RF_BUFFER_WIDTH_TILES || screenTileY < 0 || screenTileY > RF_BUFFER_HEIGHT_TILES)
			{
				printf("Out of bounds: %d, %d (sc: %d,%d) (tl: %d,%d,%d):%d\n", screenTileX, screenTileY,
					rf_scrollXUnit >> 8, rf_scrollYUnit >> 8, ost->tileX, ost->tileY,ost->plane, ost->tile);
				Quit("RFL_AnimateTiles: Out of bounds!");
			}

			CA_mapPlanes[ost->plane][ost->tileY*rf_mapWidthTiles+ost->tileX] = tile;

			RF_RenderTile16(screenTileX, screenTileY, CA_mapPlanes[0][ost->tileY*rf_mapWidthTiles+ost->tileX]);
			RF_RenderTile16m(screenTileX, screenTileY, CA_mapPlanes[1][ost->tileY*rf_mapWidthTiles+ost->tileX]);
		}
	}
}


void RF_Startup()
{
	// Create the tile backing buffer
	rf_tileBuffer = VL_CreateSurface(RF_BUFFER_WIDTH_PIXELS, RF_BUFFER_HEIGHT_PIXELS);

}

void RF_NewMap(int mapNum)
{
	rf_mapWidthTiles = CA_MapHeaders[mapNum]->width;
	rf_mapHeightTiles = CA_MapHeaders[mapNum]->height;
	rf_scrollXMinUnit = 0x0200;		//Two-tile wide border around map
	rf_scrollYMinUnit = 0x0200;
	rf_scrollXMaxUnit = ((CA_MapHeaders[mapNum]->width - RF_SCREEN_WIDTH_TILES - 2) << 8);
	rf_scrollYMaxUnit = ((CA_MapHeaders[mapNum]->height - RF_SCREEN_HEIGHT_TILES - 2) << 8);

	// Reset the scroll-blocks
	rf_numVertScrollBlocks = rf_numHorzScrollBlocks = 0;

	RFL_SetupOnscreenAnimList();
	RFL_SetupSpriteTable();
	RF_MarkTileGraphics();

	// Set-up a two-tile wide border
	RF_SetScrollBlock(0,1,true);
	RF_SetScrollBlock(0,CA_MapHeaders[mapNum]->height-2,true);
	RF_SetScrollBlock(1,0,false);
	RF_SetScrollBlock(CA_MapHeaders[mapNum]->width-2,0,false);

}

void RF_RenderTile16(int x, int y, int tile)
{
	CA_CacheGrChunk(ca_gfxInfoE.offTiles16+tile);
	VL_UnmaskedToSurface(ca_graphChunks[ca_gfxInfoE.offTiles16+tile],rf_tileBuffer,x*16,y*16,16,16);
}

void RF_RenderTile16m(int x, int y, int tile)
{
	if (!tile) return;
	CA_CacheGrChunk(ca_gfxInfoE.offTiles16m+tile);
	VL_MaskedBlitToSurface(ca_graphChunks[ca_gfxInfoE.offTiles16m+tile],rf_tileBuffer,x*16,y*16,16,16);
}

void RF_ReplaceTiles(int16_t *tilePtr, int plane, int dstX, int dstY, int width, int height)
{
	RFL_RemoveAnimRect(dstX, dstY, width, height);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			int dstTileX = dstX + x;
			int dstTileY = dstY + y;
		
			int tileScreenX = dstTileX - (rf_scrollXUnit >> 8);
			int tileScreenY = dstTileY - (rf_scrollYUnit >> 8);
			int oldTile = CA_mapPlanes[plane][dstTileY*rf_mapWidthTiles+dstTileX];
			int newTile = tilePtr[y*width+x];
			if (oldTile != newTile)
			{
				CA_mapPlanes[plane][dstTileY*rf_mapWidthTiles+dstTileX] = newTile;;
				//tilePtr[y*width+x] = oldTile;
				if (tileScreenX >= 0 && tileScreenX < RF_BUFFER_WIDTH_TILES &&
					tileScreenY >= 0 && tileScreenY < RF_BUFFER_HEIGHT_TILES)
				{
					RF_RenderTile16(tileScreenX, tileScreenY, CA_mapPlanes[0][dstTileY*rf_mapWidthTiles+dstTileX]);
					RF_RenderTile16m(tileScreenX, tileScreenY, CA_mapPlanes[1][dstTileY*rf_mapWidthTiles+dstTileX]);
				}
			}
			RFL_CheckForAnimTile(dstTileX, dstTileY);
		}
	}
}

void RFL_RenderForeTiles()
{
	int scrollXtile = rf_scrollXUnit >> 8;
	int scrollYtile = rf_scrollYUnit >> 8;

	int scrollOffsetX = (rf_scrollXUnit & 0xFF) >> 4;
	int scrollOffsetY = (rf_scrollYUnit & 0xFF) >> 4;

	for (int stx =  scrollXtile; stx < scrollXtile + RF_BUFFER_WIDTH_TILES; ++stx)
	{
		for (int sty = scrollYtile; sty < scrollYtile + RF_BUFFER_HEIGHT_TILES; ++sty)
		{
			int tile = CA_mapPlanes[1][sty*rf_mapWidthTiles+stx];
			if (!tile) continue;
			if (!(TI_ForeMisc(tile) & 0x80)) continue;
			VL_MaskedBlitToScreen(ca_graphChunks[ca_gfxInfoE.offTiles16m+tile],(stx - scrollXtile)*16-scrollOffsetX,
											(sty - scrollYtile)*16-scrollOffsetY,16,16);
		}
	}
}

// Renders a new horizontal row.
// dir: true = bottom, false = top
void RFL_NewRowHorz(bool dir)
{
	int bufferRow;
	int mapRow;
	if (dir)
	{
		bufferRow = RF_BUFFER_HEIGHT_TILES - 1;
	}
	else
	{
		bufferRow = 0;
	}
	mapRow = (rf_scrollYUnit >> 8) + bufferRow;
	int xOffset = (rf_scrollXUnit >> 8);

	// TODO: Add tiles to onscreen animation list
	for (int i = 0; i < RF_BUFFER_WIDTH_TILES; ++i)
	{
		RFL_CheckForAnimTile(i+xOffset,mapRow);
		RF_RenderTile16(i,bufferRow,CA_mapPlanes[0][mapRow*rf_mapWidthTiles+xOffset+i]);
		RF_RenderTile16m(i,bufferRow,CA_mapPlanes[1][mapRow*rf_mapWidthTiles+xOffset+i]);
	}
}

// Renders a new vertical row.
// dir: true = right, false = left
void RFL_NewRowVert(bool dir)
{
	int bufferCol;
	int mapCol;
	if (dir)
	{
		bufferCol = RF_BUFFER_WIDTH_TILES - 1;
	}
	else
	{
		bufferCol = 0;
	}
	mapCol = (rf_scrollXUnit >> 8) + bufferCol;
	int yOffset = (rf_scrollYUnit >> 8);

	// TODO: Check for animated tiles
	for (int i = 0; i < RF_BUFFER_HEIGHT_TILES; ++i)
	{
		RFL_CheckForAnimTile(mapCol,i+yOffset);
		RF_RenderTile16(bufferCol,i,CA_mapPlanes[0][(yOffset+i)*rf_mapWidthTiles+mapCol]);
		RF_RenderTile16m(bufferCol,i,CA_mapPlanes[1][(yOffset+i)*rf_mapWidthTiles+mapCol]);
	}
}


void RF_RepositionLimit(int scrollXunit, int scrollYunit)
{
	rf_scrollXUnit = scrollXunit;
	rf_scrollYUnit = scrollYunit;

	if (scrollXunit < rf_scrollXMinUnit) rf_scrollXUnit = rf_scrollXMinUnit;
	if (scrollYunit < rf_scrollYMinUnit) rf_scrollYUnit = rf_scrollYMinUnit;
	if (scrollXunit > rf_scrollXMaxUnit) rf_scrollXUnit = rf_scrollXMaxUnit;
	if (scrollYunit > rf_scrollYMaxUnit) rf_scrollYUnit = rf_scrollYMaxUnit;

	int scrollXtile = rf_scrollXUnit >> 8;
	int scrollYtile = rf_scrollYUnit >> 8;

}

void RFL_SmoothScrollLimit(int scrollXdelta, int scrollYdelta)
{
	rf_scrollXUnit += scrollXdelta;
	rf_scrollYUnit += scrollYdelta;

	int scrollXtile = rf_scrollXUnit >> 8;
	int scrollYtile = rf_scrollYUnit >> 8;

	if (scrollXdelta > 0)
	{
		scrollXtile += RF_SCREEN_WIDTH_TILES;
		for (int sb = 0; sb < rf_numHorzScrollBlocks; ++sb)
		{
			if (rf_horzScrollBlocks[sb] == scrollXtile)
			{
				rf_scrollXUnit &= ~0xff;
				break;
			}
		}
	}
	else if (scrollXdelta < 0)
	{
		for (int sb = 0; sb < rf_numHorzScrollBlocks; ++sb)
		{
			if (rf_horzScrollBlocks[sb] == scrollXtile)
			{
				rf_scrollXUnit &= ~0xff;
				rf_scrollXUnit += 256;
			}
		}
	}

	if (scrollYdelta > 0)
	{
		scrollYtile += RF_SCREEN_HEIGHT_TILES;
		for (int sb = 0; sb < rf_numVertScrollBlocks; ++sb)
		{
			if (rf_vertScrollBlocks[sb] == scrollYtile)
			{
				rf_scrollYUnit &= ~0xff;
				break;
			}
		}
	}
	else if (scrollYdelta < 0)
	{
		for (int sb = 0; sb < rf_numVertScrollBlocks; ++sb)
		{
			if (rf_vertScrollBlocks[sb] == scrollYtile)
			{
				rf_scrollYUnit &= ~0xff;
				rf_scrollYUnit += 256;
			}
		}
	}


}


void RF_Reposition(int scrollXunit, int scrollYunit)
{
	//TODO: Implement scrolling properly
	//NOTE: This should work now.
	RF_RepositionLimit(scrollXunit, scrollYunit);
	int scrollXtile = rf_scrollXUnit >> 8;
	int scrollYtile = rf_scrollYUnit >> 8;

	RFL_SetupOnscreenAnimList();


	for (int ty = 0; ty < RF_BUFFER_HEIGHT_TILES; ++ty)
	{
		for (int tx = 0; tx < RF_BUFFER_WIDTH_TILES; ++tx)
		{
			RFL_CheckForAnimTile(tx+scrollXtile,ty+scrollYtile);
			RF_RenderTile16(tx,ty,CA_mapPlanes[0][(ty+scrollYtile) * rf_mapWidthTiles + tx + scrollXtile]);
			RF_RenderTile16m(tx,ty,CA_mapPlanes[1][(ty+scrollYtile) * rf_mapWidthTiles + tx + scrollXtile]);
		}
	}
}

void RF_SmoothScroll(int scrollXdelta, int scrollYdelta)
{
	int oldScrollXTile = (rf_scrollXUnit >> 8), oldScrollYTile = (rf_scrollYUnit >> 8);
	RFL_SmoothScrollLimit(scrollXdelta,scrollYdelta);
	int scrollXTileDelta = (rf_scrollXUnit >> 8) - oldScrollXTile;
	int scrollYTileDelta = (rf_scrollYUnit >> 8) - oldScrollYTile;
	

	// If we're not moving to a new tile at all, we can quit now.
	if (scrollXTileDelta == 0 && scrollYTileDelta == 0)
		return;


	if (scrollXTileDelta > 1 || scrollXTileDelta < -1 || scrollYTileDelta > 1 || scrollYTileDelta < -1)
	{
		// We redraw the whole thing if we move too much.
		RF_Reposition(rf_scrollXUnit, rf_scrollYUnit);
		return;
	}


	int dest_x = (scrollXTileDelta < 0)?16:0;
	int dest_y = (scrollYTileDelta < 0)?16:0;

	int src_x = (scrollXTileDelta > 0)?16:0;
	int src_y = (scrollYTileDelta > 0)?16:0;

	int wOffset = (scrollXTileDelta)?-16:0;
	int hOffset = (scrollYTileDelta)?-16:0;

	VL_SurfaceToSelf(rf_tileBuffer,dest_x,dest_y,src_x,src_y,RF_BUFFER_WIDTH_PIXELS+wOffset, RF_BUFFER_HEIGHT_PIXELS+hOffset);

	if (scrollXTileDelta)
	{
		RFL_NewRowVert((scrollXTileDelta>0));
		if (scrollXTileDelta>0)
		{
			RFL_RemoveAnimCol((rf_scrollXUnit >> 8) - 1);
		}
		else
		{
			RFL_RemoveAnimCol((rf_scrollXUnit >> 8) + RF_BUFFER_WIDTH_TILES);
		}
	}

	if (scrollYTileDelta)
	{
		RFL_NewRowHorz((scrollYTileDelta>0));
		if (scrollYTileDelta>0)
		{
			RFL_RemoveAnimRow((rf_scrollYUnit >> 8) - 1);
		}
		else
		{
			RFL_RemoveAnimRow((rf_scrollYUnit >> 8) + RF_BUFFER_HEIGHT_TILES);
		}
	}
}

static int rf_numSpriteDraws;


void RF_FindSpriteCirle(RF_SpriteDrawEntry *de)
{
	if (!de) return;
	RF_SpriteDrawEntry *nde = de->next;
	while (nde)
	{
		if (de == nde)
		{
			Quit("Sprite list FAIL!");
		}
		nde = nde->next;
	}
}


void RF_RemoveSpriteDraw(RF_SpriteDrawEntry **drawEntry)
{
	if (!drawEntry) return;
	if (!(*drawEntry)) return;

	rf_numSpriteDraws--;
	printf("Removing SpriteDraw: %d of %d\n",rf_numSpriteDraws, RF_MAX_SPRITETABLEENTRIES);
#if 0
	if (rf_firstSpriteTableEntry[(*drawEntry)->zLayer] == (*drawEntry))
	{
		(*drawEntry)->prev = 0;
		rf_firstSpriteTableEntry[(*drawEntry)->zLayer] = (*drawEntry)->next;
	}

	if ((*drawEntry)->prev)
		(*drawEntry)->prev->next = (*drawEntry)->next;

#endif

	if ((*drawEntry)->next)
		(*drawEntry)->next->prevNextPtr = (*drawEntry)->prevNextPtr;

	(*((*drawEntry)->prevNextPtr)) = (*drawEntry)->next;
	(*drawEntry)->next = rf_freeSpriteTableEntry;
	rf_freeSpriteTableEntry = *drawEntry;
	*drawEntry = 0;

	RF_FindSpriteCirle(*drawEntry);
}


void RF_AddSpriteDraw(RF_SpriteDrawEntry **drawEntry, int unitX, int unitY, int chunk, bool allWhite, int zLayer)
{
	bool insertNeeded = true;
	if (chunk <= 0)
	{
		//TODO: Implement RemoveSpriteDraw
		RF_RemoveSpriteDraw(drawEntry);
		return;
	}

	RF_SpriteDrawEntry *sde = *drawEntry;

	if (sde)
	{
		//TODO: Add sprite eraser to remove previous position.

		//TODO: Support changing zLayers properly.
		if (zLayer == sde->zLayer)
		{
			insertNeeded = false;
		}
		else
		{
			if (sde->next)
			{
				sde->next->prevNextPtr = sde->prevNextPtr;
			}
			*(sde->prevNextPtr) = sde->next;
		}
	}
	else if (rf_freeSpriteTableEntry)
	{
		// Grab a new spritedraw pointer.
		sde = rf_freeSpriteTableEntry;
		rf_freeSpriteTableEntry = rf_freeSpriteTableEntry->next;
		rf_numSpriteDraws++;
		RF_FindSpriteCirle(sde);
		printf("Allocating SpriteDraw: %d of %d\n",rf_numSpriteDraws, RF_MAX_SPRITETABLEENTRIES);
	}
	else
	{
		Quit("RF_AddSpriteDraw: No free spots in spritearray");
	}

	// Add the SpriteDrawEntry to the table for its z-layer.

	if (insertNeeded)
	{
		if (rf_firstSpriteTableEntry[zLayer])
			rf_firstSpriteTableEntry[zLayer]->prevNextPtr = &sde->next;	
		sde->next = rf_firstSpriteTableEntry[zLayer];
		rf_firstSpriteTableEntry[zLayer] = sde;
		sde->prevNextPtr = &rf_firstSpriteTableEntry[zLayer];
	}

	//TODO: Remove
	CA_CacheGrChunk(chunk);

	void *sprite_data = ca_graphChunks[chunk];
	if (!sprite_data)
		Quit("RF_AddSpriteDraw: Placed an uncached sprite");

	int sprite_number = chunk - ca_gfxInfoE.offSprites;

	

	sde->chunk = chunk;
	sde->zLayer = zLayer;
	sde->x = (unitX >> 4);
	sde->y = (unitY >> 4);
	
	*drawEntry = sde;
}		

void RFL_DrawSpriteList()
{
	for (int zLayer = 0; zLayer < RF_NUM_SPRITE_Z_LAYERS; ++zLayer)
	{
		// All but the final z layer (3) are below fore-foreground tiles.
		if (zLayer == 3)
			RFL_RenderForeTiles();
		
		for (RF_SpriteDrawEntry *sde = rf_firstSpriteTableEntry[zLayer]; sde; sde=sde->next)
		{
			int pixelX = sde->x - (rf_scrollXUnit >> 4);
			int pixelY = sde->y - (rf_scrollYUnit >> 4);
			
			VH_DrawSprite(pixelX, pixelY, sde->chunk);
		}
	}
}

RF_SpriteDrawEntry *tmp = 0;

void RF_Refresh()
{
	//TODO: Everything
	int scrollXpixeloffset = (rf_scrollXUnit & 0xff) >> 4;
	int scrollYpixeloffset = (rf_scrollYUnit & 0xff) >> 4;


		
	

	RFL_AnimateTiles();

	VL_SurfaceToScreen(rf_tileBuffer,0,0,scrollXpixeloffset,scrollYpixeloffset,320,200);

	RFL_DrawSpriteList();

	CA_CacheGrChunk(ca_gfxInfoE.offTiles8m);

/*	for(int i = 0; i < ca_gfxInfoE.numTiles8m ; ++i)
	{
		VH_DrawTile8M((i%11)*8,(i/11)*8,i);
	}
*/

	//US_CenterWindow(10,6);

	//US_Print("Hello, World!\nGoodbye!\n");


	//VL_Present();
}
