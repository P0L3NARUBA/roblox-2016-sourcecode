/* Copyright 2003-2005 ROBLOX Corporation, All Rights Reserved */
#include "stdafx.h"

#include "V8World/PolyCellContact.h"
#include "V8Kernel/PolyConnectors.h"
#include "V8World/Poly.h"
#include "V8World/Mesh.h"
#include "V8World/Primitive.h"
#include "V8DataModel/MegaCluster.h"

namespace RBX {

using namespace POLY;
using namespace Voxel;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float PolyCellContact::epsilonDistance() 
{
	return ContactConnector::overlapGoal();
}


PolyCellContact::PolyCellContact(Primitive* p0, Primitive* p1, const Vector3int16& cell)
	: CellMeshContact(p0, p1, Vector3int32(cell))
	, bestPair(NULL)
{
    cellMesh = new POLY::Mesh;

    Vector3 size(kCELL_SIZE, kCELL_SIZE, kCELL_SIZE);
    Vector3 cellOffset = kCELL_SIZE * Vector3(cell) + Vector3(Voxel::kHALF_CELL, Voxel::kHALF_CELL, Voxel::kHALF_CELL);

    Grid* grid = static_cast<MegaClusterInstance*>(p0->getOwner())->getVoxelGrid();
    Cell cellData = grid->getCell(cell);

    CellOrientation orientation = cellData.solid.getOrientation();
    CellBlock type = cellData.solid.getBlock();	

    switch( type )
    {
        case CELL_BLOCK_Solid:
        default:
            cellMesh->makeCell(size, cellOffset);
            break;
        case CELL_BLOCK_VerticalWedge:
            cellMesh->makeVerticalWedgeCell(size, cellOffset, (int)orientation);
            break;
        case CELL_BLOCK_HorizontalWedge:
            cellMesh->makeHorizontalWedgeCell(size, cellOffset, (int)orientation);
            break;
        case CELL_BLOCK_CornerWedge:
            cellMesh->makeCornerWedgeCell(size, cellOffset, (int)orientation);
            break;
        case CELL_BLOCK_InverseCornerWedge:
            cellMesh->makeInverseCornerWedgeCell(size, cellOffset, (int)orientation);
            break;
    }
}

PolyCellContact::~PolyCellContact()
{
	resetBestPair(NULL);
    delete cellMesh;
}

void PolyCellContact::findClosestFeatures(ConnectorArray& newConnectors)
{
	if (!dynamic_cast<const Poly*>(getPrimitive(0)->getConstGeometry())) return;
	if (!dynamic_cast<const Poly*>(getPrimitive(1)->getConstGeometry())) return;

	findBestPair();

    if(bestPair)
	    bestPair->loadConnectors(newConnectors);
}


// find feature with GREATEST distance - i.e. least overlap or if > 0, no overlap

void PolyCellContact::findBestPair()
{
    //RBXASSERT(contactParams);
    if(!contactParams) {
        generateDataForMovingAssemblyStage();
    }

    if (!bestPair) {
	    bestPair = new CellFaceFacePair(getPrimitive(0), getPrimitive(1), *contactParams, this, false);
    }

    float currentDistance = bestPair->test();
    if (currentDistance > 0.0) {
	    return;
    }

    CellFaceFacePair face0(getPrimitive(0), getPrimitive(1), *contactParams, this, false);
    CellFaceFacePair face1(getPrimitive(1), getPrimitive(0), *contactParams, this, true);
    CellEdgeEdgePair edgeEdge(getPrimitive(0), getPrimitive(1), *contactParams, this, false);

    PolyCellPair* testPairs[] = {&face0, &face1, &edgeEdge};
    PolyCellPair* betterPair = NULL;

    for (size_t i = 0; i < 2; ++i) {				// only doing face face - Tim - if you set this to 3 we will get CellEdgeEdgePairs.
	    PolyCellPair* testPair = testPairs[i];
	    if (!bestPair->match(testPair)) {
		    float distance = testPair->test();
		    if (distance > 0.0) {					// separating plane or separating edges
			    resetBestPair(testPair);
			    return;
		    }
		    if (distance > (currentDistance + epsilonDistance())) {		// threshold to switch
			    betterPair = testPair;
			    currentDistance = distance;
		    }
	    }
    }
	
    if (betterPair) {
	    resetBestPair(betterPair);
    }
}

void PolyCellContact::generateDataForMovingAssemblyStage(void)
{
    Contact::generateDataForMovingAssemblyStage();
}

void PolyCellContact::resetBestPair(PolyCellPair* pairOnStack)
{
	if (bestPair) {
		delete bestPair;
		bestPair = NULL;
	}

	if (pairOnStack) {
		bestPair = pairOnStack->allocateClone();
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Poly* PolyCellPair::poly0() const
{
	return rbx_static_cast<const Poly*>(primitive[0]->getConstGeometry());
}

const Poly* PolyCellPair::poly1() const
{
	return rbx_static_cast<const Poly*>(primitive[1]->getConstGeometry());
}





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CellFaceFacePair::CellFaceFacePair(Primitive* p0, Primitive* p1, const ContactParams& contactParams, PolyCellContact* aPCContact, bool swap) 
	: PolyCellPair(p0, p1, contactParams, aPCContact, swap)
	, otherFace(NULL)
{
    mainFace = swapPrims ? poly0()->getMesh()->getFace(0) : aPCContact->getCellMesh()->getFace(0);
}



PolyCellPair* CellFaceFacePair::allocateClone()
{
	return new CellFaceFacePair(*this);
}


// Find best face - i.e. face with the greatest distance (least penetration)
float CellFaceFacePair::test()
{
	CoordinateFrame otherInMe = primitive[0]->getCoordinateFrame().toObjectSpace(primitive[1]->getCoordinateFrame());

	// do this only once
	//FixedArray<Vector3, 8> verticesInObject;
	FixedArray<Vector3, CONTACT_ARRAY_SIZE> verticesInObject;
	computeVertices(verticesInObject, otherInMe);

	// do current first - could be > 0.0
	const Vertex* closeVertex = NULL;
	float biggestDistance = closestVertex(mainFace, verticesInObject, closeVertex);
	if (biggestDistance > 0.0) {
		otherFace = NULL;
		return biggestDistance;			// separating plane - blow out
	}

    const Mesh* faceMesh = swapPrims ? poly0()->getMesh() : myPCContact->getCellMesh();
	for (size_t i = 0; i < faceMesh->numFaces(); ++i) {
		const POLY::Face* face = faceMesh->getFace(i);
		if (face != mainFace) {
			const Vertex* tempVertex = NULL;
			float distance = closestVertex(face, verticesInObject, tempVertex);	
			if (distance > biggestDistance) {
				biggestDistance = distance;
				mainFace = face;
				closeVertex = tempVertex;
				if (distance > 0.0) {			// separating plane - blow out
					otherFace = NULL;				// make sure we are not using this - no contact
					return biggestDistance;
				}
			}
		}
	}

	otherFace = findOtherFace(closeVertex);

	return biggestDistance;
}

//void CellFaceFacePair::computeVertices(FixedArray<Vector3, 8>& verticesInObject, const CoordinateFrame& otherInMe)
void CellFaceFacePair::computeVertices(FixedArray<Vector3, CONTACT_ARRAY_SIZE>& verticesInObject, const CoordinateFrame& otherInMe)
{
	RBXASSERT(verticesInObject.size() == 0);
	const Mesh* mesh = swapPrims ? myPCContact->getCellMesh() : poly1()->getMesh();

	for (size_t i = 0; i < mesh->numVertices(); ++i) {
		const Vertex* vertex = mesh->getVertex(i);
		Vector3 vertexInFace = otherInMe.pointToWorldSpace(vertex->getOffset());
		verticesInObject.push_back(vertexInFace);
	}
}


// Find worst point on this face - i.e. lowest distance / most penetration
//float CellFaceFacePair::closestVertex(const POLY::Face* face, const FixedArray<Vector3, 8>& verticesInObject, const Vertex* &closestVertex)
float CellFaceFacePair::closestVertex(const POLY::Face* face, const FixedArray<Vector3, CONTACT_ARRAY_SIZE>& verticesInObject, const Vertex* &closestVertex)
{
	float smallestDistance = FLT_MAX;
	const Plane& plane = face->plane();
	for (size_t i = 0; i < verticesInObject.size(); ++i) {
		float distance = plane.distance(verticesInObject[i]);		// < 0 = penetration
		if (distance < smallestDistance) {
			smallestDistance = distance;
            
            const Mesh* mesh = swapPrims ? myPCContact->getCellMesh() : poly1()->getMesh();

            closestVertex = mesh->getVertex(i);
		}
	}
	return smallestDistance;
}


const POLY::Face* CellFaceFacePair::findOtherFace(const Vertex* closeVertex)
{
	const POLY::Face* bestFace = NULL;
	float bestAlignment = -2.0;	

	CoordinateFrame faceInOther = primitive[1]->getCoordinateFrame().toObjectSpace(primitive[0]->getCoordinateFrame());

	Plane planeInOther = faceInOther.toWorldSpace(mainFace->plane());
	for (size_t i = 0; i < closeVertex->numFaces(); ++i) {
		const POLY::Face* testFace = closeVertex->getFace(i);
		const Plane& testPlane = testFace->plane();
		float alignment = planeInOther.normal().dot(-testPlane.normal());
		if (alignment > bestAlignment) {
			bestAlignment = alignment;
			bestFace = testFace;
		}
	}
	return bestFace;
}



/*

	1) 	sFrom inside?	Do Vert Inside;

	2) 

					sTo
					ABOVE			BELOW				INSIDE
	sFrom
	ABOVE			----			maybe				maybe
									edge/sideFace		edge/sideFace

	BELOW			maybe			maybe up to 2		edgeFace
					edge/sideFace	edge/2 sideFace		for sure

	INSIDE			maybe			edgeFace			---
					edge/sideFace	for Sure
*/

bool CellFaceFacePair::pairIsValid()
{
	// The initial version of this pair validation only eliminates contact connectors that have been
	// created for a -y facing terrain cell face that is interior (that is, not exposed for actual contact).
	MegaClusterInstance* terrain0 = NULL;
	MegaClusterInstance* terrain1 = NULL;

	primitive[0]->getGeometryType() == Geometry::GEOMETRY_MEGACLUSTER ? 
		terrain0 = rbx_static_cast<MegaClusterInstance*>(primitive[0]->getOwner()) :
		terrain1 = rbx_static_cast<MegaClusterInstance*>(primitive[1]->getOwner());

	RBXASSERT(primitive[0]->getGeometryType() == Geometry::GEOMETRY_MEGACLUSTER || primitive[1]->getGeometryType() == Geometry::GEOMETRY_MEGACLUSTER);

	const POLY::Face* theFace = terrain0 ? mainFace : otherFace;
    Vector3int16 cellLoc = myPCContact->getGridFeature().toVector3int16();

    if (theFace->normal().fuzzyEq(-Vector3::unitY()))
    {
        if (myPCContact->cellFaceIsInterior(cellLoc, Voxel::MinusY))
            return false;
    }

	return true;
}

void CellFaceFacePair::loadConnectors(ConnectorArray& newConnectors)
{
	if (!otherFace) {			
		return;
	}

	if (!pairIsValid())
		return;

	//FixedArray<VertexStatus, 8> vertexStatus[2];		// check all vertices - both other in main face, and main face in other.  
	FixedArray<VertexStatus, CONTACT_ARRAY_SIZE> vertexStatus[2];		// check all vertices - both other in main face, and main face in other.  
	CoordinateFrame vertexInFace[2];

	bool allVerticesIn = loadVertices(vertexStatus, vertexInFace, newConnectors);
	if (allVerticesIn) {
		return;
	}

	const CoordinateFrame& otherInMe = vertexInFace[0];

	size_t numEdges = otherFace->numEdges();
	for (size_t i = 0; i < numEdges; ++i) {
		size_t i1 = (i + 1) % numEdges;
		const Vertex* vFrom = otherFace->getVertex(i);
		const Vertex* vTo = otherFace->getVertex(i1);
		VertexStatus sFrom = vertexStatus[0][i];
		VertexStatus sTo = vertexStatus[0][i1];

		switch (sFrom)
		{
		case ABOVE_INSIDE:
			{
				switch (sTo)
				{
				case ABOVE_INSIDE:		break;
				case ABOVE_OUTSIDE:		break;
				case BELOW_INSIDE:		break;
				case BELOW_OUTSIDE:		checkOneSideIntersection(vFrom, vTo, otherInMe, newConnectors);		break;
				}
				break;
			}
		case ABOVE_OUTSIDE:
			{
				switch (sTo)
				{
				case ABOVE_INSIDE:		break;
				case ABOVE_OUTSIDE:		break;
				case BELOW_INSIDE:		checkOneSideIntersection(vFrom, vTo, otherInMe, newConnectors);		break;
				case BELOW_OUTSIDE:		checkTwoSideIntersections(vFrom, vTo, otherInMe, newConnectors);	break;
				}
				break;
			}
		case BELOW_INSIDE:
			{
				switch (sTo)
				{
				case ABOVE_INSIDE:		break;
				case ABOVE_OUTSIDE:		checkOneSideIntersection(vFrom, vTo, otherInMe, newConnectors);		break;
				case BELOW_INSIDE:		break;
				case BELOW_OUTSIDE:		validateOneSideIntersection(vFrom, vTo, otherInMe, newConnectors);	break;
				}
				break;
			}
		case BELOW_OUTSIDE:
			{
				switch (sTo)
				{
				case ABOVE_INSIDE:		checkOneSideIntersection(vFrom, vTo, otherInMe, newConnectors);		break;
				case ABOVE_OUTSIDE:		checkTwoSideIntersections(vFrom, vTo, otherInMe, newConnectors);	break;
				case BELOW_INSIDE:		validateOneSideIntersection(vTo, vFrom, otherInMe, newConnectors);	break;
				case BELOW_OUTSIDE:		checkTwoSideIntersections(vFrom, vTo, otherInMe, newConnectors);	break;
				}
				break;
			}
		}
	}
}


//bool CellFaceFacePair::loadVertices(FixedArray<VertexStatus, 8>* vertexStatus, 
bool CellFaceFacePair::loadVertices(FixedArray<VertexStatus, CONTACT_ARRAY_SIZE>* vertexStatus, 
								CoordinateFrame* vertexInFace,
								ConnectorArray& newConnectors	)		// check all vertices - both other in main face, and main face in other.  
{
    // FIX for MC
	for (size_t i = 0; i < 2; ++i) {
		vertexInFace[i] = primitive[i]->getCoordinateFrame().toObjectSpace(primitive[(i+1) % 2]->getCoordinateFrame());
		bool allVerticesIn = testVerticesInside(i, vertexStatus[i], vertexInFace[i], newConnectors);
		if (allVerticesIn) {
			return true;
		}
	}
	return false;
}

// TODO - Turn on optimize after fixed
#pragma optimize( "", off )

		//bool CellFaceFacePair::testVerticesInside(size_t faceId, FixedArray<VertexStatus, 8>& vertexStatus, 
		bool CellFaceFacePair::testVerticesInside(size_t faceId, FixedArray<VertexStatus, CONTACT_ARRAY_SIZE>& vertexStatus, 
											  const CoordinateFrame& vertexInFace,
											  ConnectorArray& newConnectors)
		{
			bool allInside = true;
			size_t vertex_id = (faceId + 1) % 2;
			const POLY::Face* planeFace = face(faceId);
			const POLY::Face* vertexFace = face(vertex_id);
            const POLY::Mesh* planeMesh = poly(faceId)->getMesh();

            if(faceId == 0)
                swapPrims ? poly0()->getMesh() : myPCContact->getCellMesh();
            else
                swapPrims ? myPCContact->getCellMesh() : poly1()->getMesh();

			for (size_t i = 0; i < vertexFace->numVertices(); ++i) {
				const Vertex* v = vertexFace->getVertex(i);
				VertexStatus vs = vertexInPoly(planeFace, planeMesh, v, vertexInFace);
				vertexStatus.push_back(vs);
				if (vs == BELOW_INSIDE) {
					vertexInside(primitive[faceId], primitive[vertex_id], v, planeFace, newConnectors);
				}
				else {
					allInside = false;
				}
			}
			return allInside;
		}

		CellFaceFacePair::VertexStatus CellFaceFacePair::vertexInPoly(const POLY::Face* planeFace, 
															  const Mesh* planeMesh, 
															  const Vertex* vertex, 
															  const CoordinateFrame& otherInMe)
		{
			Vector3 vertexInFacePoly = otherInMe.pointToWorldSpace(vertex->getOffset());
			bool below = planeFace->plane().pointOnOrBehind(vertexInFacePoly);
			bool inFace = planeFace->pointInExtrusion(vertexInFacePoly);

			if (below) {
				if (inFace) {return BELOW_INSIDE;}
				else		{return BELOW_OUTSIDE;}
			}
			else {
				if (inFace) {return ABOVE_INSIDE;}
				else		{return ABOVE_OUTSIDE;}
			}
		}

#pragma optimize( "", off )

// TODO - turn optimizer back on here after fixed
void CellFaceFacePair::checkTwoSideIntersections(const Vertex* v0, const Vertex* v1, const CoordinateFrame& otherInMe, ConnectorArray& newConnectors)
{
	Vector3 p0 = otherInMe.pointToWorldSpace(v0->getOffset());
	Vector3 p1 = otherInMe.pointToWorldSpace(v1->getOffset());

	int side0 = -1;
	int side1 = -1;
	mainFace->findInternalExtrusionIntersections(p0, p1, side0, side1);
	if (side0 != -1) {
		newConnectors.push_back(newFaceEdgeConnector(side0, v0, v1));
	}
	if (side1 != -1) {
		newConnectors.push_back(newFaceEdgeConnector(side1, v0, v1));
	}
}


void CellFaceFacePair::validateOneSideIntersection(const POLY::Vertex* belowInside, const POLY::Vertex* belowOutside, const CoordinateFrame& otherInMe, ConnectorArray& newConnectors)
{
	Vector3 pBelowInside = otherInMe.pointToWorldSpace(belowInside->getOffset());
	Vector3 pBelowOutside = otherInMe.pointToWorldSpace(belowOutside->getOffset());

	int mainFaceEdgeId = mainFace->getInternalExtrusionIntersection(pBelowInside, pBelowOutside);
	if (mainFaceEdgeId >= 0) {
		newConnectors.push_back(newFaceEdgeConnector(mainFaceEdgeId, belowInside, belowOutside));
	}
}

void CellFaceFacePair::checkOneSideIntersection(const POLY::Vertex* v0, const POLY::Vertex* v1, const CoordinateFrame& otherInMe, ConnectorArray& newConnectors)
{
	Vector3 p0 = otherInMe.pointToWorldSpace(v0->getOffset());
	Vector3 p1 = otherInMe.pointToWorldSpace(v1->getOffset());

	int mainFaceEdgeId = mainFace->findInternalExtrusionIntersection(p0, p1);
	if (mainFaceEdgeId >= 0) {
		newConnectors.push_back(newFaceEdgeConnector(mainFaceEdgeId, v0, v1));
	}
}

void CellFaceFacePair::vertexInside(Primitive* pFace,
								Primitive* pVertex,
								const POLY::Vertex* inside, 
								const POLY::Face* planeFace, 
								ConnectorArray& newConnectors)
{
	FaceVertexConnector* answer = new FaceVertexConnector(	pFace->getBody(),
															pVertex->getBody(),
															contactParams,
															planeFace->plane(),
															inside->getOffset(),
															planeFace->getId(),
															inside->getId()			);			

	RBXASSERT(!Math::isNanInf(answer->computeOverlap()));
	newConnectors.push_back(answer);
}

FaceEdgeConnector* CellFaceFacePair::newFaceEdgeConnector(size_t mainFaceEdgeId, const Vertex* v0, const Vertex* v1)
{
	const POLY::Edge* penetratingEdge = Vertex::recoverEdge(v0, v1);

	FaceEdgeConnector* answer = new FaceEdgeConnector(	primitive[0]->getBody(),
														primitive[1]->getBody(),
														contactParams,
														mainFace->plane(),
														mainFace->getSidePlane(mainFaceEdgeId),
														mainFace->getEdge(mainFaceEdgeId)->computeLine(),
														penetratingEdge->computeLine(),
														mainFaceEdgeId,
														penetratingEdge->getId()			);		
	RBXASSERT(!Math::isNanInf(answer->computeOverlap()));
	return answer;
}	

///////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

// Find best edges - i.e. edge with the greatest distance (least penetration)

CellEdgeEdgePair::CellEdgeEdgePair(Primitive* mc, Primitive* p, const ContactParams& contactParams, PolyCellContact* aPCContact, bool swap) 
	: PolyCellPair(mc, p, contactParams, aPCContact, swap)
{
    bestEdge0 = myPCContact->getCellMesh()->getEdge(0);
    bestEdge1 = poly1()->getMesh()->getEdge(0);
}

PolyCellPair* CellEdgeEdgePair::allocateClone()
{
	return new CellEdgeEdgePair(*this);
}


float CellEdgeEdgePair::test()
{
	const CoordinateFrame& c0 = primitive[0]->getCoordinateFrame();
    const CoordinateFrame& c1 = primitive[1]->getCoordinateFrame();

	Vector3 p1InP0 = c0.pointToObjectSpace(c1.translation);
	Vector3 p0InP1 = c1.pointToObjectSpace(c0.translation);

	const Vertex* closestV0 = myPCContact->getCellMesh()->farthestVertex(p1InP0);
	const Vertex* closestV1 = poly1()->getMesh()->farthestVertex(p0InP1);

	const Mesh* mesh0 = myPCContact->getCellMesh();
	const Mesh* mesh1 = poly1()->getMesh();

	float bestDistance = -FLT_MAX;	// overlap

	size_t i_low = bestEdge0->getId();			// pre-seed - start with best chance of a separating plane
	size_t j_low = bestEdge1->getId();

	size_t i_mod = mesh0->numEdges();
	size_t j_mod = mesh1->numEdges();

	size_t i_high = i_low + i_mod;
	size_t j_high = j_low + j_mod;

	for (size_t i = i_low; i < i_high; ++i) {
		const POLY::Edge* e0 = mesh0->getEdge(i % i_mod);
		if (e0->contains(closestV0)) {					// only test edges containing the extremal vertex
			Line line0 = c0.toWorldSpace(e0->computeLine());
			for (size_t j = j_low; j < j_high; ++j) {
				const POLY::Edge* e1 = mesh1->getEdge(j % j_mod);
				if (e1->contains(closestV1)) {
					Line line1 = c1.toWorldSpace(e1->computeLine());

					Vector3 crossAxis = line0.direction().cross(line1.direction());
					if (crossAxis.unitize() > 1e-3f) {				// i.e. if not parallel threshold is 1e-3

						float min0, max0, min1, max1;

						Plane plane(crossAxis, 0.0f);
						Plane planeIn0 = c0.toObjectSpace(plane);
						Plane planeIn1 = c1.toObjectSpace(plane);

						computeMinMax(planeIn0, mesh0, min0, max0);
						computeMinMax(planeIn1, mesh1, min1, max1);

						float min = std::max(min0, min1);
						float max = std::min(max0, max1);
						float distance = min - max;			// negative distance - i.e. overlap
						if (distance > bestDistance) {
							bestDistance = distance;
							bestEdge0 = e0;
							bestEdge1 = e1;
							if (distance > 0.0) {
								return distance;
							}
						}
					}
				}
			}
		}
	}
	return bestDistance;
}

void CellEdgeEdgePair::computeMinMax(const Plane& planeInMesh, const Mesh* mesh, float& min, float& max)
{
	max = -FLT_MAX;
	min = FLT_MAX;

	for (size_t i = 0; i < mesh->numVertices(); ++i) {
		const Vector3& offset = mesh->getVertex(i)->getOffset();
		float projection = planeInMesh.distance(offset);
		min = std::min(min, projection);
		max = std::max(max, projection);
	}
}


void CellEdgeEdgePair::loadConnectors(ConnectorArray& newConnectors)
{
	if (bestEdge0 && bestEdge1) {
		newConnectors.push_back(newEdgeEdgeConnector());
	}
	else {
		RBXASSERT(0);
	}
}


EdgeEdgeConnector* CellEdgeEdgePair::newEdgeEdgeConnector()
{
	EdgeEdgeConnector* answer = new EdgeEdgeConnector(	primitive[0]->getBody(),
														primitive[1]->getBody(),
														contactParams,
														bestEdge0->computeLine(),
														bestEdge1->computeLine(),
														bestEdge0->getId(),
														bestEdge1->getId()			);		
	RBXASSERT(answer->computeOverlap());
	return answer;
}	

} // namespace
