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

#include "vis.h"

int		c_chains;
int		c_portalskip, c_leafskip;
int		c_vistest, c_mighttest;

int		active;

void CheckStack (leaf_t *leaf, threaddata_t *thread)
{
	pstack_t	*p;

	for (p=thread->pstack_head.next ; p ; p=p->next)
		if (p->leaf == leaf)
			Error ("CheckStack: leaf recursion");
}


/*
==============
ClipToSeperators

Source, pass, and target are an ordering of portals.

Generates seperating planes canidates by taking two points from source and one
point from pass, and clips target by them.

If target is totally clipped away, that portal can not be seen through.

Normal clip keeps target on the same side as pass, which is correct if the
order goes source, pass, target.  If the order goes pass, source, target then
flipclip should be set.
==============
*/
winding_t	*ClipToSeperators (winding_t *source, winding_t *pass, winding_t *target, qboolean flipclip)
{
	int			i, j, k, l;
	plane_t		plane;
	vec3_t		v1, v2;
	float		d;
	vec_t		length;
	int			counts[3];
	qboolean		fliptest;

// check all combinations	
	for (i=0 ; i<source->numpoints ; i++)
	{
        
        
		l = (i+1)%source->numpoints;
		VectorSubtract (source->points[l] , source->points[i], v1); //c = a - b

        
        
	// find a vertex of pass that makes a plane that puts all of the
	// vertexes of pass on the front side and all of the vertexes of
	// source on the back side
		for (j=0 ; j<pass->numpoints ; j++)
		{
            
            
            
			VectorSubtract (pass->points[j], source->points[i], v2);
            
            //Cross-product for normal
			plane.normal[0] = v1[1]*v2[2] - v1[2]*v2[1];
			plane.normal[1] = v1[2]*v2[0] - v1[0]*v2[2];
			plane.normal[2] = v1[0]*v2[1] - v1[1]*v2[0];
			
        
            
            
        // if points don't make a valid plane, skip it

			length = plane.normal[0] * plane.normal[0] + plane.normal[1] * plane.normal[1] + plane.normal[2] * plane.normal[2];
			
			if (length < ON_EPSILON)
				continue;

            
            
            //Unit Normalize the normal vector
			length = 1/sqrt(length);
			
			plane.normal[0] *= length;
			plane.normal[1] *= length;
			plane.normal[2] *= length;

            
			plane.dist = DotProduct (pass->points[j], plane.normal);

            
            //At this point we have a valide normalized plan equation
            
            
            
		//
		// find out which side of the generated seperating plane has the
		// source portal
		//
			fliptest = false;
			for (k=0 ; k<source->numpoints ; k++)
			{
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
				{	// source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false;
					break;
				}
				else if (d > ON_EPSILON)
				{	// source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if (k == source->numpoints)
				continue;		// planar with source portal

            
            
            
            
		//
		// flip the normal if the source portal is backwards
		//
			if (fliptest)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
			
            
            
		//
		// if all of the pass portal points are now on the positive side,
		// this is the seperating plane
		//
			counts[0] = counts[1] = counts[2] = 0;
			for (k=0 ; k<pass->numpoints ; k++)
			{
				if (k==j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	// points on negative side, not a seperating plane
				
			if (!counts[0])
			{
				continue;	// planar with seperating plane
			}
		
            
            
		//
		// flip the normal if we want the back side
		//
			if (flipclip)
			{
				VectorSubtract (vec3_origin, plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}
			
            
            
		//
		// clip target by the seperating plane
		//
			target = ClipWinding (target, &plane, false);
			if (!target)
				return NULL;		// target is not visible
		}
	}
	
	return target;
}



/*
==================
RecursiveLeafFlow

Flood fill through the leafs
If src_portal is NULL, this is the originating leaf
==================
*/
void RecursiveLeafFlow (int leafnum, threaddata_t *thread, pstack_t *prevstack)
{
	pstack_t	stack;
	portal_t	*p;
	plane_t		backplane;
	winding_t	*source, *target;
	leaf_t 		*leaf;
	int			i, j;
	long		*test, *might, *vis;
	qboolean		more;
	
	c_chains++;

	leaf = &leafs[leafnum];
	CheckStack (leaf, thread);
	
// mark the leaf as visible
	if (! (thread->leafvis[leafnum>>3] & (1<<(leafnum&7)) ) )
	{
		thread->leafvis[leafnum>>3] |= 1<<(leafnum&7);
		thread->base->numcansee++;
	}
	
	prevstack->next = &stack;
	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;
	stack.mightsee = malloc(bitbytes);
	might = (long *)stack.mightsee;
	vis = (long *)thread->leafvis;
	
// check all portals for flowing into other leafs	
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];

		if ( ! (prevstack->mightsee[p->leaf>>3] & (1<<(p->leaf&7)) ) )
		{
			c_leafskip++;
			continue;	// can't possibly see it
		}

	// if the portal can't see anything we haven't allready seen, skip it
		if (p->status == stat_done)
		{
			c_vistest++;
			test = (long *)p->visbits;
		}
		else
		{
			c_mighttest++;
			test = (long *)p->mightsee;
		}
		more = false;
		for (j=0 ; j<bitlongs ; j++)
		{
			might[j] = ((long *)prevstack->mightsee)[j] & test[j];
			if (might[j] & ~vis[j])
				more = true;
		}
		
		if (!more)
		{	// can't see anything new
			c_portalskip++;
			continue;
		}
		
// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		VectorSubtract (vec3_origin, p->plane.normal, backplane.normal);
		backplane.dist = -p->plane.dist;
			
		if (VectorCompare (prevstack->portalplane.normal, backplane.normal) )
			continue;	// can't go out a coplanar face
	
		c_portalcheck++;
		
		stack.portal = p;
		stack.next = NULL;
		
		target = ClipWinding (p->winding, &thread->pstack_head.portalplane, false);
		if (!target)
			continue;
			
		if (!prevstack->pass)
		{	// the second leaf can only be blocked if coplanar
		
			stack.source = prevstack->source;
			stack.pass = target;
			RecursiveLeafFlow (p->leaf, thread, &stack);
			FreeWinding (target);
			continue;
		}

		target = ClipWinding (target, &prevstack->portalplane, false);
		if (!target)
			continue;
		
		source = CopyWinding (prevstack->source);

		source = ClipWinding (source, &backplane, false);
		if (!source)
		{
			FreeWinding (target);
			continue;
		}

		c_portaltest++;

		if (testlevel > 0)
		{
			target = ClipToSeperators (source, prevstack->pass, target, false);
			if (!target)
			{
				FreeWinding (source);
				continue;
			}
		}
		
		if (testlevel > 1)
		{
			target = ClipToSeperators (prevstack->pass, source, target, true);
			if (!target)
			{
				FreeWinding (source);
				continue;
			}
		}
		
		if (testlevel > 2)
		{
			source = ClipToSeperators (target, prevstack->pass, source, false);
			if (!source)
			{
				FreeWinding (target);
				continue;
			}
		}
		
		if (testlevel > 3)
		{
			source = ClipToSeperators (prevstack->pass, target, source, true);
			if (!source)
			{
				FreeWinding (target);
				continue;
			}
		}

		stack.source = source;
		stack.pass = target;

		c_portalpass++;
	
	// flow through it for real
		RecursiveLeafFlow (p->leaf, thread, &stack);
		
		FreeWinding (source);
		FreeWinding (target);
	}
	
	free (stack.mightsee);
}


/*
===============
PortalFlow

===============
*/
void PortalFlow (portal_t *p)
{
	threaddata_t	data;

	if (p->status != stat_working)
		Error ("PortalFlow: reflowed");
	p->status = stat_working;
	
	p->visbits = malloc (bitbytes);
	memset (p->visbits, 0, bitbytes);

	memset (&data, 0, sizeof(data));
	data.leafvis = p->visbits;
	data.base = p;
	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;
	data.pstack_head.mightsee = p->mightsee;
		
	RecursiveLeafFlow (p->leaf, &data, &data.pstack_head);

	p->status = stat_done;
}


/*
===============================================================================

This is a rough first-order approximation that is used to trivially reject some
of the final calculations.

===============================================================================
*/

byte	portalsee[MAX_PORTALS];
int		c_leafsee, c_portalsee;

void SimpleFlood (portal_t *srcportal, int leafnum)
{
	int		i;
	leaf_t	*leaf;
	portal_t	*p;
	
    //We have already been there.
	if (srcportal->mightsee[leafnum>>3] & (1<<(leafnum&7)) )
		return;
    
	srcportal->mightsee[leafnum>>3] |= (1<<(leafnum&7));
	c_leafsee++;
	
	leaf = &leafs[leafnum];
	
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];
		if ( !portalsee[ p - portals ] )
			continue;
		SimpleFlood (srcportal, p->leaf);
	}
}


/*
==============
BasePortalVis
==============
*/
/*
  FCS: 1. In order for a portal A to see a portal B, B at the very minimum has to have one of its winding part in front of A's plan.
          But we also need to make sure that B will take a view from A somewhere so we check if A is at least partially behind B.
 
       2. Once portalsee is populated for a portal, recursively flood so in the end we have a graph of part portal can see which.
 */
void BasePortalVis (void)
{
	int			i, j, k;
	portal_t	*tp, *p;
	float		d;
	winding_t	*w;
	
    //We iterate numportals*2 because we create two portals for each porta loaded from the prt file.
	for (i=0, p = portals ; i<numportals*2 ; i++, p++)
	{
        //mightsee refers to leafs the portal migh be able to see.
		p->mightsee = malloc (bitbytes);
		memset (p->mightsee, 0, bitbytes);
		
        //Reset all portalsee (array and counter).
		c_portalsee = 0;
        //portalsee refers to other portals the portal can see.
		memset (portalsee, 0, numportals*2);

		for (j=0, tp = portals ; j<numportals*2 ; j++, tp++)
		{
            
            //No check visibility of a portal with itself.
			if (j == i)
				continue;
            
            
            // Inject all points from portal j into plane equation of portal i.
			w = tp->winding;
			for (k=0 ; k<w->numpoints ; k++)
			{
				d = DotProduct (w->points[k], p->plane.normal) - p->plane.dist;
				if (d > ON_EPSILON)
					break;
			}
            // Portal j doesn't have at least one point in front of portal i, it cannot be visible.
			if (k == w->numpoints)
				continue;	// no points on front
				
			
            // It is not enough to have J in from on I. We also need to check that portal J is not looking toward I.
            // We hence need to check that I is at least partially BEHIND J.
			w = p->winding;
			for (k=0 ; k<w->numpoints ; k++)
			{
				d = DotProduct (w->points[k], tp->plane.normal) - tp->plane.dist;
				if (d < -ON_EPSILON)
					break;
			}
			if (k == w->numpoints)
				continue;	// no points on front
            

            // 1. J is in front of I
            // 2. J will actually take I somewhere (it is not looking toward J).
            //    J is seen by I.
			portalsee[j] = 1;		
			c_portalsee++;
			
		}
		
		c_leafsee = 0;
		SimpleFlood (p, p->leaf);
		p->nummightsee = c_leafsee;
//		printf ("portal:%4i  c_leafsee:%4i \n", i, c_leafsee);
	
	}

}














