//
//  PrismaticJoint.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 27/03/2014.
//  Copyright (c) 2014 Patryk Cieslak. All rights reserved.
//

#include "joints/PrismaticJoint.h"

using namespace sf;

PrismaticJoint::PrismaticJoint(std::string uniqueName, SolidEntity* solidA, SolidEntity* solidB, const Vector3& axis, bool collideLinkedEntities) : Joint(uniqueName, collideLinkedEntities)
{
    btRigidBody* bodyA = solidA->getRigidBody();
    btRigidBody* bodyB = solidB->getRigidBody();
    
    Vector3 sliderAxis = axis.normalized();
    Vector3 v2;
    if(fabs(sliderAxis.z()) > 0.8) v2 = Vector3(1,0,0); else v2 = Vector3(0,0,1);
    Vector3 v3 = (sliderAxis.cross(v2)).normalized();
    v2 = (v3.cross(sliderAxis)).normalized();
    Matrix3 sliderBasis(sliderAxis.x(), v2.x(), v3.x(),
                            sliderAxis.y(), v2.y(), v3.y(),
                            sliderAxis.z(), v2.z(), v3.z());
    Transform sliderFrame(sliderBasis, (bodyA->getCenterOfMassPosition() + bodyB->getCenterOfMassPosition())/Scalar(2.));
    
    Transform frameInA = bodyA->getCenterOfMassTransform().inverse() * sliderFrame;
    Transform frameInB = bodyB->getCenterOfMassTransform().inverse() * sliderFrame;
    axisInA = frameInA.getBasis().getColumn(0).normalized();
    
    btSliderConstraint* slider = new btSliderConstraint(*bodyA, *bodyB, frameInA, frameInB, true);
    slider->setLowerLinLimit(1.);
    slider->setUpperLinLimit(-1.);
    slider->setLowerAngLimit(0.);
    slider->setUpperAngLimit(0.);
    setConstraint(slider);
    
    sigDamping = Scalar(0.);
    velDamping = Scalar(0.);
    
    displacementIC = Scalar(0.);
}

void PrismaticJoint::setDamping(Scalar constantFactor, Scalar viscousFactor)
{
    sigDamping = constantFactor > Scalar(0) ? constantFactor : Scalar(0);
    velDamping = viscousFactor > Scalar(0) ? viscousFactor : Scalar(0);
}

void PrismaticJoint::setLimits(Scalar min, Scalar max)
{
    btSliderConstraint* slider = (btSliderConstraint*)getConstraint();
    slider->setLowerLinLimit(min);
    slider->setUpperLinLimit(max);
}

void PrismaticJoint::setIC(Scalar displacement)
{
    displacementIC = displacement;
}

JointType PrismaticJoint::getType()
{
    return JOINT_PRISMATIC;
}

void PrismaticJoint::ApplyForce(Scalar F)
{
    btRigidBody& bodyA = getConstraint()->getRigidBodyA();
    btRigidBody& bodyB = getConstraint()->getRigidBodyB();
    Vector3 axis = (bodyA.getCenterOfMassTransform().getBasis() * axisInA).normalized();
    Vector3 force = axis * F;
    bodyA.applyCentralForce(force);
    bodyB.applyCentralForce(-force);
}

void PrismaticJoint::ApplyDamping()
{
    if(sigDamping > Scalar(0.) || velDamping > Scalar(0.))
    {
        btRigidBody& bodyA = getConstraint()->getRigidBodyA();
        btRigidBody& bodyB = getConstraint()->getRigidBodyB();
        Vector3 axis = (bodyA.getCenterOfMassTransform().getBasis() * axisInA).normalized();
        Vector3 relativeV = bodyA.getLinearVelocity() - bodyB.getLinearVelocity();
        Scalar v = relativeV.dot(axis);
        
        if(v != Scalar(0.))
        {
            Scalar F = sigDamping * v/fabs(v) + velDamping * v;
            Vector3 force = axis * -F;
            
            bodyA.applyCentralForce(force);
            bodyB.applyCentralForce(-force);
        }
    }
}

bool PrismaticJoint::SolvePositionIC(Scalar linearTolerance, Scalar angularTolerance)
{
    return true;
}

Vector3 PrismaticJoint::Render()
{
    btTypedConstraint* slider = getConstraint();
    Vector3 A = slider->getRigidBodyA().getCenterOfMassPosition();
    Vector3 B = slider->getRigidBodyB().getCenterOfMassPosition();
    Vector3 pivot = (A+B)/Scalar(2.);
    Vector3 axis = (slider->getRigidBodyA().getCenterOfMassTransform().getBasis() * axisInA).normalized();
    
    //calculate axis ends
    Scalar e1 = (A-pivot).dot(axis);
    Scalar e2 = (B-pivot).dot(axis);
    Vector3 C1 = pivot + e1 * axis;
    Vector3 C2 = pivot + e2 * axis;
    
    std::vector<glm::vec3> vertices;
	vertices.push_back(glm::vec3(A.getX(), A.getY(), A.getZ()));
	vertices.push_back(glm::vec3(C1.getX(), C1.getY(), C1.getZ()));
	vertices.push_back(glm::vec3(B.getX(), B.getY(), B.getZ()));
	vertices.push_back(glm::vec3(C2.getX(), C2.getY(), C2.getZ()));
	OpenGLContent::getInstance()->DrawPrimitives(PrimitiveType::LINES, vertices, DUMMY_COLOR);
	vertices.clear();
	
	vertices.push_back(glm::vec3(C1.getX(), C1.getY(), C1.getZ()));
	vertices.push_back(glm::vec3(C2.getX(), C2.getY(), C2.getZ()));
	glEnable(GL_LINE_STIPPLE);
    OpenGLContent::getInstance()->DrawPrimitives(PrimitiveType::LINES, vertices, DUMMY_COLOR);
	glDisable(GL_LINE_STIPPLE);
    
    return (C1+C2)/Scalar(2.);
}