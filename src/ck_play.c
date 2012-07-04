//#inclucd "id_heads.h"
#include "ck_play.h"
#include "ck_def.h"
#include "id_ca.h"
#include "id_in.h"
#include "id_us.h"
#include "id_rf.h"
#include "id_vl.h"

#include "ck5_ep.h"

#include <stdio.h> /* For printf() debugging */
#include <stdlib.h> /* For abs() */

#define max(a,b) ((a<b)?b:a)
#define min(a,b) ((a<b)?a:b)

CK_object ck_objArray[CK_MAX_OBJECTS];

CK_object *ck_freeObject;
CK_object *ck_lastObject;
int ck_numObjects;

CK_object *ck_keenObj;

static CK_object tempObj;

static long ck_numTotalTics;
static int ck_ticsThisFrame;

// The rectangle within which objects are active.
static int ck_activeX0Tile;
static int ck_activeY0Tile;
static int ck_activeX1Tile;
static int ck_activeY1Tile;


void CK_KeenCheckSpecialTileInfo(CK_object *obj);

bool ck_demoEnabled;

CK_GameState ck_gameState;


void CK_DebugMemory()
{
	US_CenterWindow(16,10);

	US_CPrint("Memory Usage:");
	US_CPrint("-------------");
	US_PrintF("In Use      : %dk", MM_UsedMemory()/1024);
	US_PrintF("Blocks      : %d", MM_UsedBlocks());
	US_PrintF("Purgable    : %d", MM_PurgableBlocks());
	US_PrintF("GFX Mem Used: %dk", VL_MemUsed()/1024);
	US_PrintF("GFX Surfaces: %d", VL_NumSurfaces());
	VL_Present();
	IN_WaitKey();
	//MM_ShowMemory();
}




void CK_SetTicsPerFrame()
{
	if (!ck_demoEnabled)
		ck_ticsThisFrame = VL_GetTics(true);
	else
	{
		VL_GetTics(true);
		ck_ticsThisFrame = 2;
	}
	ck_numTotalTics += ck_ticsThisFrame;
}

int CK_GetTicksPerFrame()
{
	return ck_ticsThisFrame;
}

long CK_GetNumTotalTics()
{
	return ck_numTotalTics;
}


void CK_SetupObjArray()
{
	for (int i = 0; i < CK_MAX_OBJECTS; ++i)
	{
		ck_objArray[i].prev = &(ck_objArray[i+1]);
		ck_objArray[i].next = 0;
	}

	ck_objArray[CK_MAX_OBJECTS-1].prev = 0;

	ck_freeObject = &ck_objArray[0];
	ck_lastObject = 0;
	ck_numObjects = 0;

	ck_keenObj = CK_GetNewObj(false);

	// TODO: Add Andy's `special object'?
}

CK_object *CK_GetNewObj(bool nonCritical)
{
	if (!ck_freeObject)
	{
		if (nonCritical)
		{
			printf("Warning: No free spots in objarray! Using temp object\n");
			return &tempObj;
		}
		else
			Quit("GetNewObj: No free spots in objarray!");
	}

	CK_object *newObj = ck_freeObject;
	ck_freeObject = ck_freeObject->prev;

	//Clear any old crap out of the struct.
	//memset(newObj, 0, sizeof(CK_object));


	if (ck_lastObject)
	{
		ck_lastObject->next = newObj;
	}
	newObj->prev = ck_lastObject;
	

	newObj->active = true;
	newObj->clipped = true;

	ck_lastObject = newObj;
	ck_numObjects++;

	printf("Added object %d\n",ck_numObjects);

	return newObj;
}


void CK_RemoveObj(CK_object *obj)
{
	printf("Removing object %d\n",ck_numObjects);
	if (obj == ck_keenObj)
	{
		Quit("RemoveObj: Tried to remove the player!");
	}

	if (obj == ck_lastObject)
		ck_lastObject = obj->prev;
	else
		obj->next->prev = obj->prev;
	
	obj->prev->next = obj->next;
	obj->next = 0;

	obj->prev = ck_freeObject;
	ck_freeObject = obj;
	ck_numObjects--;
}


int CK_ActionThink(CK_object *obj, int time)
{
	CK_action *action = obj->currentAction;


	// ThinkMethod: 2
	if (obj->currentAction->type == AT_Frame)
	{
		if (obj->currentAction->think)
		{
			if (obj->timeUntillThink)
			{
				printf("ThinkType = 2 // decrementing timeThink %d\n", obj->timeUntillThink);
				obj->timeUntillThink--;
			}
			else
			{
				obj->currentAction->think(obj);
			}
		}
		return 0;
	}

	//printf("CK_ActionThink: type: %d actionDelay: %d actionTimer: %d, timeUntillThink: %d\n", action->type, action->timer, obj->actionTimer, obj->timeUntillThink);

	int newTime = time + obj->actionTimer;

	// If we won't run out of time.
	if (action->timer > newTime || !(action->timer))
	{
		obj->actionTimer = newTime;

		if (action->type == AT_ScaledOnce || action->type == AT_ScaledFrame)
		{
			if (obj->xDirection)
			{
				//TODO: Work out what to do with the nextVelocity stuff.
				obj->nextX += action->velX * time * obj->xDirection;
			}
			if (obj->yDirection)
			{
				obj->nextY += action->velY * time * obj->yDirection;
			}
		}

		if (action->type == AT_UnscaledFrame || action->type == AT_ScaledFrame)
		{
			if (action->think)
			{
				if (obj->timeUntillThink)
					obj->timeUntillThink--;
				else
				{
					obj->currentAction->think(obj);
				}
			}
		}
		return 0;
	}
	else
	{
		int remainingTime = action->timer - obj->actionTimer;
		newTime -= action->timer;
		obj->actionTimer = 0;

		if (action->type == AT_ScaledOnce || action->type == AT_ScaledFrame)
		{
			if (obj->xDirection)
			{
				obj->nextX += action->velX * remainingTime * obj->xDirection;
			}
			if (obj->yDirection)
			{
				obj->nextY += action->velY * remainingTime * obj->yDirection;
			}
		}
		else if (action->type == AT_UnscaledOnce || action->type == AT_UnscaledFrame)
		{
			if (obj->xDirection)
			{
				obj->nextX += action->velX * obj->xDirection;
			}
			if (obj->yDirection)
			{
				obj->nextY += action->velY * obj->yDirection;
			}
		}

		if (action->think)
		{
				if (obj->timeUntillThink)
					obj->timeUntillThink--;
				else
				{
					obj->currentAction->think(obj);
				}
		}

		if (action != obj->currentAction)
		{
			if (!obj->currentAction)
			{
				return 0;
			}
		}
		else
		{
			obj->currentAction = action->next;
			//obj->actionTimer = obj->currentAction->timer;
			//printf("Changing action: new timer = %d\n",obj->actionTimer);
		}
	}
	return newTime;
}

void CK_RunAction(CK_object *obj)
{
	int oldChunk = obj->gfxChunk;
	int tics = CK_GetTicksPerFrame();

	int oldPosX = obj->posX;
	int oldPosY = obj->posY;

	obj->deltaPosX = obj->deltaPosY = 0;

	obj->nextX = obj->nextY = 0;

	CK_action *prevAction = obj->currentAction;

	int ticsLeft = CK_ActionThink(obj, tics);

	if (obj->currentAction != prevAction)
	{
		obj->actionTimer = 0;
		prevAction = obj->currentAction;
	}

	while (obj->currentAction && ticsLeft)
	{
		if (obj->currentAction->protectAnimation || obj->currentAction->timer > ticsLeft)
		{
			ticsLeft = CK_ActionThink(obj, ticsLeft);
		}
		else
		{
			ticsLeft = CK_ActionThink(obj, obj->currentAction->timer - 1);
		}

		if (obj->currentAction != prevAction)
		{
			obj->actionTimer = 0;
			prevAction = obj->currentAction;
		}
	
		//TODO: This should not be needed.	
		if (!obj->currentAction)
			break;
	}
	
	if (!obj->currentAction)
	{
		RF_RemoveSpriteDraw(&obj->sde);
		CK_RemoveObj(obj);
		return;
	}

	if (obj->currentAction->chunkLeft && obj->xDirection <= 0) obj->gfxChunk = obj->currentAction->chunkLeft;
	if (obj->currentAction->chunkRight && obj->xDirection > 0) obj->gfxChunk = obj->currentAction->chunkRight;

	if (obj->gfxChunk != oldChunk || obj->nextX || obj->nextY)
	{
		CK_PhysUpdateNormalObj(obj);
	}
}

void hackdraw(CK_object *me)
{
	RF_AddSpriteDraw(&(me->sde),(150 << 4),(100 << 4),121,false,0);
}

void CK_DebugKeys()
{
	if(IN_GetKeyState(IN_SC_M))
	{
		CK_DebugMemory();
	}
}

// Check non-game keys
void CK_CheckKeys()
{
	if(IN_GetKeyState(IN_SC_F10))
	{
		CK_DebugKeys();
	}
}

IN_ControlFrame ck_inputFrame;

void CK_HandleInput()
{
	IN_ReadControls(0, &ck_inputFrame);

	ck_keenState.jumpWasPressed = ck_keenState.jumpIsPressed;
	ck_keenState.pogoWasPressed = ck_keenState.pogoIsPressed;

	ck_keenState.jumpIsPressed = ck_inputFrame.jump;
	ck_keenState.pogoIsPressed = ck_inputFrame.pogo;

	if (!ck_keenState.jumpIsPressed) ck_keenState.jumpWasPressed = false;
	if (!ck_keenState.pogoIsPressed) ck_keenState.pogoWasPressed = false;

	CK_CheckKeys();

}

extern int rf_scrollXUnit;
extern int rf_scrollYUnit;

// Centre the camera on the given object.
void CK_CentreCamera(CK_object *obj)
{
	int screenX, screenY;

	if (obj->posX < (152 << 4))
		screenX = 0;
	else
		screenX = obj->posX - (152 << 4);

	if (obj->clipRects.unitY2 < (140 << 4))
		screenY = 0;
	else
		screenY = obj->posY - (140 << 4);

	RF_Reposition(screenX, screenY);

	//TODO: This is 4 in Andy's disasm.
	ck_activeX0Tile = max((rf_scrollXUnit >> 8) - 6, 0);
	ck_activeX1Tile = max((rf_scrollXUnit >> 8) + (320 >> 4) + 6, 0);
	ck_activeY0Tile = max((rf_scrollYUnit >> 8) - 6, 0);
	ck_activeY1Tile = max((rf_scrollYUnit >> 8) + (200 >> 4) + 6, 0);
}

// Run the normal camera which follows keen
void CK_NormalCamera(CK_object *obj)
{
	//TODO: some unknown var must be 0
	
	//TODO: Check if keen is outside map bounds.

	int deltaX = 0, deltaY = 0;	// in Units

	int screenYpx = 140 << 4;

	// Keep keen's x-coord between 144-192 pixels
	if (obj->posX < (rf_scrollXUnit + (144 << 4)))
		deltaX = obj->posX - (rf_scrollXUnit+(144 << 4));
	
	if (obj->posX > (rf_scrollXUnit + (192 << 4)))
		deltaX = obj->posX - (rf_scrollXUnit+(192 << 4));


	// If we're attached to the ground, or otherwise awesome
	// do somethink inscrutible.
	if (obj->topTI || !obj->clipped)
	{
		deltaY += obj->deltaPosY;

		//TODO: Something hideous
		if (screenYpx + rf_scrollYUnit + deltaY !=  (obj->clipRects.unitY2))
		{
			int adjAmt = ((screenYpx + rf_scrollYUnit + deltaY - obj->clipRects.unitY2));
			int adjAmt2 = abs(adjAmt / 8);

			adjAmt2 = (adjAmt2 <= 48)?adjAmt2:48;
			
			if (adjAmt > 0)
				deltaY -= adjAmt2;
			else
				deltaY += adjAmt2;
			
		}
				
	}
	

	if (obj->clipRects.unitY2 < (rf_scrollYUnit + deltaY + (32 << 4)))
		deltaY += obj->clipRects.unitY2 - (rf_scrollYUnit + deltaY + (32 << 4));

	
	if (obj->clipRects.unitY2 > (rf_scrollYUnit + deltaY + (168 << 4)))
		deltaY += obj->clipRects.unitY2 - (rf_scrollYUnit + deltaY + (168 << 4));

	//Don't scroll more than one tile's worth per frame.
	if (deltaX || deltaY)
	{
		if (deltaX > 255) deltaX = 255;
		else if (deltaX < -255) deltaX = -255;

		if (deltaY > 255) deltaY = 255;
		else if (deltaY < -255) deltaY = -255;

		RF_SmoothScroll(deltaX, deltaY);
	
		ck_activeX0Tile = max((rf_scrollXUnit >> 8) - 6, 0);
		ck_activeX1Tile = max((rf_scrollXUnit >> 8) + (320 >> 4) + 6, 0);
		ck_activeY0Tile = max((rf_scrollYUnit >> 8) - 6, 0);
		ck_activeY1Tile = max((rf_scrollYUnit >> 8) + (200 >> 4) + 6, 0);
	}
}

//TODO: Add some demo number stuff
void CK_PlayDemo(int demoChunk)
{
	uint8_t *demoBuf;
	CA_CacheGrChunk(demoChunk);
	demoBuf = ca_graphChunks[demoChunk];
	//MM_SetLock(&CA_CacheGrChunk
	
	uint16_t demoMap = *demoBuf;
	demoBuf += 2;
	uint16_t demoLen = *((uint16_t *)demoBuf);
	demoBuf += 2;

	ck_currentMapNumber =demoMap;
	CA_CacheMap(demoMap);
	RF_NewMap(demoMap);
	RF_Reposition(0,0);
	
	CK_SetupObjArray();
	
	ck_demoEnabled = true;

	ck_gameState.difficulty = D_Normal;

	IN_DemoStartPlaying(demoBuf,demoLen);

	CK_PlayLoop();
	
}


// Play a level.
int CK_PlayLoop()
{
	int levelState = 0;

	ck_gameState.difficulty = D_Easy;
	CK_SetupObjArray();
	CK5_ScanInfoLayer();
	//Hack!
	ck_numTotalTics = 0;


	CK_CentreCamera(ck_keenObj);
	while (levelState == 0)
	{

		IN_PumpEvents();
		CK_HandleInput();
		CK_SetTicsPerFrame();
		//int xd = 0, yd = 0;
		//if (IN_GetKeyState(IN_SC_LeftArrow)) xd -= 12;
		//if (IN_GetKeyState(IN_SC_RightArrow)) xd += 12;
		//if (IN_GetKeyState(IN_SC_UpArrow)) yd -= 12;
		//if (IN_GetKeyState(IN_SC_DownArrow)) yd += 12;

		// Set, unset active objects.
		for (CK_object *currentObj = ck_keenObj; currentObj; currentObj = currentObj->next)
		{

			if (!currentObj->active &&
				(currentObj->clipRects.tileX2 >= (rf_scrollXUnit >> 8) - 1) &&
				(currentObj->clipRects.tileX1 <= (rf_scrollXUnit >> 8) + (320 >> 4) + 1) &&
				(currentObj->clipRects.tileY1 <= (rf_scrollYUnit >> 8) + (200 >> 4) + 1) &&
				(currentObj->clipRects.tileY2 >= (rf_scrollYUnit >> 8) - 1))
			{
				currentObj->active = true;
				currentObj->visible = true;
			}
			else if (currentObj->active && currentObj != ck_keenObj && (
				(currentObj->clipRects.tileX2 <= ck_activeX0Tile) ||
				(currentObj->clipRects.tileX1 >= ck_activeX1Tile) ||
				(currentObj->clipRects.tileY1 >= ck_activeY1Tile) ||
				(currentObj->clipRects.tileY2 <= ck_activeY0Tile)))
			{
				RF_RemoveSpriteDraw(&currentObj->sde);
				//TODO: Add an Episode callback. Ep 4 requires
				// type 33 to remove int33 (Andy's decomp)
				currentObj->active = false;
				printf("Deactivating an object: (%d, %d), (%d, %d)-(%d,%d)\n", currentObj->clipRects.tileX2, currentObj->clipRects.tileY2, ck_activeX0Tile, ck_activeY0Tile, ck_activeX1Tile, ck_activeY1Tile);
				continue;
			}

			if (currentObj->active)
				CK_RunAction(currentObj);
		}
				
				
				

		for(CK_object *currentObj = ck_keenObj; currentObj; currentObj = currentObj->next)
		{
			// Some strange Keen4 stuff here. Ignoring for now.


			if (!currentObj->active) continue;
			for (CK_object *collideObj = currentObj->next; collideObj; collideObj = collideObj->next)
			{
				if (!collideObj->active)
					continue;

				if (	(currentObj->clipRects.unitX2 > collideObj->clipRects.unitX1) &&
					(currentObj->clipRects.unitX1 < collideObj->clipRects.unitX2) &&
					(currentObj->clipRects.unitY1 < collideObj->clipRects.unitY2) &&
					(currentObj->clipRects.unitY2 > collideObj->clipRects.unitY1) )
				{
					if (currentObj->currentAction->collide)
						currentObj->currentAction->collide(currentObj,collideObj);
					if (collideObj->currentAction->collide)
						collideObj->currentAction->collide(collideObj,currentObj);
				}
			}
		}

		//TODO: If world map and keen4, check wetsuit.

		//TODO: If not world map, check keen -> item-tile collision.

		CK_KeenCheckSpecialTileInfo(ck_keenObj);



		for(CK_object *currentObj = ck_keenObj; currentObj; currentObj = currentObj->next)
		{
			if (currentObj->active)
			{
				//TODO: Check if object has fallen off the bottom of the map.
				//CK_ActionThink(currentObj,1);	
				//else...
				if (currentObj->visible && currentObj->currentAction->draw)
				{
					currentObj->visible = false;	//We don't need to render it twice!
					currentObj->currentAction->draw(currentObj);
				}
			}
		}

		//TODO: Follow player with camera.

		RF_Refresh();
#if 0
		for (CK_object *obj = ck_keenObj; obj; obj = obj->next){
		VL_ScreenRect((obj->clipRects.tileX1 << 4) - (rf_scrollXUnit >> 4), (obj->clipRects.tileY1 << 4) - (rf_scrollYUnit>>4),
			(obj->clipRects.tileX2 - obj->clipRects.tileX1 + 1) << 4,
			(obj->clipRects.tileY2 - obj->clipRects.tileY1 + 1) << 4, 10);

		VL_ScreenRect((obj->clipRects.tileXmid << 4) - (rf_scrollXUnit >> 4), (obj->clipRects.tileY2 << 4) - (rf_scrollYUnit>>4),16,16,9);
		VL_ScreenRect((obj->clipRects.unitX1 >> 4) - (rf_scrollXUnit >> 4), (obj->clipRects.unitY1 >> 4) - (rf_scrollYUnit>>4),(obj->clipRects.unitX2 - obj->clipRects.unitX1) >> 4,(obj->clipRects.unitY2 - obj->clipRects.unitY1) >> 4,8);
		}
#endif
		//RF_SmoothScroll(xd,yd);
		CK_NormalCamera(ck_keenObj);
		//TODO: Slow-mo, extra VBLs.
		VL_Present();


	}
}