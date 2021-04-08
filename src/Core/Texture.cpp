#include "Texture.hpp"

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "Pipeline.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

using namespace stm;

tuple<byte_blob, vk::Extent3D, vk::Format> Texture::LoadPixels(const fs::path& filename, bool srgb) {
	int x,y,channels;
	stbi_info(filename.string().c_str(), &x, &y, &channels);

	int desiredChannels = 0;
	if (channels == 3) desiredChannels = 4;

	byte* pixels = nullptr;
	size_t stride = 0;
	vk::Format format = vk::Format::eUndefined;
	if (stbi_is_hdr(filename.string().c_str())) {
		pixels = (byte*)stbi_loadf(filename.string().c_str(), &x, &y, &channels, desiredChannels);
		stride = sizeof(float);
		switch(desiredChannels ? desiredChannels : channels) {
			case 1: format = vk::Format::eR32Sfloat; break;
			case 2: format = vk::Format::eR32G32Sfloat; break;
			case 3: format = vk::Format::eR32G32B32Sfloat; break;
			case 4: format = vk::Format::eR32G32B32A32Sfloat; break;
		}
	} else if (stbi_is_16_bit(filename.string().c_str())) {
		pixels = (byte*)stbi_load_16(filename.string().c_str(), &x, &y, &channels, desiredChannels);
		stride = sizeof(uint16_t);
		switch(desiredChannels ? desiredChannels : channels) {
			case 1: format = vk::Format::eR16Unorm; break;
			case 2: format = vk::Format::eR16G16Unorm; break;
			case 3: format = vk::Format::eR16G16B16Unorm; break;
			case 4: format = vk::Format::eR16G16B16A16Unorm; break;
		}
	} else {
		pixels = (byte*)stbi_load(filename.string().c_str(), &x, &y, &channels, desiredChannels);
		stride = sizeof(uint8_t);
		switch (desiredChannels ? desiredChannels : channels) {
			case 1: format = vk::Format::eR8Unorm; break;
			case 2: format = vk::Format::eR8G8Unorm; break;
			case 3: format = vk::Format::eR8G8B8Unorm; break;
			case 4: format = vk::Format::eR8G8B8A8Unorm; break;
		}
	}
	if (!pixels) throw invalid_argument("could not load " + filename.string());
	if (desiredChannels) channels = desiredChannels;

	byte_blob data(span<byte>(pixels, x*y*channels*stride));
	stbi_image_free(pixels);
	return make_tuple(move(data), vk::Extent3D(x,y,1), move(format));
}

Texture::Texture(Device& device, const string& name, const vk::Extent3D& extent, vk::Format format, uint32_t arrayLayers, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::ImageUsageFlags usage, vk::ImageCreateFlags createFlags, vk::MemoryPropertyFlags properties, vk::ImageTiling tiling)
		: DeviceResource(device, name), mExtent(extent), mFormat(format), mSampleCount(numSamples), mUsage(usage), 
		mMipLevels(mipLevels ? mipLevels : (numSamples > vk::SampleCountFlagBits::e1 ? 1 : (uint32_t)floor(log2(max(mExtent.width, mExtent.height))) + 1)),
		mCreateFlags(createFlags), mMemoryProperties(properties), mArrayLayers(arrayLayers), mTiling(tiling) {
	
	vk::ImageCreateInfo imageInfo = {};
	imageInfo.imageType = mExtent.depth > 1 ? vk::ImageType::e3D
		: (mExtent.height > 1 || (mExtent.width == 1 && mExtent.height == 1 && mExtent.depth == 1)) ? vk::ImageType::e2D // special 1x1 case: force 2D
		: vk::ImageType::e1D;
	imageInfo.extent.width = mExtent.width;
	imageInfo.extent.height = mExtent.height;
	imageInfo.extent.depth = mExtent.depth;
	imageInfo.mipLevels = mMipLevels;
	imageInfo.arrayLayers = mArrayLayers;
	imageInfo.format = mFormat;
	imageInfo.tiling = mTiling;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = mUsage;
	imageInfo.samples = mSampleCount;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.flags = mCreateFlags;
	mImage = mDevice->createImage(imageInfo);
	mDevice.SetObjectName(mImage, Name());

	mMemoryBlock = mDevice.AllocateMemory(mDevice->getImageMemoryRequirements(mImage), mMemoryProperties);
	mDevice->bindImageMemory(mImage, *mMemoryBlock->mMemory, mMemoryBlock->mOffset);
	
	switch (mFormat) {
	default:
		mAspectFlags = vk::ImageAspectFlagBits::eColor;
		break;
	case vk::Format::eD16Unorm:
	case vk::Format::eD32Sfloat:
		mAspectFlags = vk::ImageAspectFlagBits::eDepth;
		break;
	case vk::Format::eS8Uint:
		mAspectFlags = vk::ImageAspectFlagBits::eStencil;
		break;
	case vk::Format::eD16UnormS8Uint:
	case vk::Format::eD24UnormS8Uint:
	case vk::Format::eD32SfloatS8Uint:
	case vk::Format::eX8D24UnormPack32:
		mAspectFlags = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		break;
	}

	mTrackedLayout = vk::ImageLayout::eUndefined;
	mTrackedStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
	mTrackedAccessFlags = {};
}

void Texture::TransitionBarrier(CommandBuffer& commandBuffer, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	if (oldLayout == newLayout) return;
	if (newLayout == vk::ImageLayout::eUndefined) {
		mTrackedLayout = newLayout;
		mTrackedStageFlags = dstStage;
		mTrackedAccessFlags = {};
		return;
	}
	vk::ImageMemoryBarrier barrier = {};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = mImage;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = MipLevels();
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = ArrayLayers();
	barrier.srcAccessMask = mTrackedAccessFlags;
	barrier.dstAccessMask = GuessAccessMask(newLayout);
	barrier.subresourceRange.aspectMask = mAspectFlags;
	commandBuffer.Barrier(srcStage, dstStage, barrier);
	mTrackedLayout = newLayout;
	mTrackedStageFlags = dstStage;
	mTrackedAccessFlags = barrier.dstAccessMask;
}

void Texture::GenerateMipMaps(CommandBuffer& commandBuffer) {
	// Transition mip 0 to vk::ImageLayout::eTransferSrcOptimal
	commandBuffer.TransitionBarrier(mImage, { mAspectFlags, 0, 1, 0, mArrayLayers }, mTrackedStageFlags, vk::PipelineStageFlagBits::eTransfer, mTrackedLayout, vk::ImageLayout::eTransferSrcOptimal);
	// Transition all other mips to vk::ImageLayout::eTransferDstOptimal
	commandBuffer.TransitionBarrier(mImage, { mAspectFlags, 1, mMipLevels - 1, 0, mArrayLayers }, mTrackedStageFlags, vk::PipelineStageFlagBits::eTransfer, mTrackedLayout, vk::ImageLayout::eTransferDstOptimal);

	vk::ImageBlit blit = {};
	blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = mArrayLayers;
	blit.srcSubresource.aspectMask = mAspectFlags;
	blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = mArrayLayers;
	blit.dstSubresource.aspectMask = mAspectFlags;

	blit.srcOffsets[1] = vk::Offset3D((int32_t)mExtent.width, (int32_t)mExtent.height, (int32_t)mExtent.depth);

	for (uint32_t i = 1; i < mMipLevels; i++) {
		// Blit the previous mip level into this one
		blit.srcSubresource.mipLevel = i - 1;
		blit.dstSubresource.mipLevel = i;
		blit.dstOffsets[1].x = max(1, blit.srcOffsets[1].x / 2);
		blit.dstOffsets[1].y = max(1, blit.srcOffsets[1].y / 2);
		blit.dstOffsets[1].z = max(1, blit.srcOffsets[1].z / 2);
		commandBuffer->blitImage(
			mImage, vk::ImageLayout::eTransferSrcOptimal,
			mImage, vk::ImageLayout::eTransferDstOptimal,
			{ blit }, vk::Filter::eLinear);

		blit.srcOffsets[1] = blit.dstOffsets[1];
		
		// Transition this mip level to vk::ImageLayout::eTransferSrcOptimal
		commandBuffer.TransitionBarrier(mImage, { mAspectFlags, i, 1, 0, mArrayLayers }, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal);
	}

	mTrackedLayout = vk::ImageLayout::eTransferSrcOptimal;
	mTrackedStageFlags = vk::PipelineStageFlagBits::eTransfer;
	mTrackedAccessFlags = vk::AccessFlagBits::eTransferRead;
}