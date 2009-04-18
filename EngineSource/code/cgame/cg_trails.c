#include "cg_local.h"

#define TRAIL_SEGMENTS		20
#define TRAIL_MIN_SPEED		300
#define TRAIL_MAX_LENGTH	( 2000 / TRAIL_SEGMENTS )

typedef struct {
	vec3_t		pos[TRAIL_SEGMENTS];
	vec3_t		tangent[TRAIL_SEGMENTS];
	vec3_t		color;
	qhandle_t	shader;
	float		width;
	float		baseSpeed;
} trail_t;

static trail_t cg_trails[MAX_GENTITIES];

void CG_InitTrails( void ) {
	memset( cg_trails, 0, sizeof(cg_trails) );
}

void CG_ResetTrail( int entityNum, vec3_t origin, float baseSpeed, float width, qhandle_t shader, vec3_t color ) {
	int i;
	
	for ( i = 0; i < TRAIL_SEGMENTS; i++ ) {
		VectorCopy( origin, cg_trails[entityNum].pos[i] );
	}
	
	cg_trails[entityNum].shader = shader;
	
	if ( baseSpeed > TRAIL_MIN_SPEED ) {
		cg_trails[entityNum].baseSpeed = baseSpeed;
	} else {
		cg_trails[entityNum].baseSpeed = TRAIL_MIN_SPEED;
	}

	cg_trails[entityNum].width = width;

	if ( color ) {
		VectorCopy( color, cg_trails[entityNum].color );
	} else {
		VectorSet( cg_trails[entityNum].color, 1.0f, 1.0f, 1.0f );
	}
}


void CG_UpdateTrailHead( int entityNum, vec3_t origin ) {
	VectorCopy( origin, cg_trails[entityNum].pos[0] );
	VectorSet( cg_trails[entityNum].tangent[0], 0, 0, 0 );
}


static void CG_LerpTrails( void ) {
	float	dist, distDelta;
	vec3_t	dir;
	int		i, j;
	trail_t	*trail;

	for ( j = 0; j < MAX_GENTITIES; j++ ) {

		trail = &cg_trails[j];
		// Don't bother updating if the very end and very start are already
		// the same. We'd either be on the start frame or the last frame of
		// the trails existence. It's not going to be drawn either way.
		
		// FIXED: Just incase we'd be able to get a full loop back of the first
		//        to the last point in with guided missiles, ALWAYS check those.
		if ( !Distance(trail->pos[0], trail->pos[TRAIL_SEGMENTS - 1] ) &&
			 !( cg_entities[j].currentState.eFlags & EF_GUIDED ) ) {
			continue;
		}

		for ( i = 1; i < TRAIL_SEGMENTS; i++ ) {
			// Set up the distance and direction
			VectorSubtract( trail->pos[i-1], trail->pos[i], dir );
			dist = VectorNormalize( dir );

			distDelta = trail->baseSpeed * ((28 - i) / 40.0f) * cg.frametime * 0.001f;

			// If we'd overshoot, halt on the previous point
			if ( distDelta > dist ) {
				VectorCopy( trail->pos[i-1], trail->pos[i] );
				VectorCopy( trail->tangent[i-1], trail->tangent[i] );
				continue;
			}

			
			// If the trail section gets too long, snap it forward like a rubberband.
			if ( dist - distDelta > TRAIL_MAX_LENGTH ) {
				VectorMA( trail->pos[i-1], -TRAIL_MAX_LENGTH, dir, trail->pos[i] );
				VectorCopy( dir, trail->tangent[i] );
				continue;
			}

			VectorMA( trail->pos[i], distDelta, dir, trail->pos[i] );
			VectorCopy( dir, trail->tangent[i] );
		}

	}

}



static void CG_ShiftTrailVerts( polyVert_t *verts ) {
	int i;

	VectorCopy( verts[0].xyz, verts[3].xyz );
	VectorCopy( verts[1].xyz, verts[2].xyz );
	verts[3].st[0] = verts[0].st[0];
	verts[3].st[1] = verts[0].st[1];
	verts[2].st[0] = verts[1].st[0];
	verts[2].st[1] = verts[1].st[1];

	// NOTE: Alpha doesn't change, so only copy RGB
	for ( i = 0; i < 3; i++ ) {
		verts[3].modulate[i] = verts[0].modulate[i];
		verts[2].modulate[i] = verts[1].modulate[i];
	}
}


static void CG_GetTrailVerts (vec3_t point, vec3_t tangent, float width, polyVert_t *verts) {
	vec3_t offset, viewLine, dummyPoint;
	float len;
	
	VectorSubtract (point, cg.refdef.vieworg, viewLine);
	CrossProduct (viewLine, tangent, offset);
	len = VectorNormalize (offset);
	
	// HACK: if len would be 0, then we wouldn't get a proper width, so
	//       in this case nudge the point upwards by 0.1 on its z-coordinate.
	if ( !len ) {
		VectorCopy( point, dummyPoint );
		dummyPoint[2] += 0.1f;
		VectorSubtract (dummyPoint, cg.refdef.vieworg, viewLine);
		CrossProduct (viewLine, tangent, offset);
		VectorNormalize (offset);
	}

	VectorMA (point, width, offset, verts[0].xyz);
	VectorMA (point, -width, offset, verts[1].xyz);
}


void CG_AddTrailsToScene ( void ) {
	int			i, j, k;
	trail_t		*trail;
	polyVert_t	verts[4];
	vec3_t		blendTangent;

	CG_LerpTrails();

	// Initialize the polyVerts.
	memset( verts, 0, sizeof(verts) );

	/*
	verts[0].st[0] = 1;
	verts[0].st[1] = 1;
	verts[1].st[0] = 1;
	verts[1].st[1] = 0;
	verts[2].st[0] = 0;
	verts[2].st[1] = 0;
	verts[3].st[0] = 0;
	verts[3].st[1] = 1;
	
	verts[0].st[1] = 0.0f;
	verts[1].st[1] = 1.0f;
	verts[2].st[1] = 1.0f;
	verts[3].st[1] = 0.0f;
	*/

	verts[0].st[1] = 1.0f;
	verts[1].st[1] = 0.0f;
	verts[2].st[1] = 0.0f;
	verts[3].st[1] = 1.0f;

	for ( i = 0; i < 4; i++ ) {
		verts[i].modulate[3] = 255;
	}

	for ( j = 0; j < MAX_GENTITIES; j++ ) {
		
		trail = &cg_trails[j];

		if ( !trail->shader ) {
			continue;
		}

		// Don't bother drawing if the very end and very start are already
		// the same. We'd have an invisible trail anyway
		
		// FIXED: Just incase we'd be able to get a full loop back of the first
		//        to the last point in with guided missiles, ALWAYS check those.
		if ( !Distance(trail->pos[0], trail->pos[TRAIL_SEGMENTS - 1] ) &&
			 !( cg_entities[j].currentState.eFlags & EF_GUIDED ) ) {
			continue;
		}

		// color the vertices correctly
		for ( i = 0; i < 4; i++ ) {
			for ( k = 0; k < 3; k++ ) {
				verts[i].modulate[k] = trail->color[k] * 255;
			}
		}

		i = TRAIL_SEGMENTS - 1;
		VectorCopy( trail->tangent[i], blendTangent );
		VectorNormalize( blendTangent );
		CG_GetTrailVerts( trail->pos[i], blendTangent, trail->width, verts );
		verts[0].st[0] = verts[1].st[0] = 0.0f;
		CG_ShiftTrailVerts( verts );

		for ( i = TRAIL_SEGMENTS - 2; i >= 0; i-- ) {

			// Don't draw this trail node if it overlaps with the previous one.
			if (! Distance( trail->pos[i+1], trail->pos[i] ) ) {
				continue;
			}

			// Properly blend the tangents for a smoother match
			if ( i == 0 || !VectorLength( trail->tangent[i] ) ) {
				VectorCopy( trail->tangent[i + 1], blendTangent );
			} else {
				VectorAdd( trail->tangent[i], trail->tangent[i+1], blendTangent );				
			}
			VectorNormalize( blendTangent );

			CG_GetTrailVerts( trail->pos[i], blendTangent, trail->width, verts );
			verts[0].st[0] = verts[1].st[0] = 1.0f - (float)i / (TRAIL_SEGMENTS - 1);

			trap_R_AddPolyToScene( trail->shader, 4, verts );
			CG_ShiftTrailVerts( verts );
		}
	}
}

