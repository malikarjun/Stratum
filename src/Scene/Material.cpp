#include "Material.hpp"
#include "Camera.hpp"
#include "Scene.hpp"

using namespace stm;

void Material::SetUniformBuffer(const string& name, const Buffer::RangeView& buffer, uint32_t arrayIndex) {
	vector<DescriptorSet::Entry>& vec = mDescriptorParameters[name];
	if (vec.size() <= arrayIndex) vec.resize(arrayIndex + 1);
	DescriptorSet::Entry p = vec[arrayIndex];
	p.mType = vk::DescriptorType::eUniformBuffer;
	p.mArrayIndex = arrayIndex;
	p.mBufferView = buffer;

	vec[arrayIndex] = p;
	mCacheValid = false;
}
void Material::SetStorageBuffer(const string& name, const Buffer::RangeView& buffer, uint32_t arrayIndex) {
	vector<DescriptorSet::Entry>& vec = mDescriptorParameters[name];
	if (vec.size() <= arrayIndex) vec.resize(arrayIndex + 1);
	DescriptorSet::Entry p = {};
	p.mType = vk::DescriptorType::eStorageBuffer;
	p.mArrayIndex = arrayIndex;
	p.mBufferView = buffer;

	vec[arrayIndex] = p;
	mCacheValid = false;
}
void Material::SetSampledTexture(const string& name, const TextureView& texture, uint32_t arrayIndex, vk::ImageLayout layout) {
	vector<DescriptorSet::Entry>& vec = mDescriptorParameters[name];
	if (vec.size() <= arrayIndex) vec.resize(arrayIndex + 1);
	DescriptorSet::Entry p = {};
	p.mType = vk::DescriptorType::eSampledImage;
	p.mArrayIndex = arrayIndex;
	p.mTextureView = texture;
	p.mImageLayout = layout;
	p.mSampler = nullptr;

	vec[arrayIndex] = p;
	mCacheValid = false;
}
void Material::SetStorageTexture(const string& name, const TextureView& texture, uint32_t arrayIndex, vk::ImageLayout layout) {
	vector<DescriptorSet::Entry>& vec = mDescriptorParameters[name];
	if (vec.size() <= arrayIndex) vec.resize(arrayIndex + 1);
	DescriptorSet::Entry p = {};
	p.mType = vk::DescriptorType::eStorageImage;
	p.mArrayIndex = arrayIndex;
	p.mTextureView = texture;
	p.mImageLayout = layout;
	p.mSampler = nullptr;
	
	vec[arrayIndex] = p;
	mCacheValid = false;
}
void Material::SetSampler(const string& name, shared_ptr<Sampler> sampler, uint32_t arrayIndex) {
	vector<DescriptorSet::Entry>& vec = mDescriptorParameters[name];
	if (vec.size() <= arrayIndex) vec.resize(arrayIndex + 1);
	DescriptorSet::Entry p = {};
	p.mType = vk::DescriptorType::eSampler;
	p.mArrayIndex = arrayIndex;
	p.mSampler = sampler;
	p.mImageLayout = vk::ImageLayout::eUndefined;
	p.mTextureView = {};

	vec[arrayIndex] = p;
	mCacheValid = false;
}

shared_ptr<GraphicsPipeline> Material::Bind(CommandBuffer& commandBuffer, optional<GeometryData> g) {
	ProfilerRegion ps("Material::Bind");

	vector<byte_blob> specializationData(mModules.size());
	vector<vector<vk::SpecializationMapEntry>> specializationEntries(mModules.size());
	vector<vk::SpecializationInfo> specializationInfos(mModules.size());
	vector<vk::SpecializationInfo*> specializationInfoPtrs(mModules.size());
	uint32_t i = 0;
	for (auto&[stage, spirv] : mModules) {
		if (!spirv.mShaderModule) {
			spirv.mDevice = *commandBuffer.mDevice;
			spirv.mShaderModule = commandBuffer.mDevice->createShaderModule(vk::ShaderModuleCreateInfo({}, spirv.mSpirv));
		}
		auto& mapData = specializationData[i];
		auto& mapEntries = specializationEntries[i];
		for (auto&[id, data] : mSpecializationConstants) {
			if (spirv.mSpecializationMap.count(id) == 0) continue;
			vk::SpecializationMapEntry& entry = mapEntries.emplace_back(spirv.mSpecializationMap.at(id));
			entry.offset = (uint32_t)mapData.size();
			mapData.resize(entry.offset + data.size());
			memcpy(mapData.data() + entry.offset, data.data(), data.size());
		}
		vk::SpecializationInfo& s = specializationInfos[i];
		specializationInfoPtrs[i] = &s;
		s.pData = mapData.data();
		s.dataSize = mapData.size();
		s.setMapEntries(mapEntries);
		i++;
	}

	vk::PipelineVertexInputStateCreateInfo vertexInfo = {};
	if (g) {
		auto[attributes, bindings] = CreateInputBindings(*g, mModules.at(vk::ShaderStageFlagBits::eVertex));
		vertexInfo.setVertexAttributeDescriptions(attributes);
		vertexInfo.setVertexBindingDescriptions(bindings);
	}

	// TODO: cache pipeline

	shared_ptr<GraphicsPipeline> pipeline = make_shared<GraphicsPipeline>(Name(), **commandBuffer.CurrentRenderPass(), commandBuffer.CurrentSubpassIndex(),
		mModules, specializationInfos,
		vertexInfo, g->mPrimitiveTopology, mCullMode, mPolygonMode,
		mSampleShading, vk::PipelineDepthStencilStateCreateInfo({}, mDepthTest, mDepthWrite, vk::CompareOp::eLessOrEqual, {}, {}, {}, {}, 0, 1));


	commandBuffer.BindPipeline(pipeline);
	

	if (!mCacheValid) {
		// Create a descriptorset with the currently bound inputs
		for (auto& [name, entries] : mDescriptorParameters) {
			auto it = pipeline->DescriptorBindings().find(name);
			if (it == pipeline->DescriptorBindings().end()) continue;

			uint32_t setIndex = it->second.mSet;

			if (mDescriptorSetCache.count(setIndex) && mDescriptorSetCache.at(setIndex)->Layout() != pipeline->DescriptorSetLayouts()[setIndex]) {
				commandBuffer.HoldResource(mDescriptorSetCache.at(setIndex));
				mDescriptorSetCache.erase(setIndex);
			}
			if (mDescriptorSetCache.count(setIndex) == 0) 
				mDescriptorSetCache.emplace(setIndex, commandBuffer.mDevice.GetPooledDescriptorSet(Name()+"/DescriptorSet"+to_string(setIndex), pipeline->DescriptorSetLayouts()[setIndex]));

			for (uint32_t i = 0; i < entries.size(); i++) {
				if (!entries[i]) continue;
				mDescriptorSetCache.at(setIndex)->at(it->second.mBinding.binding) = entries[i];
			}
		}
		mCacheValid = true;
	}

	for (auto& [index, descriptorSet] : mDescriptorSetCache) commandBuffer.BindDescriptorSet(descriptorSet, index);
	for (auto& [name, data] : mPushParameters) commandBuffer.PushConstant(name, data);

	return pipeline;
}


shared_ptr<GraphicsPipeline> MaterialDerivative::Bind(CommandBuffer& commandBuffer, optional<GeometryData> g) {
	ProfilerRegion ps("MaterialDerivative::Bind");

	shared_ptr<GraphicsPipeline> pipeline; // TODO: create ot fetch pipeline for mesh

	Material::Bind(commandBuffer, g);
	return pipeline;
}