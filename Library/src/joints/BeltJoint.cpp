//
//  BeltJoint.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 28/03/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#include "joints/BeltJoint.h"

using namespace sf;

BeltJoint::BeltJoint(std::string uniqueName, SolidEntity* solidA, SolidEntity* solidB, const Vector3& axisA, const Vector3& axisB, Scalar ratio) : Joint(uniqueName, false)
{
    Vector3 newAxisB = -axisB; // Belt -> same direction of rotation
    gearRatio = ratio;
    
    btRigidBody* bodyA = solidA->getRigidBody();
    btRigidBody* bodyB = solidB->getRigidBody();
    Vector3 axisInA = bodyA->getCenterOfMassTransform().getBasis().inverse() * axisA;
    Vector3 axisInB = bodyB->getCenterOfMassTransform().getBasis().inverse() * newAxisB;
    
    btGearConstraint* gear = new btGearConstraint(*bodyA, *bodyB, axisInA, axisInB, gearRatio);
    setConstraint(gear);
}

JointType BeltJoint::getType()
{
    return JOINT_BELT;
}

Scalar BeltJoint::getRatio()
{
    return gearRatio;
}

Vector3 BeltJoint::Render()
{
    return Vector3();
}