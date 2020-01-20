/*    
    This file is a part of Stonefish.

    Stonefish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Stonefish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

//
//  FallingTestManager.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 04/03/2014.
//  Copyright (c) 2014-2019 Patryk Cieslak. All rights reserved.
//

#include "FallingTestManager.h"

#include "FallingTestApp.h"
#include <core/Robot.h>
#include <utils/SystemUtil.hpp>
#include <utils/UnitSystem.h>
#include <entities/statics/Plane.h>
#include <entities/statics/Obstacle.h>
#include <entities/solids/Polyhedron.h>
#include <entities/solids/Box.h>
#include <entities/solids/Sphere.h>
#include <entities/solids/Torus.h>
#include <entities/solids/Cylinder.h>
#include <graphics/OpenGLContent.h>
#include <sensors/scalar/IMU.h>
#include <sensors/scalar/RotaryEncoder.h>

FallingTestManager::FallingTestManager(sf::Scalar stepsPerSecond) 
    : SimulationManager(stepsPerSecond, sf::SolverType::SOLVER_SI, sf::CollisionFilteringType::COLLISION_EXCLUSIVE, sf::FluidDynamicsType::GEOMETRY_BASED)
{
}

void FallingTestManager::BuildScenario()
{
    setICSolverParams(false);
    
    ///////MATERIALS////////
    CreateMaterial("Ground", 1000.0, 1.0);
    CreateMaterial("Steel", sf::UnitSystem::Density(sf::CGS, sf::MKS, 1.0), 0.1);
    SetMaterialsInteraction("Ground", "Ground", 0.5, 0.3);
    SetMaterialsInteraction("Ground", "Steel", 0.5, 0.3);
    SetMaterialsInteraction("Steel", "Steel", 0.5, 0.3);
    
    ///////LOOKS///////////
    CreateLook("Grid", sf::Color::Gray(1.f), 0.8f, 0.f, 0.0f, sf::GetShaderPath() + "grid.png");
    CreateLook("Green", sf::Color::RGB(0.3f, 1.0f, 0.2f), 0.2f, 0.f);
    
    ////////OBJECTS
    sf::Plane* floor = new sf::Plane("Floor", 10000.f, "Ground", "Grid");
    AddStaticEntity(floor, sf::Transform::getIdentity());
    
    //Mechanical parts
    sf::Polyhedron* obj = new sf::Polyhedron("Base", sf::GetDataPath() + "sphere_R=1.obj", 0.1, sf::Transform(sf::Quaternion(0,0,0), sf::Vector3(0,0,0)), "Steel", sf::BodyPhysicsType::SURFACE_BODY, "Green");
    sf::Box* link1 = new sf::Box("Link1", sf::Vector3(0.1,0.02,0.5), sf::Transform(sf::Quaternion(M_PI_2,0,0), sf::Vector3(0.0,0.0,-0.2)), "Steel", sf::BodyPhysicsType::SURFACE_BODY, "");
    sf::Box* link2 = new sf::Box("Link2", sf::Vector3(0.1,0.02,0.5), sf::Transform(sf::Quaternion(M_PI_2,0,0), sf::Vector3(0.0,0.0,-0.2)), "Steel", sf::BodyPhysicsType::SURFACE_BODY, "Green");
    
    std::vector<sf::SolidEntity*> links;
    links.push_back(link1);
    links.push_back(link2);

    //Sensors
    sf::IMU* imu = new sf::IMU("IMU", -1, 1000);
    sf::RotaryEncoder* enc = new sf::RotaryEncoder("Encoder", -1, 1000);
    
    //Robot
    sf::Robot* robot = new sf::Robot("Robot", false);
    
    robot->DefineLinks(obj, links);
    robot->DefineRevoluteJoint("Joint1", "Base", "Link1",
                               sf::Transform(sf::IQ(), sf::Vector3(0,0.25,-0.2)), sf::Vector3(0,1,0), std::make_pair(1.0, -1.0));
    robot->DefineRevoluteJoint("Joint2", "Base", "Link2",
                               sf::Transform(sf::IQ(), sf::Vector3(0,-0.25,-0.2)), sf::Vector3(0,1,0), std::make_pair(1.0, -1.0));
    robot->BuildKinematicTree();
    robot->AddLinkSensor(imu, "Link2", sf::I4());
    robot->AddJointSensor(enc, "Joint2");
    
    AddRobot(robot, sf::Transform(sf::IQ(), sf::Vector3(0.0,0.0,-2.0)));
}
