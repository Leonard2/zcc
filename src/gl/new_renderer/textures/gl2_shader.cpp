//
//-----------------------------------------------------------------------------
//
// Copyright (C) 2009 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// As an exception to the GPL this code may be used in GZDoom
// derivatives under the following conditions:
//
// 1. The license of these files is not changed
// 2. Full source of the derived program is disclosed
//
//
// ----------------------------------------------------------------------------
//
// Shader handling
//

#include "gl/gl_include.h"
#include "w_wad.h"
#include "i_system.h"
#include "gl/new_renderer/textures/gl2_shader.h"


namespace GLRendererNew
{
int gl_frameMS;

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShaderObject::FShaderObject()
	{
		hShader = hVertProg = hFragProg = NULL;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShaderObject::~FShaderObject()
	{
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	bool FShaderObject::Create(const char *name, const char *vertexshader, const char *fragmentshader)
	{
		hVertProg = gl.CreateShaderObject(GL_VERTEX_SHADER_ARB);
		hFragProg = gl.CreateShaderObject(GL_FRAGMENT_SHADER_ARB);	

		int vp_size = (int)strlen(vertexshader);
		int fp_size = (int)strlen(fragmentshader);

		gl.ShaderSource(hVertProg, 1, &vertexshader, &vp_size);
		gl.ShaderSource(hFragProg, 1, &fragmentshader, &fp_size);

		gl.CompileShader(hVertProg);
		gl.CompileShader(hFragProg);

		hShader = gl.CreateProgramObject();

		gl.AttachObject(hShader, hVertProg);
		gl.AttachObject(hShader, hFragProg);

		gl.BindAttribLocation(hShader, attrLightParams, "aLightParams");
		gl.BindAttribLocation(hShader, attrFogColor, "aFogColor");
		gl.BindAttribLocation(hShader, attrGlowDistance, "aGlowDistance");
		gl.BindAttribLocation(hShader, attrLightParams, "aGlowTopColor");
		gl.BindAttribLocation(hShader, attrLightParams, "aGlowBottomColor");

		gl.LinkProgram(hShader);

		char buffer[10000];
		gl.GetInfoLog(hShader, 10000, NULL, buffer);
		if (*buffer) 
		{
			Printf("Init Shader '%s':\n%s\n", name, buffer);
		}
		int linked;
		gl.GetObjectParameteriv(hShader, GL_OBJECT_LINK_STATUS_ARB, &linked);
		if (!linked) return false;

		mTimer.setIndex(hShader, "timer");
		mDesaturationFactor.setIndex(hShader, "desaturation_factor");
		mFogRadial.setIndex(hShader, "fogradial");
		mTextureMode.setIndex(hShader, "texturemode");
		mCameraPos.setIndex(hShader, "camerapos");
		mColormapColor.setIndex(hShader, "colormapcolor");
	
		gl.UseProgramObject(hShader);
		for(int i=2; i<=8;i++)
		{
			char buf[20];
			mysnprintf(buf, 20, "texture%d", i);
			int texunit_index = gl.GetUniformLocation(hShader, buf);
			if (texunit_index >= 0) gl.Uniform1i(texunit_index, i - 1);
		}
		gl.UseProgramObject(0);

		return !!linked;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	void FShaderObject::Bind()
	{
		gl.UseProgramObject(hShader);
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShader::FShader(const char *shadername)
	{
		mName = shadername;
		mBaseShader = NULL;
		mColormapShader = NULL;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShader::~FShader()
	{
		if (mBaseShader != NULL) delete mBaseShader;
		if (mColormapShader != NULL) delete mColormapShader;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShaderObject *FShader::CreateShader(const char *vp, const char *fp,const char * filename_pixfunc)
	{
		int lumpnum1 = Wads.CheckNumForFullName(vp);
		if (lumpnum1 < 0) return NULL;

		int lumpnum2 = Wads.CheckNumForFullName(fp);
		if (lumpnum2 < 0) return NULL;

		int lumpnum3 = Wads.CheckNumForFullName(filename_pixfunc);
		if (lumpnum3 < 0) return NULL;

		FMemLump lump1 = Wads.ReadLump(lumpnum1);
		FMemLump lump2 = Wads.ReadLump(lumpnum2);
		FMemLump lump3 = Wads.ReadLump(lumpnum3);
		FString fp_combined = lump2.GetString() + lump3.GetString();

		FShaderObject *sh = new FShaderObject;

		if (sh->Create(mName, lump1.GetString(), fp_combined)) return sh;
		delete sh;
		return NULL;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	bool FShader::Create(const char * filename_pixfunc)
	{
		mBaseShader = CreateShader("shaders/VertexShaderMain.vp", "shader/FragmentShaderMain.fp", filename_pixfunc);
		if (!mBaseShader) return false;

		mColormapShader = CreateShader("shaders/VertexShaderColormap.vp", "shader/FragmentShaderColormap.fp", filename_pixfunc);
		if (!mColormapShader)
		{
			delete mBaseShader;
			mBaseShader = NULL;
			return false;
		}
		return true;
	}


	void FShader::Bind(float *cm, int texturemode, float desaturation, float Speed)
	{
		FShaderObject *so;

		if (cm == NULL)
		{
			so = mBaseShader;
			so->setDesaturationFactor(desaturation);
			so->setTextureMode(texturemode);
		}
		else
		{
			so = mColormapShader;
			so->setColormapColor(cm);
		}
		so->setTimer(gl_frameMS*Speed/1000.f);
		mOwner->SetActiveShader(so);
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShaderContainer::~FShaderContainer()
	{
		for(unsigned i = 0; i < mShaders.Size(); i++)
		{
			delete mShaders[i];
		}
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	void FShaderContainer::AddShader(FShader *shader)
	{
		for(unsigned i = 0; i < mShaders.Size(); i++)
		{
			if (mShaders[i]->GetName() > shader->GetName())
			{
				mShaders.Insert(i, shader);
				return;
			}
			mShaders.Push(shader);
			shader->mOwner = this;
		}
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	bool FShaderContainer::CreateDefaultShaders()
	{
		const char *shaderdefs[] = {
			"Default", "shaders/ShaderFunc_Default.fpi",
			"Warp1", "shaders/ShaderFunc_Warp1.fpi",
			"Warp2", "shaders/ShaderFunc_Warp2.fpi",
			"Brightmap", "shaders/ShaderFunc_Brightmap.fpi",
			"AlphaShade", "shaders/ShaderFunc_AlphaShade.fpi",
			"Intensity", "shaders/ShaderFunc_Intensity.fpi",
			NULL, NULL};

		for(int i=0;shaderdefs[i]; i+=2)
		{
			FShader * shader = new FShader(shaderdefs[i]);
			if (!shader->Create(shaderdefs[i+1]))
			{
				delete shader;
				if (i == 0) I_FatalError("Unable to create default shader");
			}
			AddShader(shader);
		}
		return true;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	FShader *FShaderContainer::FindShader(const char *name)
	{
		FName cmp = name;
		int min = 0;
		int max = mShaders.Size()-1;

		while (min < max)
		{
			int mid = (min+max) >> 1;

			if (mShaders[mid]->GetName() == cmp) return mShaders[mid];
			else if (mShaders[mid]->GetName() > cmp)
			{
				max = mid - 1;
			}
			else
			{
				min = mid + 1;
			}
		}
		return NULL;
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	void FShaderContainer::SetActiveShader(FShaderObject *active)
	{
		if (mActiveShader != active)
		{
			mActiveShader = active;
			if (active != NULL) 
			{
				active->setFogType(mFogType);
				active->setCameraPos(mCameraPos);
				active->Bind();
			}
			else
			{
				gl.UseProgramObject(NULL);
			}
		}
	}

}