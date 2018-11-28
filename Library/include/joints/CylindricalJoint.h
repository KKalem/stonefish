//
//  CylindricalJoint.h
//  Stonefish
//
//  Created by Patryk Cieslak on 28/03/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_CylindricalJoint__
#define __Stonefish_CylindricalJoint__

#include "joints/Joint.h"

namespace sf
{

class CylindricalJoint : public Joint
{
public:
    CylindricalJoint(std::string uniqueName, SolidEntity* solidA, SolidEntity* solidB, const Vector3& pivot, const Vector3& axis, bool collideLinkedEntities = true);
    
    void ApplyForce(Scalar F);
    void ApplyTorque(Scalar T);
    void ApplyDamping();
    bool SolvePositionIC(Scalar linearTolerance, Scalar angularTolerance);
   	
    Vector3 Render();
    
    void setDamping(Scalar linearConstantFactor, Scalar linearViscousFactor, Scalar angularConstantFactor, Scalar angularViscousFactor);
    void setLimits(Scalar linearMin, Scalar linearMax, Scalar angularMin, Scalar angularMax);
    void setIC(Scalar displacement, Scalar angle);
    
    JointType getType();
    
private:
    Vector3 axisInA;
    Vector3 pivotInA;
    Scalar linSigDamping;
    Scalar linVelDamping;
    Scalar angSigDamping;
    Scalar angVelDamping;
    Scalar displacementIC;
    Scalar angleIC;
};
    
}

#endif