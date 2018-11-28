//
//  SimulationManager.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 11/28/12.
//  Copyright (c) 2012-2018 Patryk Cieslak. All rights reserved.
//

#include "core/SimulationManager.h"

#include <BulletDynamics/ConstraintSolver/btNNCGConstraintSolver.h>
#include <BulletDynamics/MLCPSolvers/btDantzigSolver.h>
#include <BulletDynamics/MLCPSolvers/btSolveProjectedGaussSeidel.h>
#include <BulletDynamics/MLCPSolvers/btLemkeSolver.h>
#include <BulletDynamics/MLCPSolvers/btMLCPSolver.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <chrono>
#include <thread>
#include "core/FilteredCollisionDispatcher.h"
#include "core/GraphicalSimulationApp.h"
#include "graphics/OpenGLTrackball.h"
#include "utils/SystemUtil.hpp"
#include "utils/tinyxml2.h"
#include "utils/UnitSystem.h"
#include "entities/SolidEntity.h"
#include "entities/StaticEntity.h"
#include "entities/ForcefieldEntity.h"
#include "entities/forcefields/Ocean.h"
#include "entities/forcefields/Trigger.h"
#include "entities/statics/Plane.h"
#include "sensors/VisionSensor.h"

using namespace sf;

SimulationManager::SimulationManager(bool zAxisUp, Scalar stepsPerSecond, SolverType st, CollisionFilteringType cft, HydrodynamicsType ht)
{
    //Set coordinate system
    zUp = zAxisUp;
    
    //Initialize simulation world
    realtimeFactor = Scalar(1);
    solver = st;
    collisionFilter = cft;
    hydroType = ht;
    hydroCounter = 0;
    currentTime = 0;
    physicsTime = 0;
    simulationTime = 0;
    mlcpFallbacks = 0;
	nameManager = NULL;
    materialManager = NULL;
    dynamicsWorld = NULL;
    dwSolver = NULL;
    dwBroadphase = NULL;
    dwCollisionConfig = NULL;
    dwDispatcher = NULL;
    ocean = NULL;
	trackball = NULL;
    simHydroMutex = SDL_CreateMutex();
    simSettingsMutex = SDL_CreateMutex();
    simInfoMutex = SDL_CreateMutex();
    setStepsPerSecond(stepsPerSecond);
	
    //Set IC solver params
    icProblemSolved = false;
    setICSolverParams(false);
	simulationFresh = false;
    
    //Misc
    drawLightDummies = false;
    drawCameraDummies = false;
}

SimulationManager::~SimulationManager()
{
	DestroyScenario();
    SDL_DestroyMutex(simSettingsMutex);
    SDL_DestroyMutex(simInfoMutex);
    SDL_DestroyMutex(simHydroMutex);
	delete materialManager;
	delete nameManager;
}

bool SimulationManager::LoadSDF(const std::string& path)
{
    tinyxml2::XMLDocument doc;
    doc.LoadFile(path.c_str());
    return doc.ErrorID() == 0;
}

void SimulationManager::AddRobot(Robot* robot, const Transform& worldTransform)
{
    if(robot != NULL)
        robot->AddToSimulation(this, worldTransform);
}

void SimulationManager::AddEntity(Entity *ent)
{
    if(ent != NULL)
    {
        entities.push_back(ent);
        ent->AddToDynamicsWorld(dynamicsWorld);
    }
}

void SimulationManager::AddStaticEntity(StaticEntity* ent, const Transform& worldTransform)
{
    if(ent != NULL)
    {
        entities.push_back(ent);
        ent->AddToDynamicsWorld(dynamicsWorld, worldTransform);
    }
}

void SimulationManager::AddSolidEntity(SolidEntity* ent, const Transform& worldTransform)
{
    if(ent != NULL)
    {
        entities.push_back(ent);
        ent->AddToDynamicsWorld(dynamicsWorld, worldTransform);
    }
}

 void SimulationManager::AddFeatherstoneEntity(FeatherstoneEntity* ent, const Transform& worldTransform)
 {
     if(ent != NULL)
     {
         entities.push_back(ent);
         ent->AddToDynamicsWorld(dynamicsWorld, worldTransform);
     }
 }

void SimulationManager::EnableOcean(bool waves, Fluid* f)
{
	if(ocean != NULL)
		return;
	
    if(f == NULL)
    {
        std::string water = getMaterialManager()->CreateFluid("Water", UnitSystem::Density(CGS, MKS, 1.0), 1.308e-3, 1.55); 
        f = getMaterialManager()->getFluid(water);
    }
    
	ocean = new Ocean("Ocean", waves, f);
	ocean->AddToDynamicsWorld(dynamicsWorld);
	
	if(SimulationApp::getApp()->hasGraphics())
	{
		ocean->InitGraphics(simHydroMutex);
		ocean->setRenderable(true);
	}
}

void SimulationManager::AddSensor(Sensor *sens)
{
    if(sens != NULL)
        sensors.push_back(sens);
}

void SimulationManager::AddController(Controller *cntrl)
{
    if(cntrl != NULL)
        controllers.push_back(cntrl);
}

void SimulationManager::AddJoint(Joint *jnt)
{
    if(jnt != NULL)
    {
        joints.push_back(jnt);
        jnt->AddToDynamicsWorld(dynamicsWorld);
    }
}

void SimulationManager::AddActuator(Actuator *act)
{
    if(act != NULL)
        actuators.push_back(act);
}

Contact* SimulationManager::AddContact(Entity *entA, Entity *entB, size_type contactHistoryLength)
{
    Contact* contact = getContact(entA, entB);
    
    if(contact == NULL)
    {
        contact = new Contact(entA, entB, contactHistoryLength);
        contacts.push_back(contact);
        EnableCollision(entA, entB);
    }
    
    return contact;
}

int SimulationManager::CheckCollision(Entity *entA, Entity *entB)
{
    for(unsigned int i = 0; i < collisions.size(); ++i)
    {
        if((collisions[i].A == entA && collisions[i].B == entB) 
            || (collisions[i].B == entA && collisions[i].A == entB))
                return i;
    }
    
    return -1;
}

void SimulationManager::EnableCollision(Entity* entA, Entity* entB)
{
    int colId = CheckCollision(entA, entB);
    
    if(collisionFilter == INCLUSIVE && colId == -1)
    {
        Collision c;
        c.A = entA;
        c.B = entB;
        collisions.push_back(c);
    }
    else if(collisionFilter == EXCLUSIVE && colId > -1)
    {
        collisions.erase(collisions.begin() + colId);
    }
}
    
void SimulationManager::DisableCollision(Entity* entA, Entity* entB)
{
    int colId = CheckCollision(entA, entB);
    
    if(collisionFilter == EXCLUSIVE && colId == -1)
    {
        Collision c;
        c.A = entA;
        c.B = entB;
        collisions.push_back(c);
    }
    else if(collisionFilter == INCLUSIVE && colId > -1)
    {
        collisions.erase(collisions.begin() + colId);
    }
}

Contact* SimulationManager::getContact(Entity* entA, Entity* entB)
{
    for(unsigned int i = 0; i < contacts.size(); i++)
    {
        if(contacts[i]->getEntityA() == entA)
        {
            if(contacts[i]->getEntityB() == entB)
                return contacts[i];
        }
        else if(contacts[i]->getEntityB() == entA)
        {
            if(contacts[i]->getEntityA() == entB)
                return contacts[i];
        }
    }
    
    return NULL;
}

Contact* SimulationManager::getContact(unsigned int index)
{
    if(index < contacts.size())
        return contacts[index];
    else
        return NULL;
}

CollisionFilteringType SimulationManager::getCollisionFilter()
{
    return collisionFilter;
}

SolverType SimulationManager::getSolverType()
{
    return solver;
}

HydrodynamicsType SimulationManager::getHydrodynamicsType()
{
	return hydroType;
}

Entity* SimulationManager::getEntity(unsigned int index)
{
    if(index < entities.size())
        return entities[index];
    else
        return NULL;
}

Entity* SimulationManager::getEntity(std::string name)
{
    for(unsigned int i = 0; i < entities.size(); i++)
        if(entities[i]->getName() == name)
            return entities[i];
    
    return NULL;
}

Joint* SimulationManager::getJoint(unsigned int index)
{
    if(index < joints.size())
        return joints[index];
    else
        return NULL;
}

Joint* SimulationManager::getJoint(std::string name)
{
    for(unsigned int i = 0; i < joints.size(); i++)
        if(joints[i]->getName() == name)
            return joints[i];
    
    return NULL;
}

Actuator* SimulationManager::getActuator(unsigned int index)
{
    if(index < actuators.size())
        return actuators[index];
    else
        return NULL;
}

Actuator* SimulationManager::getActuator(std::string name)
{
    for(unsigned int i = 0; i < actuators.size(); i++)
        if(actuators[i]->getName() == name)
            return actuators[i];
    
    return NULL;
}

Sensor* SimulationManager::getSensor(unsigned int index)
{
    if(index < sensors.size())
        return sensors[index];
    else
        return NULL;
}

Sensor* SimulationManager::getSensor(std::string name)
{
    for(unsigned int i = 0; i < sensors.size(); i++)
        if(sensors[i]->getName() == name)
            return sensors[i];
    
    return NULL;
}

Controller* SimulationManager::getController(unsigned int index)
{
    if(index < controllers.size())
        return controllers[index];
    else
        return NULL;
}

Controller* SimulationManager::getController(std::string name)
{
    for(unsigned int i = 0; i < controllers.size(); i++)
        if(controllers[i]->getName() == name)
            return controllers[i];
    
    return NULL;
}

Ocean* SimulationManager::getOcean()
{
	return ocean;
}

btMultiBodyDynamicsWorld* SimulationManager::getDynamicsWorld()
{
    return dynamicsWorld;
}

bool SimulationManager::isZAxisUp()
{
    return zUp;
}

bool SimulationManager::isSimulationFresh()
{
	return simulationFresh;
}

Scalar SimulationManager::getSimulationTime()
{
    SDL_LockMutex(simInfoMutex);
    Scalar st = simulationTime;
    SDL_UnlockMutex(simInfoMutex);
    return st;
}

MaterialManager* SimulationManager::getMaterialManager()
{
    return materialManager;
}

NameManager* SimulationManager::getNameManager()
{
	return nameManager;
}

OpenGLTrackball* SimulationManager::getTrackball()
{
    return trackball;
}

void SimulationManager::setStepsPerSecond(Scalar steps)
{
    if(sps == steps)
        return;
    
    SDL_LockMutex(simSettingsMutex);
    sps = steps;
    ssus = (uint64_t)(1000000.0/steps);
    hydroPrescaler = (unsigned int)round(sps/Scalar(50));
    hydroPrescaler = hydroPrescaler == 0 ? 1 : hydroPrescaler;
    SDL_UnlockMutex(simSettingsMutex);
}

Scalar SimulationManager::getStepsPerSecond()
{
    return sps;
}

Scalar SimulationManager::getPhysicsTimeInMiliseconds()
{
    SDL_LockMutex(simInfoMutex);
    Scalar t = (Scalar)physicsTime/Scalar(1000);
    SDL_UnlockMutex(simInfoMutex);
    return t;
}

Scalar SimulationManager::getRealtimeFactor()
{
    SDL_LockMutex(simInfoMutex);
    Scalar rf = realtimeFactor;
    SDL_UnlockMutex(simInfoMutex);
	return rf;
}

void SimulationManager::getWorldAABB(Vector3& min, Vector3& max)
{
    min.setValue(BT_LARGE_FLOAT, BT_LARGE_FLOAT, BT_LARGE_FLOAT);
    max.setValue(-BT_LARGE_FLOAT, -BT_LARGE_FLOAT, -BT_LARGE_FLOAT);
    
    for(unsigned int i = 0; i < entities.size(); i++)
    {
        Vector3 entAabbMin, entAabbMax;
        entities[i]->getAABB(entAabbMin, entAabbMax);
        if(entAabbMin.x() < min.x()) min.setX(entAabbMin.x());
        if(entAabbMin.y() < min.y()) min.setY(entAabbMin.y());
        if(entAabbMin.z() < min.z()) min.setZ(entAabbMin.z());
        if(entAabbMax.x() > max.x()) max.setX(entAabbMax.x());
        if(entAabbMax.y() > max.y()) max.setY(entAabbMax.y());
        if(entAabbMax.z() > max.z()) max.setZ(entAabbMax.z());
    }
}

void SimulationManager::setGravity(Scalar gravityConstant)
{
    g = gravityConstant;
}

Vector3 SimulationManager::getGravity()
{
    return Vector3(0., 0., zUp ? -g : g);
}

void SimulationManager::setICSolverParams(bool useGravity, Scalar timeStep, unsigned int maxIterations, Scalar maxTime, Scalar linearTolerance, Scalar angularTolerance)
{
    icUseGravity = useGravity;
    icTimeStep = timeStep > SIMD_EPSILON ? timeStep : Scalar(0.001);
    icMaxIter = maxIterations > 0 ? maxIterations : INT_MAX;
    icMaxTime = maxTime > SIMD_EPSILON ? maxTime : BT_LARGE_FLOAT;
    icLinTolerance = linearTolerance > SIMD_EPSILON ? linearTolerance : Scalar(1e-6);
    icAngTolerance = angularTolerance > SIMD_EPSILON ? angularTolerance : Scalar(1e-6);
}

void SimulationManager::InitializeSolver()
{
    dwBroadphase = new btDbvtBroadphase();
    dwCollisionConfig = new btDefaultCollisionConfiguration();
  
    //Choose collision dispatcher
    switch(collisionFilter)
    {
        case INCLUSIVE:
            dwDispatcher = new FilteredCollisionDispatcher(dwCollisionConfig, true);
            break;

        case EXCLUSIVE:
            dwDispatcher = new FilteredCollisionDispatcher(dwCollisionConfig, false);
            break;
    }
    
    //Choose constraint solver
    if(solver == SolverType::SI)
	{
		dwSolver = new btMultiBodyConstraintSolver();
	}
	else
	{
		btMLCPSolverInterface* mlcp;
    
		switch(solver)
		{
            default:
			case SolverType::DANTZIG:
				mlcp = new btDantzigSolver();
				break;
            
			case SolverType::PROJ_GAUSS_SIEDEL:
				mlcp = new btSolveProjectedGaussSeidel();
				break;
            
			case SolverType::LEMKE:
				mlcp = new btLemkeSolver();
				//((btLemkeSolver*)mlcp)->m_maxLoops = 10000;
				break;
		}
		
		dwSolver = new ResearchConstraintSolver(mlcp);
	}
    
    //Create dynamics world
    dynamicsWorld = new btMultiBodyDynamicsWorld(dwDispatcher, dwBroadphase, dwSolver, dwCollisionConfig);
    
    //Basic configuration
    dynamicsWorld->getSolverInfo().m_solverMode = SOLVER_USE_WARMSTARTING | SOLVER_SIMD | SOLVER_USE_2_FRICTION_DIRECTIONS | SOLVER_RANDMIZE_ORDER; // | SOLVER_ENABLE_FRICTION_DIRECTION_CACHING; //| SOLVER_RANDMIZE_ORDER;
    dynamicsWorld->getSolverInfo().m_warmstartingFactor = Scalar(1.);
    dynamicsWorld->getSolverInfo().m_minimumSolverBatchSize = 256;
	
    //Quality/stability
	dynamicsWorld->getSolverInfo().m_tau = Scalar(1.);  //mass factor
    dynamicsWorld->getSolverInfo().m_erp = Scalar(0.25);  //non-contact constraint error reduction
    dynamicsWorld->getSolverInfo().m_erp2 = Scalar(0.75); //contact constraint error reduction
    dynamicsWorld->getSolverInfo().m_frictionERP = Scalar(0.5); //friction constraint error reduction
    dynamicsWorld->getSolverInfo().m_numIterations = 100; //number of constraint iterations
    dynamicsWorld->getSolverInfo().m_sor = Scalar(1.0); //not used
    dynamicsWorld->getSolverInfo().m_maxErrorReduction = Scalar(0.); //not used
    
    //Collision
    dynamicsWorld->getSolverInfo().m_splitImpulse = true; //avoid adding energy to the system
    dynamicsWorld->getSolverInfo().m_splitImpulsePenetrationThreshold = Scalar(0.0); //value close to zero needed for accurate friction/too close to zero causes multibody sinking
    dynamicsWorld->getSolverInfo().m_splitImpulseTurnErp = Scalar(1.0); //error reduction for rigid body angular velocity
    dynamicsWorld->getDispatchInfo().m_useContinuous = false;
    dynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = Scalar(0.0);
    dynamicsWorld->setApplySpeculativeContactRestitution(false); //to make it work one needs restitution in the m_restitution field
	dynamicsWorld->getSolverInfo().m_restitutionVelocityThreshold = Scalar(0.05); //Velocity at which restitution is overwritten with 0 (bodies stick, stop vibrating)
    
    //Special forces
    dynamicsWorld->getSolverInfo().m_maxGyroscopicForce = Scalar(1e30); //gyroscopic effect
    
    //Unrealistic components
    dynamicsWorld->getSolverInfo().m_globalCfm = Scalar(0.); //global constraint force mixing factor
    dynamicsWorld->getSolverInfo().m_damping = Scalar(0.); //global damping
    dynamicsWorld->getSolverInfo().m_friction = Scalar(0.); //global friction
    dynamicsWorld->getSolverInfo().m_frictionCFM = Scalar(0.); //friction constraint force mixing factor
    dynamicsWorld->getSolverInfo().m_singleAxisRollingFrictionThreshold = Scalar(1e30); //single axis rolling velocity threshold
    dynamicsWorld->getSolverInfo().m_linearSlop = Scalar(0.); //position bias
    
    //Override default callbacks
    dynamicsWorld->setWorldUserInfo(this);
    dynamicsWorld->getPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
    gContactAddedCallback = SimulationManager::CustomMaterialCombinerCallback;
    dynamicsWorld->setSynchronizeAllMotionStates(false);
    
    //Set default params
    g = Scalar(9.81);
    
	//Create name manager
	nameManager = new NameManager();
	
    //Create material manager & load standard materials
    materialManager = new MaterialManager();
    
    //Debugging
    debugDrawer = new OpenGLDebugDrawer(btIDebugDraw::DBG_DrawWireframe, zUp);
    dynamicsWorld->setDebugDrawer(debugDrawer);
}

void SimulationManager::InitializeScenario()
{
	if(SimulationApp::getApp()->hasGraphics())
	{
		GraphicalSimulationApp* gApp = (GraphicalSimulationApp*)SimulationApp::getApp();
		trackball = new OpenGLTrackball(Vector3(0,0,1.0), 1.0, Vector3(0,0, 1.0), 0, 0, gApp->getWindowWidth(), gApp->getWindowHeight(), 90.f, 1000.f, 4, true);
		trackball->Rotate(Quaternion(0.25, 0.0, 0.0));
		OpenGLContent::getInstance()->AddView(trackball);
	}
}

void SimulationManager::RestartScenario()
{
    DestroyScenario();
    InitializeSolver();
    InitializeScenario();
    BuildScenario(); //Defined by specific application
	OpenGLContent::getInstance()->Finalize();
	
	simulationFresh = true;
}

void SimulationManager::DestroyScenario()
{
    if(dynamicsWorld != NULL)
    {
        //remove objects from dynamic world
        for(int i = dynamicsWorld->getNumConstraints()-1; i >= 0; i--)
        {
            btTypedConstraint* constraint = dynamicsWorld->getConstraint(i);
            dynamicsWorld->removeConstraint(constraint);
            delete constraint;
        }
    
        for(int i = dynamicsWorld->getNumCollisionObjects()-1; i >= 0; i--)
        {
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
            btRigidBody* body = btRigidBody::upcast(obj);
            if (body && body->getMotionState())
                delete body->getMotionState();
            dynamicsWorld->removeCollisionObject(obj);
            delete obj;
        }
    
        delete dynamicsWorld;
        delete dwSolver;
        delete dwBroadphase;
        delete dwDispatcher;
        delete dwCollisionConfig;
        delete debugDrawer;
    }
    
    //remove sim manager objects
    for(unsigned int i=0; i<entities.size(); i++)
        delete entities[i];
    entities.clear();
    
    if(ocean != NULL)
	{
        delete ocean;
		ocean = NULL;
	}
    
    for(unsigned int i=0; i<joints.size(); i++)
        delete joints[i];
    joints.clear();
    
    for(unsigned int i=0; i<contacts.size(); i++)
        delete contacts[i];
    contacts.clear();
    
    for(unsigned int i=0; i<sensors.size(); i++)
        delete sensors[i];
    sensors.clear();
    
    for(unsigned int i=0; i<actuators.size(); i++)
        delete actuators[i];
    actuators.clear();
    
    for(unsigned int i=0; i<controllers.size(); i++)
        delete controllers[i];
    controllers.clear();
	
	if(nameManager != NULL)
		nameManager->ClearNames();
		
    if(materialManager != NULL)
        materialManager->ClearMaterialsAndFluids();

	OpenGLContent::getInstance()->DestroyContent();
}

bool SimulationManager::StartSimulation()
{
	simulationFresh = false;
    currentTime = 0;
    physicsTime = 0;
    simulationTime = 0;
    mlcpFallbacks = 0;
    hydroCounter = 0;
    
    //Solve initial conditions problem
    if(!SolveICProblem())
        return false;
    
    //Reset contacts
    for(unsigned int i = 0; i < contacts.size(); i++)
        contacts[i]->ClearHistory();
    
    //Reset sensors
    for(unsigned int i = 0; i < sensors.size(); i++)
        sensors[i]->Reset();
    
    //Turn on controllers
    for(unsigned int i = 0; i < controllers.size(); i++)
        controllers[i]->Start();
    
    return true;
}

void SimulationManager::ResumeSimulation()
{
    if(!icProblemSolved)
        StartSimulation();
    else
    {
        currentTime = 0;

        for(unsigned int i = 0; i < controllers.size(); i++)
            controllers[i]->Start();
    }
}

void SimulationManager::StopSimulation()
{
    for(unsigned int i=0; i < controllers.size(); i++)
        controllers[i]->Stop();
}

bool SimulationManager::SolveICProblem()
{
    //Solve for joint positions
    icProblemSolved = false;
    
    //Should use gravity?
    if(icUseGravity)
        dynamicsWorld->setGravity(Vector3(0., 0., zUp ? -g : g));
    else
        dynamicsWorld->setGravity(Vector3(0.,0.,0.));
    
    //Set IC callback
    dynamicsWorld->setInternalTickCallback(SolveICTickCallback, this, true); //Pre-tick
    dynamicsWorld->setInternalTickCallback(NULL, this, false); //Post-tick
    
    uint64_t icTime = GetTimeInMicroseconds();
    unsigned int iterations = 0;
    
    do
    {
        if(iterations > icMaxIter) //Check iterations limit
        {
            cError("IC problem not solved! Reached maximum interation count.");
            return false;
        }
        else if((GetTimeInMicroseconds() - icTime)/(double)1e6 > icMaxTime) //Check time limit
        {
            cError("IC problem not solved! Reached maximum time.");
            return false;
        }
        
        //Simulate world
        dynamicsWorld->stepSimulation(icTimeStep, 1, icTimeStep);
        iterations++;
    }
    while(!icProblemSolved);
    
    double solveTime = (GetTimeInMicroseconds() - icTime)/(double)1e6;
    
    //Synchronize body transforms
    dynamicsWorld->synchronizeMotionStates();
    simulationTime = Scalar(0.);

    //Solving time
    cInfo("IC problem solved with %d iterations in %1.6lf s.", iterations, solveTime);
    
    //Set gravity
    dynamicsWorld->setGravity(Vector3(0., 0., zUp ? -g : g));
    
    //Set simulation tick
    dynamicsWorld->setInternalTickCallback(SimulationTickCallback, this, true); //Pre-tick
    dynamicsWorld->setInternalTickCallback(SimulationPostTickCallback, this, false); //Post-tick
    return true;
}

void SimulationManager::AdvanceSimulation()
{
    //Check if initial conditions solved
    if(!icProblemSolved)
        return;
		
    //Calculate eleapsed time
	uint64_t timeInMicroseconds = GetTimeInMicroseconds();
	uint64_t deltaTime;
    
    //Start of simulation
    if(currentTime == 0)
	{
        deltaTime = 0.0;
		currentTime = timeInMicroseconds;
		return;
	}
    
    //Calculate and adjust delta
    deltaTime = timeInMicroseconds - currentTime;
    currentTime = timeInMicroseconds;
	
    if(deltaTime < ssus)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(ssus - deltaTime));
        timeInMicroseconds = GetTimeInMicroseconds();
        deltaTime = timeInMicroseconds - (currentTime - deltaTime);
        currentTime = timeInMicroseconds;
    }
    
    //Step simulation
    SDL_LockMutex(simSettingsMutex);
    uint64_t physicsStart = GetTimeInMicroseconds();
    dynamicsWorld->stepSimulation((Scalar)deltaTime * realtimeFactor/Scalar(1000000.0), 1000000, (Scalar)ssus/Scalar(1000000.0));
    uint64_t physicsEnd = GetTimeInMicroseconds();
    SDL_UnlockMutex(simSettingsMutex);
    
    SDL_LockMutex(simInfoMutex);
    physicsTime = physicsEnd - physicsStart;
    
    /*Scalar factor1 = (Scalar)deltaTime/(Scalar)physicsTime;
    Scalar factor2 = Scalar(1000000.0/60.0)/(Scalar)physicsTime;
    realtimeFactor *=  factor1*factor2;
    realtimeFactor = realtimeFactor < Scalar(0.05) ? Scalar(0.05) : (realtimeFactor > Scalar(1) ? Scalar(1) : realtimeFactor);*/
	realtimeFactor = Scalar(1);
    
    //Inform about MLCP failures
	if(solver != SolverType::SI)
	{
		int numFallbacks = ((ResearchConstraintSolver*)dwSolver)->getNumFallbacks();
		if(numFallbacks)
		{
			mlcpFallbacks += numFallbacks;
#ifdef DEBUG
            cWarning("MLCP solver failed %d times.\n", mlcpFallbacks);
#endif
		}
		((ResearchConstraintSolver*)dwSolver)->setNumFallbacks(0);
	}
    
    SDL_UnlockMutex(simInfoMutex);
}

void SimulationManager::SimulationStepCompleted()
{
#ifdef DEBUG
	if(!SimulationApp::getApp()->hasGraphics())
		cInfo("Simulation time: %1.3lf s", getSimulationTime());
#endif	
}

void SimulationManager::UpdateDrawingQueue()
{
	//Build new drawing queue
    OpenGLPipeline* glPipeline = ((GraphicalSimulationApp*)SimulationApp::getApp())->getGLPipeline();
    
    //Solids, manipulators, systems....
	for(unsigned int i=0; i<entities.size(); ++i)
	{
		std::vector<Renderable> items = entities[i]->Render();
		for(unsigned int h=0; h<items.size(); ++h)
		{
			if(!zUp)
				items[h].model = glm::rotate((float)M_PI, glm::vec3(0,1.f,0)) * items[h].model;
			
            glPipeline->AddToDrawingQueue(items[h]);
		}
	}
    
    //Motors, thrusters, propellers, fins....
    for(unsigned int i=0; i<actuators.size(); ++i)
    {
        std::vector<Renderable> items = actuators[i]->Render();
		for(unsigned int h=0; h<items.size(); ++h)
		{
			if(!zUp)
				items[h].model = glm::rotate((float)M_PI, glm::vec3(0,1.f,0)) * items[h].model;
			
			glPipeline->AddToDrawingQueue(items[h]);
		}
    }
    
    //Sensors
    for(unsigned int i=0; i<sensors.size(); ++i)
    {
        std::vector<Renderable> items = sensors[i]->Render();
		for(unsigned int h=0; h<items.size(); ++h)
		{
			if(!zUp)
				items[h].model = glm::rotate((float)M_PI, glm::vec3(0,1.f,0)) * items[h].model;
			
			glPipeline->AddToDrawingQueue(items[h]);
		}
        
        if(sensors[i]->getType() == SensorType::SENSOR_VISION)
			((VisionSensor*)sensors[i])->UpdateTransform();
    }
    
    //Contacts
    for(unsigned int i=0; i<contacts.size(); ++i)
    {
        std::vector<Renderable> items = contacts[i]->Render();
		for(unsigned int h=0; h<items.size(); ++h)
		{
			if(!zUp)
				items[h].model = glm::rotate((float)M_PI, glm::vec3(0,1.f,0)) * items[h].model;
			
			glPipeline->AddToDrawingQueue(items[h]);
		}
    }
    
    if(ocean != NULL)
    {
        std::vector<Renderable> items = ocean->Render();
        for(unsigned int h=0; h<items.size(); ++h)
		{
			if(!zUp)
				items[h].model = glm::rotate((float)M_PI, glm::vec3(0,1.f,0)) * items[h].model;
			
			glPipeline->AddToDrawingQueue(items[h]);
		}
    }
}

Entity* SimulationManager::PickEntity(int x, int y)
{
	/*
    for(int i = 0; i < views.size(); i++)
    {
        if(views[i]->isActive())
        {
            Vector3 ray = views[i]->Ray(x, y);
            if(ray.length2() > 0)
            {
                btCollisionWorld::ClosestRayResultCallback rayCallback(views[i]->GetEyePosition(), ray);
                dynamicsWorld->rayTest(views[i]->GetEyePosition(), ray, rayCallback);
                
                if (rayCallback.hasHit())
                {
                    Entity* ent = (Entity*)rayCallback.m_collisionObject->getUserPointer();
                    return ent;
                }
                else
                    return NULL;
            }
        }
    }
    */
    return NULL;
}

extern ContactAddedCallback gContactAddedCallback;

bool SimulationManager::CustomMaterialCombinerCallback(btManifoldPoint& cp,	const btCollisionObjectWrapper* colObj0Wrap,int partId0,int index0,const btCollisionObjectWrapper* colObj1Wrap,int partId1,int index1)
{
    Entity* ent0 = (Entity*)colObj0Wrap->getCollisionObject()->getUserPointer();
    Entity* ent1 = (Entity*)colObj1Wrap->getCollisionObject()->getUserPointer();
    
    if(ent0 == NULL || ent1 == NULL)
    {
        cp.m_combinedFriction = Scalar(0.);
        cp.m_combinedRollingFriction = Scalar(0.);
        cp.m_combinedRestitution = Scalar(0.);
        return true;
    }
    
	MaterialManager* mm = SimulationApp::getApp()->getSimulationManager()->getMaterialManager();
	
    Material mat0;
    Vector3 contactVelocity0;
    Scalar contactAngularVelocity0;
    
    if(ent0->getType() == ENTITY_STATIC)
    {
		StaticEntity* sent0 = (StaticEntity*)ent0;
        mat0 = sent0->getMaterial();
        contactVelocity0.setZero();
        contactAngularVelocity0 = Scalar(0);
    }
    else if(ent0->getType() == ENTITY_SOLID)
    {
        SolidEntity* sent0 = (SolidEntity*)ent0;
        mat0 = sent0->getMaterial();
		//Vector3 localPoint0 = sent0->getTransform().getBasis() * cp.m_localPointA;
		Vector3 localPoint0 = sent0->getCGTransform().inverse() * cp.getPositionWorldOnA();
		contactVelocity0 = sent0->getLinearVelocityInLocalPoint(localPoint0);
		contactAngularVelocity0 = sent0->getAngularVelocity().dot(-cp.m_normalWorldOnB);
    }
    else
    {
        cp.m_combinedFriction = Scalar(0);
        cp.m_combinedRollingFriction = Scalar(0);
        cp.m_combinedRestitution = Scalar(0);
        return true;
    }
    
    Material mat1;
    Vector3 contactVelocity1;
    Scalar contactAngularVelocity1;
    
    if(ent1->getType() == ENTITY_STATIC)
    {
		StaticEntity* sent1 = (StaticEntity*)ent1;
        mat1 = sent1->getMaterial();
        contactVelocity1.setZero();
        contactAngularVelocity1 = Scalar(0);
    }
    else if(ent1->getType() == ENTITY_SOLID)
    {
        SolidEntity* sent1 = (SolidEntity*)ent1;
        mat1 = sent1->getMaterial();
        //Vector3 localPoint1 = sent1->getTransform().getBasis() * cp.m_localPointB;
		Vector3 localPoint1 = sent1->getCGTransform().inverse() * cp.getPositionWorldOnB();
        contactVelocity1 = sent1->getLinearVelocityInLocalPoint(localPoint1);
        contactAngularVelocity1 = sent1->getAngularVelocity().dot(cp.m_normalWorldOnB);
    }
    else
    {
        cp.m_combinedFriction = Scalar(0);
        cp.m_combinedRollingFriction = Scalar(0);
        cp.m_combinedRestitution = Scalar(0);
        return true;
    }
	
    Vector3 relLocalVel = contactVelocity1 - contactVelocity0;
    Vector3 normalVel = cp.m_normalWorldOnB * cp.m_normalWorldOnB.dot(relLocalVel);
    Vector3 slipVel = relLocalVel - normalVel;
    Scalar slipVelMod = slipVel.length();
    Scalar sigma = 100;
    // f = (static - dynamic)/(sigma * v^2 + 1) + dynamic
	Friction f = mm->GetMaterialsInteraction(mat0.name, mat1.name);
	cp.m_combinedFriction = (f.fStatic - f.fDynamic)/(sigma * slipVelMod * slipVelMod + Scalar(1)) + f.fDynamic;
	
    //Rolling friction not possible to generalize - needs special treatment
    cp.m_combinedRollingFriction = Scalar(0);
    
    //Slipping
    cp.m_userPersistentData = (void *)(new Vector3(slipVel));
    
    //Damping angular velocity around contact normal (reduce spinning)
    //calculate relative angular velocity
    Scalar relAngularVelocity01 = contactAngularVelocity0 - contactAngularVelocity1;
    Scalar relAngularVelocity10 = contactAngularVelocity1 - contactAngularVelocity0;
    
    //calculate contact normal force and friction torque
    Scalar normalForce = cp.m_appliedImpulse * SimulationApp::getApp()->getSimulationManager()->getStepsPerSecond();
    Scalar T = cp.m_combinedFriction * normalForce * 0.002;

    //apply damping torque
    if(ent0->getType() == ENTITY_SOLID && !btFuzzyZero(relAngularVelocity01))
        ((SolidEntity*)ent0)->ApplyTorque(cp.m_normalWorldOnB * relAngularVelocity01/btFabs(relAngularVelocity01) * T);
    
    if(ent1->getType() == ENTITY_SOLID && !btFuzzyZero(relAngularVelocity10))
        ((SolidEntity*)ent1)->ApplyTorque(cp.m_normalWorldOnB * relAngularVelocity10/btFabs(relAngularVelocity10) * T);
    
    //Restitution
    cp.m_combinedRestitution = mat0.restitution * mat1.restitution;
    
    //printf("%s <-> %s  R:%1.3lf F:%1.3lf\n", ent0->getName().c_str(), ent1->getName().c_str(), cp.m_combinedRestitution, cp.m_combinedFriction);
    
    return true;
}

void SimulationManager::SolveICTickCallback(btDynamicsWorld* world, Scalar timeStep)
{
    SimulationManager* simManager = (SimulationManager*)world->getWorldUserInfo();
    btMultiBodyDynamicsWorld* researchWorld = (btMultiBodyDynamicsWorld*)world;
    
    //Clear all forces to ensure that no summing occurs
    researchWorld->clearForces(); //Includes clearing of multibody forces!
    
    //Solve for objects settling
    bool objectsSettled = true;
    
    if(simManager->icUseGravity)
    {
        //Apply gravity to bodies
        for(unsigned int i = 0; i < simManager->entities.size(); i++)
        {
            if(simManager->entities[i]->getType() == ENTITY_SOLID)
            {
                SolidEntity* solid = (SolidEntity*)simManager->entities[i];
                solid->ApplyGravity(world->getGravity());
            }
            else if(simManager->entities[i]->getType() == ENTITY_FEATHERSTONE)
            {
                FeatherstoneEntity* feather = (FeatherstoneEntity*)simManager->entities[i];
                feather->ApplyGravity(world->getGravity());
            }
        }
        
        if(simManager->simulationTime < Scalar(0.01)) //Wait for a few cycles to ensure bodies started moving
            objectsSettled = false;
        else
        {
            //Check if objects settled
            for(unsigned int i = 0; i < simManager->entities.size(); i++)
            {
                if(simManager->entities[i]->getType() == ENTITY_SOLID)
                {
                    SolidEntity* solid = (SolidEntity*)simManager->entities[i];
                    if(solid->getLinearVelocity().length() > simManager->icLinTolerance * Scalar(100.) || solid->getAngularVelocity().length() > simManager->icAngTolerance * Scalar(100.))
                    {
                        objectsSettled = false;
                        break;
                    }
                }
                else if(simManager->entities[i]->getType() == ENTITY_FEATHERSTONE)
                {
                    FeatherstoneEntity* multibody = (FeatherstoneEntity*)simManager->entities[i];
                    
                    //Check base velocity
                    Vector3 baseLinVel = multibody->getLinkLinearVelocity(0);
                    Vector3 baseAngVel = multibody->getLinkAngularVelocity(0);
                    
                    if(baseLinVel.length() > simManager->icLinTolerance * Scalar(100.) || baseAngVel.length() > simManager->icAngTolerance * Scalar(100.0))
                    {
                        objectsSettled = false;
                        break;
                    }
                    
                    //Loop through all joints
                    for(unsigned int h = 0; h < multibody->getNumOfJoints(); h++)
                    {
                        Scalar jVelocity;
                        btMultibodyLink::eFeatherstoneJointType jType;
                        multibody->getJointVelocity(h, jVelocity, jType);
                        
                        switch(jType)
                        {
                            case btMultibodyLink::eRevolute:
                                if(Vector3(jVelocity,0,0).length() > simManager->icAngTolerance * Scalar(100.))
                                    objectsSettled = false;
                                break;
                                
                            case btMultibodyLink::ePrismatic:
                                if(Vector3(jVelocity,0,0).length() > simManager->icLinTolerance * Scalar(100.))
                                    objectsSettled = false;
                                break;
                                
                            default:
                                break;
                        }
                        
                        if(!objectsSettled)
                            break;
                    }
                }
            }
        }
    }
    
    //Solve for joint initial conditions
    bool jointsICSolved = true;
    
    for(unsigned int i = 0; i < simManager->joints.size(); i++)
        if(!simManager->joints[i]->SolvePositionIC(simManager->icLinTolerance, simManager->icAngTolerance))
            jointsICSolved = false;

    //Check if everything solved
    if(objectsSettled && jointsICSolved)
        simManager->icProblemSolved = true;
    
    //Update time
    simManager->simulationTime += timeStep;
}

//Used to apply and accumulate forces
void SimulationManager::SimulationTickCallback(btDynamicsWorld* world, Scalar timeStep)
{
    SimulationManager* simManager = (SimulationManager*)world->getWorldUserInfo();
    btMultiBodyDynamicsWorld* mbDynamicsWorld = (btMultiBodyDynamicsWorld*)world;
    	
    //Clear all forces to ensure that no summing occurs
    mbDynamicsWorld->clearForces(); //Includes clearing of multibody forces!
        
    //loop through all actuators -> apply forces to bodies (free and connected by joints)
    for(unsigned int i = 0; i < simManager->actuators.size(); ++i)
        simManager->actuators[i]->Update(timeStep);
    
    //loop through all joints -> apply damping forces to bodies connected by joints
    for(unsigned int i = 0; i < simManager->joints.size(); ++i)
        simManager->joints[i]->ApplyDamping();
    
    //loop through all entities that may need special actions
    for(unsigned int i = 0; i < simManager->entities.size(); ++i)
    {
        Entity* ent = simManager->entities[i];
        
        if(ent->getType() == ENTITY_SOLID)
        {
            SolidEntity* solid = (SolidEntity*)ent;
            solid->ApplyGravity(mbDynamicsWorld->getGravity());
        }
        else if(ent->getType() == ENTITY_FEATHERSTONE)
        {
            FeatherstoneEntity* multibody = (FeatherstoneEntity*)ent;
            multibody->ApplyGravity(mbDynamicsWorld->getGravity());
            multibody->ApplyDamping();
        }
       /* else if(ent->getType() == ENTITY_CABLE)
        {
            CableEntity* cable = (CableEntity*)ent;
            cable->ApplyGravity();
        }*/
        /*else if(ent->getType() == ENTITY_SYSTEM)
        {
            SystemEntity* system = (SystemEntity*)ent;
            system->UpdateActuators(timeStep);
            system->ApplyGravity(mbDynamicsWorld->getGravity());
			system->ApplyDamping();
        }*/
		else if(ent->getType() == ENTITY_FORCEFIELD)
		{
			ForcefieldEntity* ff = (ForcefieldEntity*)ent;
			if(ff->getForcefieldType() == FORCEFIELD_TRIGGER)
			{				
				Trigger* trigger = (Trigger*)ff;
				trigger->Clear();
				btBroadphasePairArray& pairArray = trigger->getGhost()->getOverlappingPairCache()->getOverlappingPairArray();
				int numPairs = pairArray.size();
					
				for(int h=0; h<numPairs; h++)
				{
					const btBroadphasePair& pair = pairArray[h];
					btBroadphasePair* colPair = world->getPairCache()->findPair(pair.m_pProxy0, pair.m_pProxy1);
					if(!colPair)
						continue;
                    
					btCollisionObject* co1 = (btCollisionObject*)colPair->m_pProxy0->m_clientObject;
					btCollisionObject* co2 = (btCollisionObject*)colPair->m_pProxy1->m_clientObject;
                
					if(co1 == trigger->getGhost())
						trigger->Activate(co2);
					else if(co2 == trigger->getGhost())
						trigger->Activate(co1);
				}
			}
		}
    }
    
	//Fluid forces
    if(simManager->ocean != NULL)
    {
        bool recompute = simManager->hydroCounter % simManager->hydroPrescaler == 0;
        
        if(recompute) SDL_LockMutex(simManager->simHydroMutex);
        
        btBroadphasePairArray& pairArray = simManager->ocean->getGhost()->getOverlappingPairCache()->getOverlappingPairArray();
        int numPairs = pairArray.size();
        if(numPairs > 0)
        {    
            for(int h=0; h<numPairs; h++)
            {
                const btBroadphasePair& pair = pairArray[h];
                btBroadphasePair* colPair = world->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
                if (!colPair)
                    continue;
                    
                btCollisionObject* co1 = (btCollisionObject*)colPair->m_pProxy0->m_clientObject;
                btCollisionObject* co2 = (btCollisionObject*)colPair->m_pProxy1->m_clientObject;
                
                if(co1 == simManager->ocean->getGhost())
                    simManager->ocean->ApplyFluidForces(simManager->getHydrodynamicsType(), world, co2, recompute);
                else if(co2 == simManager->ocean->getGhost())
                    simManager->ocean->ApplyFluidForces(simManager->getHydrodynamicsType(), world, co1, recompute);
            }
        }
        
        ++simManager->hydroCounter;
        
        if(recompute) SDL_UnlockMutex(simManager->simHydroMutex);
    }
}

//Used to measure body motions and calculate controls
void SimulationManager::SimulationPostTickCallback(btDynamicsWorld *world, Scalar timeStep)
{
    SimulationManager* simManager = (SimulationManager*)world->getWorldUserInfo();
	
	//Update acceleration data
	for(unsigned int i = 0 ; i < simManager->entities.size(); ++i)
    {
        Entity* ent = simManager->entities[i];
            
        if(ent->getType() == ENTITY_SOLID)
        {
            SolidEntity* solid = (SolidEntity*)ent;
            solid->UpdateAcceleration(timeStep);
        }
		else if(ent->getType() == ENTITY_FEATHERSTONE)
		{
			FeatherstoneEntity* fe = (FeatherstoneEntity*)ent;
			fe->UpdateAcceleration(timeStep);
		}
    }
	
	//loop through all sensors -> update measurements
    for(unsigned int i = 0; i < simManager->sensors.size(); ++i)
        simManager->sensors[i]->Update(timeStep);
    
    //loop through all controllers
    for(unsigned int i = 0; i < simManager->controllers.size(); ++i)
        simManager->controllers[i]->Update(timeStep);
    
    //Update simulation time
    simManager->simulationTime += timeStep;
    
    //Optional method to update some post simulation data (like ROS messages...)
    simManager->SimulationStepCompleted();
}