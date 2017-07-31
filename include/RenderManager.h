#pragma once

#include "Error.h"
#include "entity/component/Model.h"
#include "entity/component/Transform.h"
#include "Types.h"

#include <citro3d.h>
#include <3ds.h>

//Shaders
#include "v_default_shbin.h"

#ifdef __INTELLISENSE__
// My intellisense keeps forgetting C3D_RT exists because reasons...
#define C3D_RenderTarget C3D_RenderTarget_tag
#endif



class RenderManager
{
public:

	/* Returns a reference to the RenderManager instance */
	static RenderManager& instance();

	/* Returns a pointer to an initialized rendertarget */
	C3D_RenderTarget* getRenderTarget(RenderTarget target);
	
	/* Begin the render frame */
	void beginFrame();

	/* End it */
	void endFrame();

	/* Call once a layer has finished being rendered (ie, background, world, gui, etc..) */
	void finishRenderLayer();

	/* Renders a model with the default shader*/
	void renderModel(Model model, Transform transform, RenderTarget target);


private:
	RenderManager();
	~RenderManager();

	static RenderManager instance_;

	shaderProgram_s defaultShaderProgram_;
	C3D_RenderTarget* targetTopLeft_, *targetTopRight_, *targetBottom_;
	
	int clearColor_ = 0x0000FFFF;

	int renderFlags_ =
		(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

	const float separationMultiplier_ = 0.3;
};
