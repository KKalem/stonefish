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
//  OpenGLPipeline.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 30/03/2014.
//  Copyright(c) 2014-2019 Patryk Cieslak. All rights reserved.
//

#include "graphics/OpenGLPipeline.h"

#include "core/SimulationManager.h"
#include "graphics/OpenGLState.h"
#include "graphics/GLSLShader.h"
#include "graphics/OpenGLContent.h"
#include "graphics/OpenGLOcean.h"
#include "graphics/OpenGLCamera.h"
#include "graphics/OpenGLRealCamera.h"
#include "graphics/OpenGLDepthCamera.h"
#include "graphics/OpenGLFLS.h"
#include "graphics/OpenGLAtmosphere.h"
#include "graphics/OpenGLLight.h"
#include "graphics/OpenGLOceanParticles.h"
#include "core/Console.h"
#include "utils/SystemUtil.hpp"
#include "entities/forcefields/Ocean.h"
#include "entities/forcefields/Atmosphere.h"

namespace sf
{

OpenGLPipeline::OpenGLPipeline(RenderSettings s, HelperSettings h) : rSettings(s), hSettings(h)
{
    drawingQueueMutex = SDL_CreateMutex();
    
    //Set default OpenGL options
    cInfo("Initialising OpenGL rendering pipeline...");
    
    if(s.msaa) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE);
    
    //Load shaders and create rendering buffers
    cInfo("Loading shaders...");
    OpenGLAtmosphere::BuildAtmosphereAPI(rSettings.atmosphere);
    OpenGLCamera::Init();
    OpenGLDepthCamera::Init();
    OpenGLFLS::Init();
    OpenGLOceanParticles::Init();
    content = new OpenGLContent();
    
    //Create display framebuffer
    glGenFramebuffers(1, &screenFBO);
    OpenGLState::BindFramebuffer(screenFBO);
    
    glGenTextures(1, &screenTex);
    OpenGLState::BindTexture(TEX_BASE, GL_TEXTURE_2D, screenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rSettings.windowW, rSettings.windowH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //Cheaper/better gaussian blur for GUI background
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //Cheaper/better gaussian blur for GUI background
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTex, 0);
    OpenGLState::UnbindTexture(TEX_BASE);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
        cError("Display FBO initialization failed!");
    
    OpenGLState::BindFramebuffer(0);
    
    lastSimTime = Scalar(0);
}

OpenGLPipeline::~OpenGLPipeline()
{
    OpenGLCamera::Destroy();
    OpenGLDepthCamera::Destroy();
    OpenGLOceanParticles::Destroy();
    OpenGLLight::Destroy();
    delete content;
    
    glDeleteTextures(1, &screenTex);
    glDeleteFramebuffers(1, &screenFBO);
    SDL_DestroyMutex(drawingQueueMutex);
}

RenderSettings OpenGLPipeline::getRenderSettings() const
{
    return rSettings;
}
    
HelperSettings& OpenGLPipeline::getHelperSettings()
{
    return hSettings;
}
    
GLuint OpenGLPipeline::getScreenTexture()
{
    return screenTex;
}

SDL_mutex* OpenGLPipeline::getDrawingQueueMutex()
{
    return drawingQueueMutex;
}
    
OpenGLContent* OpenGLPipeline::getContent()
{
    return content;
}

void OpenGLPipeline::AddToDrawingQueue(const Renderable& r)
{
    drawingQueue.push_back(r);
}

void OpenGLPipeline::AddToDrawingQueue(const std::vector<Renderable>& r)
{
    drawingQueue.insert(drawingQueue.end(), r.begin(), r.end());
}

bool OpenGLPipeline::isDrawingQueueEmpty()
{
    return drawingQueue.empty();
}
    
void OpenGLPipeline::PerformDrawingQueueCopy()
{
    if(!drawingQueue.empty())
    {
        drawingQueueCopy.clear();
        SDL_LockMutex(drawingQueueMutex);
        drawingQueueCopy.insert(drawingQueueCopy.end(), drawingQueue.begin(), drawingQueue.end());
        //Update camera transforms to ensure consistency
        for(unsigned int i=0; i < content->getViewsCount(); ++i)
            if(content->getView(i)->getType() == ViewType::CAMERA)
                ((OpenGLRealCamera*)content->getView(i))->UpdateTransform();
            else if(content->getView(i)->getType() == ViewType::DEPTH_CAMERA)
                ((OpenGLDepthCamera*)content->getView(i))->UpdateTransform();
            else if(content->getView(i)->getType() == ViewType::SONAR)
                ((OpenGLFLS*)content->getView(i))->UpdateTransform();
        //Update light transforms to ensure consistency
        for(unsigned int i=0; i < content->getLightsCount(); ++i)
            content->getLight(i)->UpdateTransform();
        drawingQueue.clear(); //Enable update of drawing queue by clearing old queue
        SDL_UnlockMutex(drawingQueueMutex);
    }
}

void OpenGLPipeline::DrawDisplay()
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, screenFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, rSettings.windowW, rSettings.windowH, 0, 0, rSettings.windowW, rSettings.windowH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void OpenGLPipeline::DrawObjects()
{
    for(size_t i=0; i<drawingQueueCopy.size(); ++i)
    {
        if(drawingQueueCopy[i].type == RenderableType::SOLID)
            content->DrawObject(drawingQueueCopy[i].objectId, drawingQueueCopy[i].lookId, drawingQueueCopy[i].model);
    }
}

void OpenGLPipeline::DrawLights()
{
	content->SetDrawingMode(DrawingMode::RAW);
	for(unsigned int i=0; i<content->getLightsCount(); ++i)
		content->getLight(i)->DrawLight();
}
    
void OpenGLPipeline::DrawHelpers()
{
    //Coordinate systems
    if(hSettings.showCoordSys)
    {
        content->DrawCoordSystem(glm::mat4(), 1.f);
        
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::SOLID_CS)
                content->DrawCoordSystem(drawingQueueCopy[h].model, 0.5f);
        }
    }
    
    //Discrete and multibody joints
    if(hSettings.showJoints)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::MULTIBODY_AXIS)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.5f,1.f,1.f), drawingQueueCopy[h].model);
            else if(drawingQueueCopy[h].type == RenderableType::JOINT_LINES)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.5f,1.f,1.f), drawingQueueCopy[h].model);
        }
    }
    
    //Sensors
    if(hSettings.showSensors)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::SENSOR_CS)
                content->DrawCoordSystem(drawingQueueCopy[h].model, 0.25f);
            else if(drawingQueueCopy[h].type == RenderableType::SENSOR_POINTS)
                content->DrawPrimitives(PrimitiveType::POINTS, drawingQueueCopy[h].points, glm::vec4(1.f,1.f,0,1.f), drawingQueueCopy[h].model);
            else if(drawingQueueCopy[h].type == RenderableType::SENSOR_LINES)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,1.f,0,1.f), drawingQueueCopy[h].model);
            else if(drawingQueueCopy[h].type == RenderableType::SENSOR_LINE_STRIP)
                content->DrawPrimitives(PrimitiveType::LINE_STRIP, drawingQueueCopy[h].points, glm::vec4(1.f,1.f,0,1.f), drawingQueueCopy[h].model);
        }
    }
    
    //Actuators
    if(hSettings.showActuators)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::ACTUATOR_LINES)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.5f,0,1.f), drawingQueueCopy[h].model);
        }
    }
    
    //Fluid dynamics
    if(hSettings.showFluidDynamics)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            switch(drawingQueueCopy[h].type)
            {
                case RenderableType::HYDRO_CS:
                    content->DrawEllipsoid(drawingQueueCopy[h].model, glm::vec3(0.005f), glm::vec4(0.3f, 0.7f, 1.f, 1.f));
                    break;
                    
                case RenderableType::HYDRO_CYLINDER:
                    content->DrawCylinder(drawingQueueCopy[h].model, drawingQueueCopy[h].points[0], glm::vec4(0.2f, 0.5f, 1.f, 1.f));
                    break;
                    
                case RenderableType::HYDRO_ELLIPSOID:
                    content->DrawEllipsoid(drawingQueueCopy[h].model, drawingQueueCopy[h].points[0], glm::vec4(0.2f, 0.5f, 1.f, 1.f));
                    break;
                    
                case RenderableType::HYDRO_POINTS:
                    content->DrawPrimitives(PrimitiveType::POINTS, drawingQueueCopy[h].points, glm::vec4(0.3f, 0.7f, 1.f, 1.f), drawingQueueCopy[h].model);
                    break;
                    
                case RenderableType::HYDRO_LINES:
                    content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(0.2f, 0.5f, 1.f, 1.f), drawingQueueCopy[h].model);
                    break;
                    
                case RenderableType::HYDRO_LINE_STRIP:
                    content->DrawPrimitives(PrimitiveType::LINE_STRIP, drawingQueueCopy[h].points, glm::vec4(0.2f, 0.5f, 1.f, 1.f), drawingQueueCopy[h].model);
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    //Forces
    if(hSettings.showForces)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            switch(drawingQueueCopy[h].type)
            {
                case RenderableType::FORCE_BUOYANCY:
                    content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(0.f,0.f,1.f,1.f), drawingQueueCopy[h].model);
                    break;
        
                case RenderableType::FORCE_LINEAR_DRAG:
                    content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(0.f,1.f,1.f,1.f), drawingQueueCopy[h].model);
                    break;
                    
                case RenderableType::FORCE_QUADRATIC_DRAG:
                    content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.f,1.f,1.f), drawingQueueCopy[h].model);
                    break;
        
                default:
                    break;
            }
        }
    }
}

void OpenGLPipeline::Render(SimulationManager* sim)
{	
    //Update time step for animation purposes
    Scalar now = sim->getSimulationTime();
    Scalar dt = now-lastSimTime;
    lastSimTime = now;

    //Double-buffering of drawing queue
    PerformDrawingQueueCopy();
    
    //Choose rendering mode
    unsigned int renderMode = 0; //Defaults to rendering without ocean
    Ocean* ocean = sim->getOcean();
    if(ocean != NULL)
    {
        ocean->getOpenGLOcean()->Simulate(dt);
        renderMode = rSettings.ocean > RenderQuality::QUALITY_DISABLED && ocean->isRenderable() ? 1 : 0;
    }
    Atmosphere* atm = sim->getAtmosphere();
    OpenGLState::EnableDepthTest();
    OpenGLState::EnableCullFace();
    
    //Bake shadow maps for lights (independent of view)
    if(rSettings.shadows > RenderQuality::QUALITY_DISABLED)
    {
        glCullFace(GL_FRONT);
        content->SetDrawingMode(DrawingMode::FLAT);
        for(unsigned int i=0; i<content->getLightsCount(); ++i)
            content->getLight(i)->BakeShadowmap(this);
        glCullFace(GL_BACK);
    }
    
    //Clear display framebuffer
    OpenGLState::BindFramebuffer(screenFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    
    //Loop through all views -> trackballs, cameras, depth cameras...
    for(unsigned int i=0; i<content->getViewsCount(); ++i)
    {
        OpenGLView* view = content->getView(i);
        
        if(view->needsUpdate())
        {
            if(view->getType() == DEPTH_CAMERA)
            {
                OpenGLDepthCamera* camera = (OpenGLDepthCamera*)view;
                GLint* viewport = camera->GetViewport();
                content->SetViewportSize(viewport[2],viewport[3]);
            
                OpenGLState::BindFramebuffer(camera->getRenderFBO());
                glClear(GL_DEPTH_BUFFER_BIT); //Only depth is rendered
                camera->SetViewport();
                content->SetCurrentView(camera);
                content->SetDrawingMode(DrawingMode::FLAT);
                DrawObjects();
                
                camera->DrawLDR(screenFBO);
                OpenGLState::BindFramebuffer(0);
                
                delete [] viewport;
            }
            else if(view->getType() == SONAR)
            {
                OpenGLFLS* fls = (OpenGLFLS*)view;
                
                //Draw object and compute sonar data
                fls->ComputeOutput(drawingQueueCopy);
                
                //Draw sonar output
                GLint* viewport = fls->GetViewport();
                content->SetViewportSize(viewport[2], viewport[3]);
                fls->DrawLDR(screenFBO);
                delete [] viewport;
            }
            else if(view->getType() == CAMERA || view->getType() == TRACKBALL)
            {
                //Apply view properties
                OpenGLCamera* camera = (OpenGLCamera*)view;
                OpenGLLight::SetCamera(camera);
                GLint* viewport = camera->GetViewport();
                content->SetViewportSize(viewport[2],viewport[3]);
            
                //Bake parallel-split shadowmaps for sun
                if(rSettings.shadows > RenderQuality::QUALITY_DISABLED)
                {
                    content->SetDrawingMode(DrawingMode::FLAT);
                    atm->getOpenGLAtmosphere()->BakeShadowmaps(this, camera);
                }
            
                //Clear main framebuffer and setup camera
                OpenGLState::BindFramebuffer(camera->getRenderFBO());
                GLenum renderBuffs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2, renderBuffs);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                camera->SetViewport();
                content->SetCurrentView(camera);
                
                //Draw scene
                if(renderMode == 0) //NO OCEAN
                {
                    //Render all objects
                    content->SetDrawingMode(DrawingMode::FULL);
                    DrawObjects();
					
                    //Ambient occlusion
                    if(rSettings.ao > RenderQuality::QUALITY_DISABLED)
                        camera->DrawAO(1.0f);
            
					//Draw lights
					DrawLights();
					
                    //Render sky (at the end to take profit of early bailing)
                    atm->getOpenGLAtmosphere()->DrawSkyAndSun(camera);			
                    
                    //Go to postprocessing stage
                    camera->EnterPostprocessing();
                }
                else if(renderMode == 1) //OCEAN
                {
                    OpenGLOcean* glOcean = ocean->getOpenGLOcean();
                    
                    //Update ocean for this camera
                    if(ocean->hasWaves())
                        glOcean->UpdateSurface(camera);
                    
                    //Generating stencil mask
                    glOcean->DrawUnderwaterMask(camera);
                    
                    //Drawing underwater without stencil test
                    content->SetDrawingMode(DrawingMode::UNDERWATER);
                    DrawObjects();
                    glOcean->DrawBackground(camera);
					
					//Draw lights
					DrawLights();
					
                    //a) Surface with waves
                    if(ocean->hasWaves())
                    {
                        OpenGLState::EnableBlend();
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glOcean->DrawSurface(camera);
                        OpenGLState::DisableBlend();
                        glOcean->DrawBacksurface(camera);
                    }
                    else //b) Surface without waves (disable depth testing but write to depth buffer)
                    {
                        OpenGLState::EnableBlend();
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthFunc(GL_ALWAYS);
                        glOcean->DrawSurface(camera);
                        OpenGLState::DisableBlend();
                        glDepthFunc(GL_LESS);
                        glOcean->DrawBacksurface(camera);
                        glDepthFunc(GL_LEQUAL);
                    }
                    
                    //Stencil masking
                    OpenGLState::EnableStencilTest();
                    glStencilMask(0x00);
                    glStencilFunc(GL_EQUAL, 0, 0xFF);
                    
                    //Draw all objects as above surface (depth testing will secure drawing only what is above water)
                    content->SetDrawingMode(DrawingMode::FULL);
                    DrawObjects();
                    
					//Draw lights
					DrawLights();
					
                    //Render sky (left for the end to only fill empty spaces)
                    atm->getOpenGLAtmosphere()->DrawSkyAndSun(camera);
                    
                    OpenGLState::DisableStencilTest();
                    
                    //Linear depth front faces
                    camera->GenerateLinearDepth(0, true);
                    
                    //Linear depth back faces
                    OpenGLState::BindFramebuffer(camera->getRenderFBO());
                    glClear(GL_DEPTH_BUFFER_BIT);
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glCullFace(GL_FRONT);
                    content->SetDrawingMode(DrawingMode::FLAT);
                    DrawObjects();
                    glCullFace(GL_BACK);
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    camera->GenerateLinearDepth(0, false);
                    
                    //Go to postprocessing stage
                    camera->EnterPostprocessing();
                    
                    //Draw reflections
                    OpenGLState::DisableDepthTest();
                    camera->DrawSSR();
                    
                    //Draw blur only below surface
                    OpenGLState::EnableStencilTest();
                    glStencilFunc(GL_EQUAL, 1, 0xFF);
                    glOcean->DrawVolume(camera, camera->getPostprocessTexture(0), camera->getLinearDepthTexture(true));
                    glOcean->DrawParticles(camera);
                    OpenGLState::DisableStencilTest();
                }
            
                //Tone mapping
                OpenGLState::Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                camera->DrawLDR(screenFBO);
            
                //Helper objects
                if(camera->getType() == ViewType::TRACKBALL)
                {
                    //Overlay debugging info
                    OpenGLState::BindFramebuffer(screenFBO); //No depth buffer, just one color buffer
                    content->SetProjectionMatrix(camera->GetProjectionMatrix());
                    content->SetViewMatrix(camera->GetViewMatrix());
                    OpenGLState::DisableCullFace();
                    
                    //Simulation debugging
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    //if(sim->getSolidDisplayMode() == DisplayMode::DISPLAY_PHYSICAL) DrawObjects();
                    DrawHelpers();
                    if(hSettings.showBulletDebugInfo) sim->RenderBulletDebug();
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    
                    //Graphics debugging
                    /*if(ocean != NULL)
                    {
                        //ocean->getOpenGLOcean().ShowOceanSpectrum(glm::vec2((GLfloat)viewport[2], (GLfloat)viewport[3]), glm::vec4(0,200,300,300));
                        //ocean->getOpenGLOcean()->ShowTexture(4, glm::vec4(0,0,256,256));
                    }*/
            
                    /*atm->getOpenGLAtmosphere()->ShowAtmosphereTexture(AtmosphereTextures::TRANSMITTANCE,glm::vec4(0,0,200,200));
                    atm->getOpenGLAtmosphere()->ShowAtmosphereTexture(AtmosphereTextures::IRRADIANCE,glm::vec4(200,0,200,200));
                    atm->getOpenGLAtmosphere()->ShowAtmosphereTexture(AtmosphereTextures::SCATTERING,glm::vec4(400,0,200,200));
                    atm->getOpenGLAtmosphere()->ShowSunShadowmaps(0, 0, 0.1f);*/
                    
                    //camera->ShowSceneTexture(SceneComponent::NORMAL, glm::vec4(0,0,300,200));
                    //camera->ShowViewNormalTexture(glm::vec4(0,200,300,200));
                    //camera->ShowLinearDepthTexture(glm::vec4(0,0,300,200), true);
                    //camera->ShowLinearDepthTexture(glm::vec4(0,400,300,200), false);
                    
                    //camera->ShowDeinterleavedDepthTexture(glm::vec4(0,400,300,200), 0);
                    //camera->ShowDeinterleavedDepthTexture(glm::vec4(0,400,300,200), 8);
                    //camera->ShowDeinterleavedDepthTexture(glm::vec4(0,600,300,200), 9);
                    //camera->ShowDeinterleavedAOTexture(glm::vec4(0,600,300,200), 0);
                    //camera->ShowAmbientOcclusion(glm::vec4(0,800,300,200));
                    
                    OpenGLState::BindFramebuffer(0);
                }
            
                delete [] viewport;
            }
            
            OpenGLState::EnableDepthTest();
            OpenGLState::EnableCullFace();
            OpenGLState::DisableBlend();
        }
        else
        {
            GLint* viewport = view->GetViewport();
            OpenGLState::Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            view->DrawLDR(screenFBO);
            delete [] viewport;
        }
    }
}

}
