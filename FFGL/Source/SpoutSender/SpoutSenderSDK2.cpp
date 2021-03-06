﻿/*

	FFGL plugin for sending DirectX texture to a Spout receiver
	With compatible hardware sends texture, otherwise memoryshare

	Original 01.06.13  - first version based on RR-DXGLBridge
	Copyright 2013 Elio <elio@r-revue.de>
	Note fix to FFGL.cpp to allow setting string parameters
	http://resolume.com/forum/viewtopic.php?f=14&t=10324

	Now rewritten with Spout SDK - Version 3

	See also SpoutReceiver.cpp
	
	----------------------------------------------------------------------------------
	24.06.14 - major revision using SpoutSDK - renamed project to SpoutSenderSDK2
	08-07-14 - Version 3.000
	14.07-14 - changed to fixed SpoutSender object
	16.07.14 - restored host fbo binding after writetexture
	25.07.14 - Version 3.001
			 - changed option to true DX9 mode rather than just compatible format
			 - recompiled with latest SDK
	16.08.14 - used DrawToSharedTexture
			 - Version 3.002
	18.08.14 - recompiled for testing and copied to GitHub
	20.08.14 - sender name existence check
			 - activated event locks
			 - Version 3.003
			 - recompiled for testing and copied to GitHub
	================================================
	22.08.14 - Rebuild with MB sendernames class
			 - Version 3.004
	25.08.14 - GitHub update
	29.08.14 - user messages for revised SpoutPanel instead of MessageBox
			 - Version 3.005
	01.09.14 - leak test and some changes made to SpoutGLDXinterop
			 - changed to vertex array draw for DrawToSharedTexture
	02.09.14 - added more information in plugin Description and About
			 - Version 3.006
	21.09.14 - recompiled for DirectX 11 including mutex texture access locks
			 - removed development DX11 parameter
			 - removed OpenGL state save/restore due to receiver problmes. Not needed.
			 - Version 3.007
	30-09-14 - Host fbo argument for DrawToSharedTexture
			 - Version 3.008
	02-10.14 - Removed DirectX option from Help text
			 - Version 3.009
	12.10.14 - Recompile for release
			 - SpoutSDK.cpp - allowed for change of texture size in DrawToSharedTexture
			 - Version 3.010
	21.10.14 - Recompile for update V 2.001 beta
			 - Version 3.011
	23.11.14 - Recompile to remove leftover print statements from SDK
			 - Version 3.012
	16.12.14 - included NvOptimusEnablement export
			 - Version 3.013
	31.01.15 - Changed ID to LJ46/47 instead of OF46/47
			   Included define for DirectX 9 compile
	02.01.15 - Memoryshare SendTexture instead of DrawToSharedTexture
			   Recomplied for DirectX 11, DirectX9 and Memoryshare for 2015 release
			   Version 3.014
	23.02.15 - Removed OptimusEnablement export because it does not work in a plugin
	25.04.15 - Changed from graphics auto detection to set DirectX mode to optional installer
	01.05.15 - Changed project Linker > Debugging > Generate debugging info to YES
			   Version 3.015
	26.05.15 - Recompile for revised SpoutPanel registry write of sender name
			   Version 3.016


*/
#include "SpoutSenderSDK2.h"
#include <FFGL.h>
#include <FFGLLib.h>

// For DirectX 11 mode enable the define below, otherwise compiles for DirectX 9
// 14.02.15 - added auto detection in SpoutGLDXinterop so can leave as DX11 default
// 25.04.15 - changed to optional installation rather than auto-detect
// #define DX9Mode

// For memoryshare, enable the define below
// #define MemoryShareMode

#ifndef MemoryShareMode
	#define FFPARAM_SharingName (0)
	#define FFPARAM_Update      (1)
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Plugin information
////////////////////////////////////////////////////////////////////////////////////////////////////
static CFFGLPluginInfo PluginInfo (

	SpoutSenderSDK2::CreateInstance,		// Create method
	#ifndef MemoryShareMode
	"LJ46",									// Plugin unique ID - LJ note 4 chars only
	"SpoutSender2",							// Plugin name - LJ note 16 chars only ! see freeframe.h
	1,										// API major version number
	005,									// API minor version number - FFGL 1.5
	3,										// Plugin major version number
	016,									// Plugin minor version number
	FF_EFFECT,								// Plugin type
	#ifdef DX9Mode
	"Spout Sender DirectX 9 - Vers 3.016\nSends textures to Spout Receivers\n\nSender Name : enter a sender name\nUpdate : update the name entry", // Plugin description
	#else
	"Spout Sender DirectX 11 - Vers 3.016\nSends textures to Spout Receivers\n\nSender Name : enter a sender name\nUpdate : update the name entry", // Plugin description
	#endif

	#else
	"LJ47",									 // Plugin unique ID - LJ note 4 chars only
	"SpoutSender2M",						 // Plugin name - LJ note 16 chars only ! see freeframe.h
	1,										 // API major version number
	005,									 // API minor version number - FFGL 1.5
	3,										 // Plugin major version number
	015,									 // Plugin minor version number
	FF_EFFECT,								 // Plugin type
	"Spout Memoryshare sender - Vers 3.016", // Plugin description - uses strdup
	#endif
	"S P O U T - Version 2\nspout.zeal.co"		// About
);


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Constructor and destructor
////////////////////////////////////////////////////////////////////////////////////////////////////
SpoutSenderSDK2::SpoutSenderSDK2() : CFreeFrameGLPlugin(), m_initResources(1), m_maxCoordsLocation(-1)
{
	// Input properties
	SetMinInputs(1);
	SetMaxInputs(1);

	/*
	// Debug console window so printf works
	FILE* pCout;
	AllocConsole();
	freopen_s(&pCout, "CONOUT$", "w", stdout); 
	printf("SpoutSender2 Vers 3.016\n");
	*/

	// initial values

	#ifdef MemoryShareMode
	bMemoryMode = true; // Compilation memoryshare
	#else
	bMemoryMode = false;
	#endif

	#ifdef DX9Mode
	bDX9mode          = true; // DirectX 9 rather than default DirectX 11
	#else
	bDX9mode          = false;
	#endif

	m_Width           = 0;
	m_Height          = 0;
	UserSenderName[0] = 0;
	SenderName[0]     = 0;

	// Set parameters if not memoryshare mode
	#ifndef MemoryShareMode
	SetParamInfo(FFPARAM_SharingName,   "Sender Name",      FF_TYPE_TEXT,    "");
	SetParamInfo(FFPARAM_Update,        "Update",           FF_TYPE_EVENT,   false );
	#endif

	// For memory mode, tell Spout to use memoryshare 
	// Default is false or detected according to compatibility
	if(bMemoryMode) {
		sender.SetMemoryShareMode(true);
		// Give it a user name for ProcessOpenGL
		strcpy_s(UserSenderName, 256, "MemoryShare"); 
	}

	// Set DirectX mode depending on DX9 flag
	if(bDX9mode) 
		sender.SetDX9(true);
	else 
	    sender.SetDX9(false);


}


SpoutSenderSDK2::~SpoutSenderSDK2()
{
	// OpenGL context required
	if(wglGetCurrentContext()) {
		// ReleaseSender does nothing if there is no sender
		if(bInitialized) sender.ReleaseSender();
	}

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Methods
////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD SpoutSenderSDK2::InitGL(const FFGLViewportStruct *vp)
{
	// initialize FFGL gl extensions
	m_extensions.Initialize();

	return FF_SUCCESS;
}


DWORD SpoutSenderSDK2::DeInitGL()
{
	// OpenGL context required
	if(wglGetCurrentContext()) {
		if(bInitialized) sender.ReleaseSender();
	}
	bInitialized = false;

	return FF_SUCCESS;
}


DWORD SpoutSenderSDK2::ProcessOpenGL(ProcessOpenGLStruct *pGL)
{
	
	// We need a texture to process
	if (pGL->numInputTextures < 1) return FF_FAIL;
	if (pGL->inputTextures[0] == NULL) return FF_FAIL;
  
	FFGLTextureStruct &InputTexture = *(pGL->inputTextures[0]);

	// get the max s,t that correspond to the width, height
	// of the used portion of the allocated texture space
	FFGLTexCoords maxCoords = GetMaxGLTexCoords(InputTexture);

	// Draw now whether a sender has initialized or not
	DrawTexture(InputTexture.Handle, maxCoords);

	// If there is no sender name yet, the sender cannot be created
	if(!UserSenderName[0]) {
		return FF_SUCCESS; // keep waiting for a name
	}
	
	// Otherwise create a sender if not initialized yet
	else if(!bInitialized) {

		// Update the sender name
		strcpy_s(SenderName, 256, UserSenderName); 

		// Set global width and height so any change can be tested
		m_Width  = (unsigned int)InputTexture.Width;
		m_Height = (unsigned int)InputTexture.Height;
		// Create a new sender
		bInitialized = sender.CreateSender(SenderName, m_Width, m_Height);
		if(!bInitialized) {
			sender.spout.SelectSenderPanel("Could not create sender\nTry another name");
			UserSenderName[0] = 0; // wait for another name to be entered
		}
		return FF_SUCCESS; // give it one frame to initialize
	}
	// Has the texture size or user entered sender name changed
	else if(m_Width  != (unsigned int)InputTexture.Width 
	 || m_Height != (unsigned int)InputTexture.Height
	 || strcmp(SenderName, UserSenderName) != 0 ) {
			// Release existing sender
			sender.ReleaseSender();
			bInitialized = false;
			return FF_SUCCESS; // return for initialization on the next frame
	}

	// Render the Freeframe texture into the shared texture
	// Important - pass the FFGL host FBO to restore the binding because Spout uses a local fbo
	// Default aspect = 1.0, default invert flag = true
	if(bMemoryMode)
		sender.SendTexture(InputTexture.Handle, GL_TEXTURE_2D, m_Width, m_Height);
	else
		sender.DrawToSharedTexture(InputTexture.Handle, GL_TEXTURE_2D,  m_Width, m_Height, (float)maxCoords.s, (float)maxCoords.t, 1.0f, true, pGL->HostFBO);

	return FF_SUCCESS;

}

DWORD SpoutSenderSDK2::GetParameter(DWORD dwIndex)
{
	DWORD dwRet = FF_FAIL;

	#ifndef MemoryShareMode
	switch (dwIndex) {
		case FFPARAM_SharingName:
			if(!bMemoryMode) dwRet = (DWORD)UserSenderName;
			return dwRet;
		default:
			return FF_FAIL;
	}
	#endif

	return FF_FAIL;
}


DWORD SpoutSenderSDK2::SetParameter(const SetParameterStruct* pParam)
{
	HGLRC glContext = wglGetCurrentContext();

	// The parameters will not exist for memoryshare mode
	#ifndef MemoryShareMode
	if (pParam != NULL) {

		switch (pParam->ParameterNumber) {

			case FFPARAM_SharingName:
				if(pParam->NewParameterValue && strlen((char*)pParam->NewParameterValue) > 0) {
					strcpy_s(UserSenderName, (char*)pParam->NewParameterValue);
				}
				else {
					UserSenderName[0] = 0;
				}
				break;

			// Update user entered name
			// Not needed if the host updates the sender name after user entry
			case FFPARAM_Update :
				if (pParam->NewParameterValue) { 
					// Is there any name entered ?
					if(!UserSenderName[0]) {
						sender.spout.SelectSenderPanel("No sender name entered");
					}
					else {
						// Is it different to the current sender name ?
						if(strcmp(SenderName, UserSenderName) != 0) {
							// Create a new sender
							if(bInitialized) sender.ReleaseSender();
							// ProcessOpenGL will pick up the change
							bInitialized = false; 
						}
					}
				}
				break;


			default:
				break;

		}
		return FF_SUCCESS;
	}
	#endif

	return FF_FAIL;
}


void SpoutSenderSDK2::DrawTexture(GLuint TextureHandle, FFGLTexCoords maxCoords)
{

	GLfloat tex_coords[] = {
				0.0,                0.0,
				0.0,                (float)maxCoords.t,
				(float)maxCoords.s, (float)maxCoords.t,
				(float)maxCoords.s, 0.0 };

	GLfloat verts[] =  {
						-1.0, -1.0,
						-1.0,  1.0,
						 1.0,  1.0,
						 1.0, -1.0 };

	glPushMatrix();
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, TextureHandle); // bind the FFGL texture

	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer(2, GL_FLOAT, 0, tex_coords );
	glEnableClientState(GL_VERTEX_ARRAY);		
	glVertexPointer(2, GL_FLOAT, 0, verts );
	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glPopMatrix();

}

