#pragma once

#include <Scene/Scene.hpp>
#include <Scene/Camera.hpp>

#include "ImageLoader.hpp"
#include "Shaders/common.hlsli"

enum class OrganMaskBits : uint32_t {
	eBladder = 1,
	eKidney = 2,
	eColon = 4,
	eSpleen = 8,
	eIleum = 16,
	eAorta = 32,
	eAll = 0x3F
};
using OrganMask = vk::Flags<OrganMaskBits>;

class RenderVolume : public Object {
	private:
	// The volume loaded directly from the folder
	stm_ptr<Texture> mRawVolume = nullptr;
	// The mask loaded directly from the folder
	stm_ptr<Texture> mRawMask = nullptr;
	// The baked volume. This CAN be nullptr, in which case the pipeline will use the raw volume to compute colors on the fly.
	stm_ptr<Texture> mBakedVolume = nullptr;
	bool mBakeDirty = false;
	// The gradient of the volume. This CAN be nullptr, in which case the pipeline will compute the gradient on the fly.
	stm_ptr<Texture> mGradient = nullptr;
	bool mGradientDirty = false;
	
	stm_ptr<Buffer> mUniformBuffer;
	Device* mDevice;

public:
	enum class ShadingMode {
		eNone,
		eLocal
	};

	float mSampleRate = 1.f;
	bool mColorize = false;
	float mDensityScale = 1.f;
	float2 mHueRange = float2(.01f, .5f);
	float2 mRemapRange = float2(.125f, 1.f);
	ShadingMode mShadingMode = {};
	OrganMask mOrganMask = {};

	PLUGIN_EXPORT RenderVolume(const std::string& name, Device* device, const fs::path& imageStackFolder);
	PLUGIN_EXPORT void DrawGui(stm_ptr<CommandBuffer> commandBuffer, Camera* camera, GuiContext* gui);
	PLUGIN_EXPORT void UpdateBake(stm_ptr<CommandBuffer> commandBuffer);
	PLUGIN_EXPORT void Draw(stm_ptr<CommandBuffer> commandBuffer, Framebuffer* framebuffer, Camera* camera);
};
