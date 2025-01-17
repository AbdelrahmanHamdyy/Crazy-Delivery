#pragma once

#include <glad/gl.h>
#include <glm/vec4.hpp>
#include <json/json.hpp>

namespace our {
    // There are some options in the render pipeline that we cannot control via shaders
    // such as blending, depth testing and so on
    // Since each material could require different options (e.g. transparent materials usually use blending),
    // we will encapsulate all these options into a single structure that will also be responsible for configuring OpenGL's pipeline
    struct PipelineState {
        // This set of pipeline options specifies whether face culling will be used or not and how it will be configured
        struct {
            bool enabled = false;
            GLenum culledFace = GL_BACK;
            GLenum frontFace = GL_CCW;
        } faceCulling;

        // This set of pipeline options specifies whether depth testing will be used or not and how it will be configured
        struct {
            bool enabled = false;
            GLenum function = GL_LEQUAL;
        } depthTesting;

        // This set of pipeline options specifies whether blending will be used or not and how it will be configured
        struct {
            bool enabled = false;
            GLenum equation = GL_FUNC_ADD;
            GLenum sourceFactor = GL_SRC_ALPHA;
            GLenum destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
            glm::vec4 constantColor = {0, 0, 0, 0};
        } blending;

        // These options specify the color and depth mask which can be used to
        // prevent the rendering/clearing from modifying certain channels of certain targets in the framebuffer
        glm::bvec4 colorMask = {true, true, true, true}; // To know how to use it, check glColorMask
        bool depthMask = true; // To know how to use it, check glDepthMask


        // This function should set the OpenGL options to the values specified by this structure
        // For example, if faceCulling.enabled is true, you should call glEnable(GL_CULL_FACE), otherwise, you should call glDisable(GL_CULL_FACE)
        void setup() const {
            // TODO: (Req 4) Write this function
            // Check if face culling is enabled then set the culling face and front face
            // We know the culling face and front face as we send to it if it's a CCW or CW
            // Counter clockwise-> Front Face
            // Clockwise-> Back Face
            // If we rotated the object 180 degree then the faces will be changed
            if (faceCulling.enabled) {
                // Firstly we need to enable GL_CULL_FACE
                glEnable(GL_CULL_FACE);
                // Which faces we will remove
                glCullFace(faceCulling.culledFace);
                // Direction of the front face that won't be removed
                glFrontFace(faceCulling.frontFace);
            }
            else {
                // Else disable face culling
                glDisable(GL_CULL_FACE);
            }

            // Check if depth testing is enabled then set the depth function
            // and specify the color and depth mask
            if (depthTesting.enabled) {
                // Firstly we need to enable GL_DEPTH_TEST
                glEnable(GL_DEPTH_TEST);
                // Which method should we choose to make the depth testing
                glDepthFunc(depthTesting.function);
                // Set the color & depth mask using the GL functions & simply passing them
                glColorMask(colorMask.r, colorMask.g, colorMask.b, colorMask.a);
                glDepthMask(depthMask);
            }
            else {
                // Else disable depth testing
                glDisable(GL_DEPTH_TEST);
            }

            // Check if blending is enabled then set the blending equation, source factor and destination factor
            // and specify the constant color
            if (blending.enabled) {
                // Firstly we need to enable GL_BLEND
                glEnable(GL_BLEND);
                // Set the blending equation, source factor and destination factor
                glBlendEquation(blending.equation);
                glBlendFunc(blending.sourceFactor, blending.destinationFactor);
                // Retrieve the constant color from blending and set as our blend color
                glBlendColor(blending.constantColor.r, blending.constantColor.g, blending.constantColor.b, blending.constantColor.a);
            }
            else {
                // Else disable blending
                glDisable(GL_BLEND);
            }
        }

        // Given a json object, this function deserializes a PipelineState structure
        void deserialize(const nlohmann::json& data);
    };
}