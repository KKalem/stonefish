//
//  OpenGLPrinter.h
//  Stonefish
//
//  Created by Patryk Cieslak on 11/28/12.
//  Copyright (c) 2012 Patryk Cieslak. All rights reserved.
//

#ifndef __Stonefish_OpenGLPrinter__
#define __Stonefish_OpenGLPrinter__

#include "common.h"
#include <FTGL/ftgl.h>
#include <stdint.h>

class OpenGLPrinter
{
public:
    OpenGLPrinter(const char* fontPath, GLint size, GLfloat spacing);
    ~OpenGLPrinter();
    
    void Start();
    void Print(GLfloat* color, GLfloat x, GLfloat y, GLfloat size, const char* text);
    GLfloat TextLength(const char* text);
    void Finish();
    
    static void SetWindowSize(GLint width, GLint height);
    
private:
    FTFont* font;
    GLfloat spacing;
    static GLint windowW, windowH;
};


#endif