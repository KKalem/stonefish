//
//  TorusShape.h
//  Stonefish
//
//  Created by Patryk Cieslak on 5/15/14.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_TorusShape__
#define __Stonefish_TorusShape__

#include <BulletCollision/CollisionShapes/btConvexInternalShape.h>
#include <BulletCollision/BroadphaseCollision/btBroadphaseProxy.h>
#include "StonefishCommon.h"

namespace sf
{

ATTRIBUTE_ALIGNED16(class) TorusShape : public btConvexInternalShape
{
protected:
    Scalar m_majorRadius;
	Scalar m_minorRadius;
    
public:
	BT_DECLARE_ALIGNED_ALLOCATOR();

	TorusShape(Scalar majorRadius, Scalar minorRadius);

    virtual Vector3 localGetSupportingVertex(const Vector3& vec)const;
	virtual Vector3 localGetSupportingVertexWithoutMargin(const Vector3& vec)const;
	virtual void batchedUnitVectorGetSupportingVertexWithoutMargin(const Vector3* vectors,Vector3* supportVerticesOut,int numVectors) const;

	virtual void calculateLocalInertia(Scalar mass,Vector3& inertia) const;
    
	virtual void getAabb(const Transform& t,Vector3& aabbMin,Vector3& aabbMax) const;
    
    Scalar getMajorRadius() const { return m_majorRadius; }
	Scalar getMinorRadius() const { return m_minorRadius; }
	Scalar getRadius() const { return getMajorRadius(); }
	
	Vector3 getHalfExtentsWithMargin() const;
	const Vector3& getHalfExtentsWithoutMargin() const;
	virtual void setLocalScaling(const Vector3& scaling);

	virtual const char*	getName()const { return "TORUS"; }
	
	virtual Vector3 getAnisotropicRollingFrictionDirection() const { return Vector3(0,1,0); }
};

}
    
#endif