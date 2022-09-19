/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//

// cg_localents.c -- every frame, generate renderer commands for locally
// processed entities, like smoke puffs, gibs, shells, etc.

#include "cg_local.h"

#define	MAX_LOCAL_ENTITIES	8192
localEntity_t	cg_localEntities[MAX_LOCAL_ENTITIES];
localEntity_t	cg_activeLocalEntities;		// double linked list
localEntity_t	*cg_freeLocalEntities;		// single linked list

/*
===================
CG_InitLocalEntities

This is called at startup and for tournement restarts
===================
*/
void	CG_InitLocalEntities( void ) {
	int		i;

	memset( cg_localEntities, 0, sizeof( cg_localEntities ) );
	cg_activeLocalEntities.next = &cg_activeLocalEntities;
	cg_activeLocalEntities.prev = &cg_activeLocalEntities;
	cg_freeLocalEntities = cg_localEntities;
	for ( i = 0 ; i < MAX_LOCAL_ENTITIES - 1 ; i++ ) {
		cg_localEntities[i].next = &cg_localEntities[i+1];
	}
}


/*
==================
CG_FreeLocalEntity
==================
*/
void CG_FreeLocalEntity( localEntity_t *le ) {
	if ( !le->prev ) {
		CG_Error( "CG_FreeLocalEntity: not active" );
	}

	// remove from the doubly linked active list
	le->prev->next = le->next;
	le->next->prev = le->prev;

	// the free list is only singly linked
	le->next = cg_freeLocalEntities;
	cg_freeLocalEntities = le;
}

/*
===================
CG_AllocLocalEntity

Will allways succeed, even if it requires freeing an old active entity
===================
*/
localEntity_t	*CG_AllocLocalEntity( void ) {
	localEntity_t	*le;

	if ( !cg_freeLocalEntities ) {
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		CG_FreeLocalEntity( cg_activeLocalEntities.prev );
	}

	le = cg_freeLocalEntities;
	cg_freeLocalEntities = cg_freeLocalEntities->next;

	memset( le, 0, sizeof( *le ) );

	// link into the active list
	le->next = cg_activeLocalEntities.next;
	le->prev = &cg_activeLocalEntities;
	cg_activeLocalEntities.next->prev = le;
	cg_activeLocalEntities.next = le;
	return le;
}


/*
====================================================================================

FRAGMENT PROCESSING

A fragment localentity interacts with the environment in some way (hitting walls),
or generates more localentities along a trail.

====================================================================================
*/

/*
================
CG_BloodTrail

Leave expanding blood puffs behind gibs
================
*/
void CG_BloodTrail( localEntity_t *le ) {}


/*
================
CG_FragmentBounceMark
================
*/
void CG_FragmentBounceMark( localEntity_t *le, trace_t *trace ) {}

/*
================
CG_FragmentBounceSound
================
*/
void CG_FragmentBounceSound( localEntity_t *le, trace_t *trace ) {}


/*
================
CG_ReflectVelocity
================
*/
void CG_ReflectVelocity( localEntity_t *le, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = cg.time - cg.frametime + cg.frametime * trace->fraction;
	BG_EvaluateTrajectoryDelta( NULL, &le->pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, le->pos.trDelta );

	VectorScale( le->pos.trDelta, le->bounceFactor, le->pos.trDelta );

	VectorCopy( trace->endpos, le->pos.trBase );
	le->pos.trTime = cg.time;


	// check for stop, making sure that even on low FPS systems it doesn't bobble
	if ( trace->allsolid || 
		( trace->plane.normal[2] > 0 && 
		( le->pos.trDelta[2] < 40 || le->pos.trDelta[2] < -cg.frametime * le->pos.trDelta[2] ) ) ) {
		le->pos.trType = TR_STATIONARY;
	} else {

	}
}

/*
================
CG_AddFragment
================
*/
void CG_AddFragment( localEntity_t *le ) {
	vec3_t	newOrigin;
	trace_t	trace;

	if ( le->pos.trType == TR_STATIONARY ) {
		// sink into the ground if near the removal time
		int		t;
		float	oldZ;
		
		t = le->endTime - cg.time;
		if ( t < SINK_TIME ) {
			// we must use an explicit lighting origin, otherwise the
			// lighting would be lost as soon as the origin went
			// into the ground
			VectorCopy( le->refEntity.origin, le->refEntity.lightingOrigin );
			le->refEntity.renderfx |= RF_LIGHTING_ORIGIN;
			oldZ = le->refEntity.origin[2];
			le->refEntity.origin[2] -= 16 * ( 1.0 - (float)t / SINK_TIME );
			trap_R_AddRefEntityToScene( &le->refEntity );
			le->refEntity.origin[2] = oldZ;
		} else {
			trap_R_AddRefEntityToScene( &le->refEntity );
		}

		return;
	}

	// calculate new position
	BG_EvaluateTrajectory( NULL, &le->pos, cg.time, newOrigin );

	// trace a line from previous position to new position
	CG_Trace( &trace, le->refEntity.origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID );
	if ( trace.fraction == 1.0 ) {
		// still in free fall
		VectorCopy( newOrigin, le->refEntity.origin );

		if ( le->leFlags & LEF_TUMBLE ) {
			vec3_t angles;

			BG_EvaluateTrajectory( NULL, &le->angles, cg.time, angles );
			AnglesToAxis( angles, le->refEntity.axis );
		}

		trap_R_AddRefEntityToScene( &le->refEntity );

		// add a blood trail
		if ( le->leBounceSoundType == LEBS_BLOOD ) {
			CG_BloodTrail( le );
		}

		return;
	}

	// if it is in a nodrop zone, remove it
	// this keeps gibs from waiting at the bottom of pits of death
	// and floating levels
	if ( CG_PointContents( trace.endpos, 0 ) & CONTENTS_NODROP ) {
		CG_FreeLocalEntity( le );
		return;
	}

	// leave a mark
	CG_FragmentBounceMark( le, &trace );

	// do a bouncy sound
	CG_FragmentBounceSound( le, &trace );

	// reflect the velocity on the trace plane
	CG_ReflectVelocity( le, &trace );

	trap_R_AddRefEntityToScene( &le->refEntity );
}

/*
=====================================================================

TRIVIAL LOCAL ENTITIES

These only do simple scaling or modulation before passing to the renderer
=====================================================================
*/

/*
====================
CG_AddFadeNo
====================
*/
void CG_AddFadeNo( localEntity_t *le ) {
	refEntity_t* re = &le->refEntity;
	VectorScale(le->color,255,re->shaderRGBA);
	trap_R_AddRefEntityToScene(re);
	if(le->light){
		float light = (float)(cg.time - le->startTime) / (le->endTime - le->startTime);
		light = light < 0.5f ? 1.0f : 1.0f - (light - 0.5f) * 2.0f;
		light = le->light * light;
		trap_R_AddLightToScene(re->origin,light,le->lightColor);
	}
}

/*
====================
CG_AddFadeRGB
====================
*/
void CG_AddFadeRGB( localEntity_t *le ) {
	refEntity_t *re;
	float c;

	re = &le->refEntity;

	c = ( le->endTime - cg.time ) * le->lifeRate;
	c *= 0xff;

	re->shaderRGBA[0] = le->color[0] * c;
	re->shaderRGBA[1] = le->color[1] * c;
	re->shaderRGBA[2] = le->color[2] * c;

	trap_R_AddRefEntityToScene( re );
}

/*
====================
CG_AddFadeAlpha
====================
*/
void CG_AddFadeAlpha( localEntity_t *le ) {
	refEntity_t *re;
	float c;

	re = &le->refEntity;

	c = ( le->endTime - cg.time ) * le->lifeRate;
	c *= 0xff;

	re->shaderRGBA[3] = le->color[3] * c;

	trap_R_AddRefEntityToScene( re );
}

/*
==================
CG_AddMoveScaleFade
==================
*/
static void CG_AddMoveScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	//vec3_t		delta;
	//float		len;

	re = &le->refEntity;

	if ( le->fadeInTime > le->startTime && cg.time < le->fadeInTime ) {
		// fade / grow time
		c = 1.0 - (float) ( le->fadeInTime - cg.time ) / ( le->fadeInTime - le->startTime );
	}
	else {
		// fade / grow time
		c = ( le->endTime - cg.time ) * le->lifeRate;
	}

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	if ( !( le->leFlags & LEF_PUFF_DONT_SCALE ) ) {
		re->radius = le->radius * ( 1.0 - c ) + 8;
	}

	BG_EvaluateTrajectory( NULL, &le->pos, cg.time, re->origin );
/*
	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}
*/
	trap_R_AddRefEntityToScene( re );
}


/*
===================
CG_AddScaleFade

For rocket smokes that hang in place, fade out, and are
removed if the view passes through them.
There are often many of these, so it needs to be simple.
===================
*/
static void CG_AddScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	//vec3_t		delta;
	//float		len;

	re = &le->refEntity;

	// fade / grow time
	c = ( le->endTime - cg.time ) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];
	re->radius = le->radius * ( 1.0 - c ) + 8;
/*
	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}
*/
	trap_R_AddRefEntityToScene( re );
}

static void CG_AddScaleFadeRGB( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	//vec3_t		delta;
	//float		len;

	re = &le->refEntity;

	// fade / grow time
	c = ( le->endTime - cg.time ) * le->lifeRate;

	re->shaderRGBA[0] = 0xff * c * le->color[0];
	re->shaderRGBA[1] = 0xff * c * le->color[1];
	re->shaderRGBA[2] = 0xff * c * le->color[2];
	re->radius = le->radius * ( 1.0 - c ) + 8;
/*
	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}
*/
	trap_R_AddRefEntityToScene( re );
}

/*
=================
CG_AddFallScaleFade

This is just an optimized CG_AddMoveScaleFade
For blood mists that drift down, fade out, and are
removed if the view passes through them.
There are often 100+ of these, so it needs to be simple.
=================
*/
static void CG_AddFallScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	//vec3_t		delta;
	//float		len;

	re = &le->refEntity;

	// fade time
	c = ( le->endTime - cg.time ) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	re->origin[2] = le->pos.trBase[2] - ( 1.0 - c ) * le->pos.trDelta[2];

	re->radius = le->radius * ( 1.0 - c ) + 16;
/*
	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}
*/
	trap_R_AddRefEntityToScene( re );
}



/*
================
CG_AddExplosion
================
*/
static void CG_AddExplosion( localEntity_t *ex ) {
	refEntity_t* ent = &ex->refEntity;
	trap_R_AddRefEntityToScene(ent);
	if(ex->light){
		float light = (float)(cg.time - ex->startTime) / (ex->endTime - ex->startTime);
		light = light < 0.5f ? 1.0f : 1.0f - (light - 0.5f) * 2.0f;
		light = ex->light * light;
		trap_R_AddLightToScene(ent->origin,light,ex->lightColor);
	}
}


/*
========================
CG_AddStraightBeamFade
========================
*/
static void CG_AddStraightBeamFade( localEntity_t *le ) {
	refEntity_t	*ent;		// reference entity stored in the local entity
	float		scale_l;	// stores the scale factor for the beam's length
	float		scale_w;	// stores the scale factor for the beam's width
	vec3_t		start;		// temporary storage for the beam's start point
	vec3_t		direction;	// vector used in calculating the shortening of the beam
	vec4_t color;
	// set up for quick reference
	ent = &le->refEntity;

	// Save the start vector so it can be recovered after having been rendered.
	VectorCopy( ent->origin, start );

	scale_l = 1 - (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime );
	scale_w = 1 - (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime );
	if (scale_w < 0) {
		scale_w = 0;
	}
	VectorSet(color,scale_l,scale_l,scale_l);
	VectorScale(color,255,color);
	color[3] = scale_l * 255;
	// Set the scaled beam
	VectorSubtract( ent->origin, ent->oldorigin, direction);
	VectorScale(direction, scale_l, direction);
	VectorAdd(ent->oldorigin, direction, ent->origin);

	CG_DrawLineRGBA(ent->origin,ent->oldorigin,le->radius * scale_w,ent->customShader,color);

	// Restore the start vector
	VectorCopy(start, ent->origin);
}


/*
====================
CG_AddZEQExplosion
====================
*/
static void CG_AddZEQExplosion( localEntity_t *le ) {
	refEntity_t	*ent;
	float		c;
	float		RGBfade;
	float		phase;
	vec3_t		tmpAxes[3];
	
	ent = &le->refEntity;

	RGBfade = (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime );
	if ( RGBfade < 0.5 ) {
		RGBfade = 1.0;
	} else {
		RGBfade = 1.0 - ( RGBfade - 0.5 ) * 2;
	}

	ent->shaderRGBA[0] = 0xff * RGBfade;
	ent->shaderRGBA[1] = 0xff * RGBfade;
	ent->shaderRGBA[2] = 0xff * RGBfade;
	ent->shaderRGBA[3] = 0xff * RGBfade;
	
	// grow time
	phase = (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime ) * M_PI / 2.0f;
	c = 1.0f + (sin(phase));

	// preserve the full scale
	VectorCopy(ent->axis[0], tmpAxes[0]);
	VectorCopy(ent->axis[1], tmpAxes[1]);
	VectorCopy(ent->axis[2], tmpAxes[2]);

	// set the current scale
	VectorScale(ent->axis[0], 1 - c, ent->axis[0]);
	VectorScale(ent->axis[1], 1 - c, ent->axis[1]);
	VectorScale(ent->axis[2], 1 - c, ent->axis[2]);

	if(cg_drawBBox.value){
		vec3_t	mins,maxs;
		trap_R_ModelBounds( ent->hModel, mins, maxs, ent->frame );
		VectorScale(mins, ent->radius, mins);
		VectorScale(maxs, ent->radius, maxs);
		VectorScale(mins, 1 - c, mins);
		VectorScale(maxs, 1 - c, maxs);
		CG_DrawBoundingBox( ent->origin, mins, maxs );
	}

	// add the entity
	trap_R_AddRefEntityToScene( ent );

	// set the full scale again
	VectorCopy(tmpAxes[0], ent->axis[0]);
	VectorCopy(tmpAxes[1], ent->axis[1]);
	VectorCopy(tmpAxes[2], ent->axis[2]);

	if(le->light){
		float light = (float)(cg.time - le->startTime) / (le->endTime - le->startTime);
		float lightRad;
		vec3_t color;
		light = light < 0.5f ? light * 2.0f : 1.0f - (light - 0.5f) * 2.0f;
		lightRad = le->light * light;
		VectorScale(le->lightColor,light,color);
		trap_R_AddLightToScene(ent->origin,lightRad,color);
	}
}

/*
====================
CG_AddZEQSplash
====================
*/
static void CG_AddZEQSplash( localEntity_t *le ) {
	refEntity_t	*ent;
	float		c;
	//float		phase;
	float		RGBfade;
	vec3_t		tmpAxes[3];
	
	ent = &le->refEntity;

	RGBfade = (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime );
	if ( RGBfade < 0.5 ) {
		RGBfade = 1.0;
	} else {
		RGBfade = 1.0 - ( RGBfade - 0.5 ) * 2;
	}

	ent->shaderRGBA[3] = 0xff * RGBfade;
	
	// grow time
	c = ( le->endTime - cg.time ) * le->lifeRate / 1.1;

	// grow time
	//phase = (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime ) * M_PI / 1.5f;
	//c = 1.0f + (sin(phase));

	// preserve the full scale
	VectorCopy(ent->axis[0], tmpAxes[0]);
	VectorCopy(ent->axis[1], tmpAxes[1]);
	VectorCopy(ent->axis[2], tmpAxes[2]);

	// set the current scale
	VectorScale(ent->axis[0], 1 - c, ent->axis[0]);
	VectorScale(ent->axis[1], 1 - c, ent->axis[1]);
	VectorScale(ent->axis[2], 1 - c, ent->axis[2]);

	// add the entity
	trap_R_AddRefEntityToScene( ent );

	// set the full scale again
	VectorCopy(tmpAxes[0], ent->axis[0]);
	VectorCopy(tmpAxes[1], ent->axis[1]);
	VectorCopy(tmpAxes[2], ent->axis[2]);
}

/*
================
CG_AddSpriteExplosion
================
*/
static void CG_AddSpriteExplosion( localEntity_t *le ) {
	refEntity_t	re;
	float c;

	re = le->refEntity;

	c = ( le->endTime - cg.time ) / ( float ) ( le->endTime - le->startTime );
	if ( c > 1 ) {
		c = 1.0;	// can happen during connection problems
	}

	re.shaderRGBA[0] = 0xff;
	re.shaderRGBA[1] = 0xff;
	re.shaderRGBA[2] = 0xff;
	re.shaderRGBA[3] = 0xff * c * 0.33;

	re.reType = RT_SPRITE;
	re.radius = 42 * ( 1.0 - c ) + 30;

	trap_R_AddRefEntityToScene( &re );
	
	if(le->light){
		float light = (float)(cg.time - le->startTime) / (le->endTime - le->startTime);
		light = light < 0.5f ? 1.0f : 1.0f - (light - 0.5f) * 2.0f;
		light = le->light * light;
		trap_R_AddLightToScene(re.origin,light,le->lightColor);
	}
}

/*===================
CG_AddScorePlum
===================*/
#define NUMBER_SIZE		8

void CG_AddScorePlum( localEntity_t *le ) {
	refEntity_t	*re;
	vec3_t		origin, delta, dir, vec, up = {0, 0, 1};
	float		c, len;
	int			i, score, digits[10], numdigits, negative;

	re = &le->refEntity;

	c = ( le->endTime - cg.time ) * le->lifeRate;

	score = le->radius;
	if (score < 0) {
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0x11;
		re->shaderRGBA[2] = 0x11;
	}
	else {
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		if (score >= 50) {
			re->shaderRGBA[1] = 0;
		} else if (score >= 20) {
			re->shaderRGBA[0] = re->shaderRGBA[1] = 0;
		} else if (score >= 10) {
			re->shaderRGBA[2] = 0;
		} else if (score >= 2) {
			re->shaderRGBA[0] = re->shaderRGBA[2] = 0;
		}

	}
	if (c < 0.25)
		re->shaderRGBA[3] = 0xff * 4 * c;
	else
		re->shaderRGBA[3] = 0xff;

	re->radius = NUMBER_SIZE / 2;

	VectorCopy(le->pos.trBase, origin);
	origin[2] += 110 - c * 100;

	VectorSubtract(cg.refdef.vieworg, origin, dir);
	CrossProduct(dir, up, vec);
	VectorNormalize(vec);

	VectorMA(origin, -10 + 20 * sin(c * 2 * M_PI), vec, origin);

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < 20 ) {
		CG_FreeLocalEntity( le );
		return;
	}

	negative = qfalse;
	if (score < 0) {
		negative = qtrue;
		score = -score;
	}

	for (numdigits = 0; !(numdigits && !score); numdigits++) {
		digits[numdigits] = score % 10;
		score = score / 10;
	}

	if (negative) {
		digits[numdigits] = 10;
		numdigits++;
	}

	for (i = 0; i < numdigits; i++) {
		VectorMA(origin, (float) (((float) numdigits / 2) - i) * NUMBER_SIZE, vec, re->origin);
		re->customShader = cgs.media.numberShaders[digits[numdigits-1-i]];
		trap_R_AddRefEntityToScene( re );
	}
}




//==============================================================================

/*
===================
CG_AddLocalEntities

===================
*/
void CG_AddLocalEntities( void ) {
	localEntity_t	*le, *next;

	// walk the list backwards, so any new local entities generated
	// (trails, marks, etc) will be present this frame
	le = cg_activeLocalEntities.prev;
	for ( ; le != &cg_activeLocalEntities ; le = next ) {
		// grab next now, so if the local entity is freed we
		// still have it
		next = le->prev;

		if ( cg.time >= le->endTime ) {
			CG_FreeLocalEntity( le );
			continue;
		}
		switch ( le->leType ) {
		default:
			CG_Error( "Bad leType: %i", le->leType );
			break;
		case LE_MARK:
			break;
		case LE_SPRITE_EXPLOSION:
			CG_AddSpriteExplosion( le );
			break;
		case LE_EXPLOSION:
			CG_AddExplosion( le );
			break;
		case LE_ZEQEXPLOSION:
			if (cg.time >= le->startTime) {
				CG_AddZEQExplosion( le );
			}
			break;
		case LE_ZEQSMOKE:
			if (cg.time >= le->startTime) {
				CG_AddMoveScaleFade( le );
			}
			break;
		case LE_ZEQSPLASH:
			if (cg.time >= le->startTime) {
				CG_AddZEQSplash( le );
			}
			break;
		case LE_STRAIGHTBEAM_FADE:
			CG_AddStraightBeamFade( le );
			break;
		case LE_FRAGMENT:			// gibs and brass
			CG_AddFragment( le );
			break;
		case LE_MOVE_SCALE_FADE:		// water bubbles
			CG_AddMoveScaleFade( le );
			break;
		case LE_FADE_RGB:				// teleporters, railtrails
			CG_AddFadeRGB( le );
			break;
		case LE_FADE_ALPHA:				// teleporters, railtrails
			CG_AddFadeAlpha( le );
			break;
		case LE_FALL_SCALE_FADE: // gib blood trails
			CG_AddFallScaleFade( le );
			break;
		case LE_SCALE_FADE:		// rocket trails
			CG_AddScaleFade( le );
			break;
		case LE_SCALE_FADE_RGB:		// rocket trails
			CG_AddScaleFadeRGB( le );
			break;
		case LE_SCOREPLUM:
			CG_AddScorePlum( le );
			break;
		case LE_FADE_NO:				// teleporters, railtrails
			CG_AddFadeNo( le );
			break;
		}
	}
}




