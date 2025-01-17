#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"

#include <iostream>

namespace our
{

	void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
	{
		// First, we store the window size for later use
		this->windowSize = windowSize;

		// Then we check if there is a sky texture in the configuration
		if (config.contains("sky"))
		{
			// First, we create a sphere which will be used to draw the sky
			this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

			// We can draw the sky using the same shader used to draw textured objects
			ShaderProgram *skyShader = new ShaderProgram();
			skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
			skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
			skyShader->link();

			// TODO: (Req 10) Pick the correct pipeline state to draw the sky
			//  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth function should we pick?
			//  We will draw the sphere from the inside, so what options should we pick for the face culling.
			/*
					firstly we need to define a pipeline just to set if we need faceCulling, depthTesting or Blending
					-> we need to enable depth testing, and set the function to GL_LEQUAL as the clouds are drawn behind the scenes
					-> we need to enable faceCulling, and to select the front face to be removed
			*/
			PipelineState skyPipelineState{};
			skyPipelineState.depthTesting.enabled = true;
			skyPipelineState.depthTesting.function = GL_LEQUAL;
			// GL_LEQUAL is the best since most cloud are drawn behind the scene
			skyPipelineState.faceCulling.enabled = true;
			// we want to remove the front face of the sky
			skyPipelineState.faceCulling.culledFace = GL_FRONT;

			// Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
			std::string skyTextureFile = config.value<std::string>("sky", "");
			Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

			// Setup a sampler for the sky
			Sampler *skySampler = new Sampler();
			skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
			skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Combine all the aforementioned objects (except the mesh) into a material
			this->skyMaterial = new TexturedMaterial();
			this->skyMaterial->shader = skyShader;
			this->skyMaterial->texture = skyTexture;
			this->skyMaterial->sampler = skySampler;
			this->skyMaterial->pipelineState = skyPipelineState;
			this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			this->skyMaterial->alphaThreshold = 1.0f;
			this->skyMaterial->transparent = false;
		}

		// Then we check if there is a postprocessing shader in the configuration
		if (config.contains("postprocess"))
		{
			if (config.contains("energyPostProcess"))
			{
				this->boostPath = config.value<std::string>("energyPostProcess", "");
			}
			this->crashingPath = config.value<std::string>("postprocess", "");
			// TODO: (Req 11) Create a framebuffer
			//  Generating FrameBuffer
			glGenFramebuffers(1, &postprocessFrameBuffer);
			// Binding the postprocessFrameBuffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);
			// TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
			//  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
			//  The depth format can be (Depth component with 24 bits).
			//  As we send a window size then we get the maximum coordinate of them tp generate number of needed levels, if the case was color
			colorTarget = texture_utils::empty(GL_RGBA8, windowSize);
			depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);
			/*
			Attaching the frame buffer with the color and depth
			Just like to specify to the frame buffer which texture to be drawn
			We choose GL_COLOR_ATTACHEMENT0 as we will draw on a single texture, as there are other options to draw on different textures
			*/
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

			// TODO: (Req 11) Unbind the framebuffer just to be safe
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			// Create a vertex array to use for drawing the texture
			glGenVertexArrays(1, &postProcessVertexArray);

			// Create a sampler to use for sampling the scene texture in the post processing shader
			Sampler *postprocessSampler = new Sampler();
			postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Create the post processing shader
			ShaderProgram *postprocessShader = new ShaderProgram();
			postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
			postprocessShader->attach(this->crashingPath, GL_FRAGMENT_SHADER);
			postprocessShader->link();

			// Create a post processing material
			postprocessMaterial = new TexturedMaterial();
			postprocessMaterial->shader = postprocessShader;
			postprocessMaterial->texture = colorTarget;
			postprocessMaterial->sampler = postprocessSampler;
			// The default options are fine but we don't need to interact with the depth buffer
			// so it is more performant to disable the depth mask
			postprocessMaterial->pipelineState.depthMask = false;
		}
		// Then we check if there is a postprocessing shader in the configuration
	}

	void ForwardRenderer::destroy()
	{
		// Delete all objects related to the sky
		if (skyMaterial)
		{
			delete skySphere;
			delete skyMaterial->shader;
			delete skyMaterial->texture;
			delete skyMaterial->sampler;
			delete skyMaterial;
		}
		// Delete all objects related to post processing
		if (postprocessMaterial)
		{
			glDeleteFramebuffers(1, &postprocessFrameBuffer);
			glDeleteVertexArrays(1, &postProcessVertexArray);
			delete colorTarget;
			delete depthTarget;
			delete postprocessMaterial->sampler;
			delete postprocessMaterial->shader;
			delete postprocessMaterial;
		}
	}

	void ForwardRenderer::render(World *world)
	{
		// First of all, we search for a camera and for all the mesh renderers
		CameraComponent *camera = nullptr;
		opaqueCommands.clear();
		transparentCommands.clear();
		lights.clear();

		for (auto entity : world->getEntities())
		{
			// If we hadn't found a camera yet, we look for a camera in this entity
			if (!camera)
				camera = entity->getComponent<CameraComponent>();
			// If this entity has a mesh renderer component
			if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
			{
				// We construct a command from it
				RenderCommand command;
				command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
				command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
				command.mesh = meshRenderer->mesh;
				command.material = meshRenderer->material;
				// if it is transparent, we add it to the transparent commands list
				if (command.material->transparent)
				{
					transparentCommands.push_back(command);
				}
				else
				{
					// Otherwise, we add it to the opaque command list
					opaqueCommands.push_back(command);
				}
			}
			// If the entity has a light component, we add it to the lights list
			if (auto light = entity->getComponent<LightComponent>(); light)
			{
				lights.push_back(light);
			}
		}

		// If there is no camera, we return (we cannot render without a camera)
		if (camera == nullptr)
			return;

		// TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
		// HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one

		// This is the camera's forward direction in its local space & as we know, the camera points at the negative z.
		glm::vec4 forward = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

		// Transform the camera forward position to the world space from the local space using the view matrix
		glm::vec4 cameraForward = camera->getViewMatrix() * forward;

		// Sort the transparent commands with the custom comparable function as explained below
		std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward](const RenderCommand &first, const RenderCommand &second)
							{
            // TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second".

            // Get the "first" distance by applying dot product between the center of the "first" after
            // converting it to a 4D vector and after trasforming it to the world space, with the camera forward position
            double firstDistance = glm::dot(first.localToWorld * glm::vec4(first.center, 1), cameraForward);
            // Do the same for the "second" distance
            double secondDistance =  glm::dot(second.localToWorld * glm::vec4(second.center, 1), cameraForward);

            // If the "first" distance is greater than the "second" distance, return true, otherwise return false
            if(firstDistance > secondDistance)
                return true;
            return false; });

		// TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
		// Multiply the camera projection matrix after passing the window size with the view matrix
		glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();

		// TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
		// Use the OpenGL glViewport & pass 0,0 as out starting x & y then extract the total length of x & y from
		// the window size
		glViewport(0, 0, windowSize.x, windowSize.y);

		// TODO: (Req 9) Set the clear color to black and the clear depth to 1
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClearDepth(1.0);

		// TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
		glColorMask(true, true, true, true);
		glDepthMask(true);
		// If there is a postprocess material, bind the framebuffer
		if (postprocessMaterial && (this->crashingEffect || this->boostingEffect) )
		{
			// TODO: (Req 11) bind the framebuffer
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);
		}

		// TODO: (Req 9) Clear the color and depth buffers
		// Use glClear to clear both bits
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int numLights = lights.size();

		// TODO: (Req 9) Draw all the opaque commands
		// Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
		// Loop over all opaque commands & apply the following steps
		for (auto opaqueCommand : opaqueCommands)
		{
			// Setup the material of each opaque command
			opaqueCommand.material->setup();

			if (auto lightMaterial = dynamic_cast<LightMaterial *>(opaqueCommand.material); lightMaterial)
			{
				opaqueCommand.material->shader->set("VP", VP);
				opaqueCommand.material->shader->set("eye", glm::vec3(cameraForward.x, cameraForward.y, cameraForward.z));
				opaqueCommand.material->shader->set("light_count", numLights);
				opaqueCommand.material->shader->set("M", opaqueCommand.localToWorld);
				opaqueCommand.material->shader->set("M_IT", glm::transpose(glm::inverse(opaqueCommand.localToWorld)));

				opaqueCommand.material->shader->set("sky.top", glm::vec3(0.0f, 0.1f, 0.5f));
				opaqueCommand.material->shader->set("sky.horizon", glm::vec3(0.3f, 0.3f, 0.3f));
				opaqueCommand.material->shader->set("sky.bottom", glm::vec3(0.1f, 0.1f, 0.1f));

				for (int i = 0; i < numLights; i++)
				{
					std::string prefix = "lights[" + std::to_string(i) + "].";
					glm::vec3 position = lights[i]->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
					glm::vec3 direction = lights[i]->getOwner()->getLocalToWorldMatrix() * glm::vec4(lights[i]->direction, 0.0f);
					opaqueCommand.material->shader->set(prefix + "position", glm::vec3(position.x, position.y, position.z));
					opaqueCommand.material->shader->set(prefix + "direction", glm::normalize(glm::vec3(direction.x, direction.y, direction.z)));
					opaqueCommand.material->shader->set(prefix + "type", (int)lights[i]->type);

					opaqueCommand.material->shader->set(prefix + "diffuse", lights[i]->diffuse);
					opaqueCommand.material->shader->set(prefix + "specular", lights[i]->specular);
					opaqueCommand.material->shader->set(prefix + "attenuation", lights[i]->attenuation);
					opaqueCommand.material->shader->set(prefix + "cone_angles", lights[i]->cone_angles);
				}
			}
			else
			{
				// Set the "transform" uniform as the VP matrix we obtained above & transform it to world space
				opaqueCommand.material->shader->set("transform", VP * opaqueCommand.localToWorld);
			}

			// Draw Mesh
			opaqueCommand.mesh->draw();
		}

		// If there is a sky material, draw the sky
		if (this->skyMaterial)
		{
			// TODO: (Req 10) setup the sky material
			skyMaterial->setup();

			// TODO: (Req 10) Get the camera position
			// Getting the Model Matrix to transform from local coordinates to world coordinates
			glm::mat4 M = camera->getOwner()->getLocalToWorldMatrix();
			// Getting Camera position by selecting the last column of the Model matrix
			glm::vec3 cameraPosition = M * glm::vec4(0, 0, 0, 1);

			// TODO: (Req 10) Create a model matrix for the sky such that it always follows the camera (sky sphere center = camera position)
			// We need to translate the sky sphere center to the camera position
			// Set the sky sphere center as a 4x4 matrix of ones
			glm::mat4 skySphereCenter = glm::mat4(1.0f);
			// Translate this center to the camera position such that it equals it & store
			// the resulting transformed matrix in modelMatrix
			glm::mat4 modelMatrix = glm::translate(skySphereCenter, cameraPosition);

			// TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
			// We can acheive this by multiplying by an extra matrix after the projection but what values should we put in it?
			// For the sky to be drawn behind, we need to modify the z-index. How?
			// Simply the 3rd column of our generated alwaysBehindTransform matrix will set the z-coordinate as w.
			// Then after normalization, when the values of the matrix are divided by w, we will obtain the z which was
			// previously set to w as 1. Our goal is achieved.
			// This will ensure that the sky is the very first thing drawn behind
			// and set the 4th column to 0,0,0,1 to keep the w-coordinate unchanged
			glm::mat4 alwaysBehindTransform = glm::mat4(
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 1.0f);
			// TODO: (Req 10) set the "transform" uniform
			// We need to get the view Matrix and Projection Matrix
			// Following TRS sequence of matrices, we will get the VP matrix as we did before from the projection and view matrices
			// and multiply it by the model matrix to get the transformation matrix
			// Finally we set it to the "transform" uniform variable in the shader
			glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();
			glm::mat4 transformationMatrix = alwaysBehindTransform * VP * modelMatrix;
			skyMaterial->shader->set("transform", transformationMatrix);

			// TODO: (Req 10) draw the sky sphere
			skySphere->draw();
		}

		// TODO: (Req 9) Draw all the transparent commands
		// Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
		// Loop over all transparent commands
		for (auto transparentCommand : transparentCommands)
		{
			// Call the setup function in the material of each transparent command
			transparentCommand.material->setup();

			if (auto lightMaterial = dynamic_cast<LightMaterial *>(transparentCommand.material); lightMaterial)
			{
				transparentCommand.material->shader->set("VP", VP);
				transparentCommand.material->shader->set("eye", glm::vec3(cameraForward.x, cameraForward.y, cameraForward.z));
				transparentCommand.material->shader->set("light_count", numLights);
				transparentCommand.material->shader->set("M", transparentCommand.localToWorld);
				transparentCommand.material->shader->set("M_IT", glm::transpose(glm::inverse(transparentCommand.localToWorld)));

				transparentCommand.material->shader->set("sky.top", glm::vec3(0.0f, 0.1f, 0.5f));
				transparentCommand.material->shader->set("sky.horizon", glm::vec3(0.3f, 0.3f, 0.3f));
				transparentCommand.material->shader->set("sky.bottom", glm::vec3(0.1f, 0.1f, 0.1f));

				for (int i = 0; i < numLights; i++)
				{
					std::string prefix = "lights[" + std::to_string(i) + "].";
					glm::vec3 position = lights[i]->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
					glm::vec3 direction = lights[i]->getOwner()->getLocalToWorldMatrix() * glm::vec4(lights[i]->direction, 0.0f);
					transparentCommand.material->shader->set(prefix + "position", glm::vec3(position.x, position.y, position.z));
					transparentCommand.material->shader->set(prefix + "direction", glm::normalize(glm::vec3(direction.x, direction.y, direction.z)));
					transparentCommand.material->shader->set(prefix + "type", (int)lights[i]->type);

					transparentCommand.material->shader->set(prefix + "diffuse", lights[i]->diffuse);
					transparentCommand.material->shader->set(prefix + "specular", lights[i]->specular);
					transparentCommand.material->shader->set(prefix + "attenuation", lights[i]->attenuation);
					transparentCommand.material->shader->set(prefix + "cone_angles", lights[i]->cone_angles);
				}
			}
			else
			{
				// Set the "transform" uniform from the shader as the VP matrix in the world space
				transparentCommand.material->shader->set("transform", VP * transparentCommand.localToWorld);
			}
			// Draw Mesh
			transparentCommand.mesh->draw();
		}

		// If there is a postprocess material, apply postprocessing
		if (this->crashingEffect && postprocessMaterial)
		{

            ShaderProgram *postprocessShader = new ShaderProgram();
            postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            postprocessShader->attach(this->crashingPath, GL_FRAGMENT_SHADER);
            postprocessShader->link();

            postprocessMaterial->shader=postprocessShader;
			// TODO: (Req 11) Return to the default framebuffer
			// Bind Frame buffer as default (0) using glBindFramebuffer openGL function
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			// TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
			// Bind the Vertex Array
			glBindVertexArray(postProcessVertexArray);
			// Setup the post process material
			postprocessMaterial->setup();
			// Drawing the new triangles after postProcessing of the image
			// By setting the mode as GL_TRIANGLES
			// The first argument to 0 since we want to start from the beginning
			// The count is set to 3
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

        if(this->boostPath!="" && this->boostingEffect && postprocessMaterial){
                
                // Create the post processing shader
                ShaderProgram *postprocessShader = new ShaderProgram();
                postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
                postprocessShader->attach(this->boostPath, GL_FRAGMENT_SHADER);
                postprocessShader->link();

                postprocessMaterial->shader=postprocessShader;
                // TODO: (Req 11) Return to the default framebuffer
                // Bind Frame buffer as default (0) using glBindFramebuffer openGL function
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
                // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
                // Bind the Vertex Array
                glBindVertexArray(postProcessVertexArray);
                // Setup the post process material
                postprocessMaterial->setup();
                // Drawing the new triangles after postProcessing of the image
                // By setting the mode as GL_TRIANGLES
                // The first argument to 0 since we want to start from the beginning
                // The count is set to 3
                glDrawArrays(GL_TRIANGLES, 0, 3);
        }
	}
}