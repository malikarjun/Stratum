#include "XRScene.hpp"

using namespace stm;

bool check_result(XrInstance instance, XrResult result, const string& errmsg) {
	if (XR_FAILED(result)) {
		char resultString[XR_MAX_RESULT_STRING_SIZE];
		xrResultToString(instance, result, resultString);
		fprintf_color(ConsoleColor::eRed, stderr, "%s: %s\n", errmsg.c_str(), resultString);
		return true;
	}
	return false;
}

XRScene::XRScene(Node& node) : mNode(node) {
	vector<const char*> extensions { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };
	xr::InstanceCreateInfo instanceInfo = {};
	instanceInfo.applicationInfo.apiVersion = xr::Version::current();
	strcpy(instanceInfo.applicationInfo.engineName, "Stratum");
	strcpy(instanceInfo.applicationInfo.applicationName, "Stratum");
	instanceInfo.applicationInfo.engineVersion = VK_MAKE_VERSION(STRATUM_VERSION_MAJOR, STRATUM_VERSION_MINOR, 0);
	instanceInfo.applicationInfo.applicationVersion = instanceInfo.applicationInfo.engineVersion;
	instanceInfo.enabledExtensionCount = extensions.size();
	instanceInfo.enabledExtensionNames = extensions.data();
	instanceInfo.enabledApiLayerCount = 0;
  mInstance = xr::createInstance(instanceInfo);

	xr::InstanceProperties instanceProperties = mInstance.getInstanceProperties();
	cout << "OpenXR runtime: " << instanceProperties.runtimeName << endl;

  mDispatch = xr::DispatchLoaderDynamic{ mInstance };

	//xr::DebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
	//debugMessengerInfo.messageSeverities = xr::DebugUtilsMessageSeverityFlagBitsEXT::Error | xr::DebugUtilsMessageSeverityFlagBitsEXT::Warning;
	//debugMessengerInfo.messageTypes = xr::DebugUtilsMessageTypeFlagBitsEXT::AllBits;
	//debugMessengerInfo.userCallback = DebugCallback;
	//mDebugMessenger = mInstance.createDebugUtilsMessengerEXT(debugMessengerInfo, mDispatch);

	xr::SystemGetInfo systemInfo = {};
	systemInfo.formFactor = xr::FormFactor::HeadMountedDisplay;
  mSystem = mInstance.getSystem(systemInfo);
	mSystemProperties = mInstance.getSystemProperties(mSystem);
	cout << "OpenXR system: " << mSystemProperties.systemName << endl;
}

void XRScene::destroy() {
	for (xr::SwapchainImageVulkanKHR* images : mSwapchainImages)
    delete[] images;
	for (const xr::Swapchain& swapchain : mSwapchains)
    swapchain.destroy();

	if (mPoseAction) mPoseAction.destroy();
	if (mGrabAction) mGrabAction.destroy();

	if (mActionSet) xrDestroyActionSet(mActionSet);
	for (const xr::Space& space : mActionSpaces) space.destroy();
	if (mReferenceSpace) mReferenceSpace.destroy(); 

	if (mSession) {
    mSession.endSession();
    mSession.destroy();
	}

	//if (mDebugMessenger) mDebugMessenger.destroy();
	if (mInstance) mInstance.destroy();

  for (component_ptr<Camera>& camera : mViews)
    camera.node().node_graph().erase(camera.node());

	mViews.clear();
	mSwapchainImages.clear();
	mSwapchains.clear();
	mActionSpaces.clear();
	mInstance = XR_NULL_HANDLE;
	mSession = XR_NULL_HANDLE;
	mReferenceSpace = XR_NULL_HANDLE;
	mActionSet = XR_NULL_HANDLE;
	mPoseAction = XR_NULL_HANDLE;
	mGrabAction = XR_NULL_HANDLE;
}

void XRScene::get_vulkan_extensions(string& instanceExtensions, string& deviceExtensions) {
	instanceExtensions = mInstance.getVulkanInstanceExtensionsKHR(mSystem, mDispatch);
	deviceExtensions = mInstance.getVulkanDeviceExtensionsKHR(mSystem, mDispatch);
}

/*
bool XRScene::InitScene(Scene* scene) {
	mScene = scene;

	#pragma region View configuration enumeration
	uint32_t viewConfigurationCount, tmp;
	xrEnumerateViewConfigurations(mInstance, mSystem, 0, &viewConfigurationCount, nullptr);
	vector<XrViewConfigurationType> viewConfigurations(viewConfigurationCount);
	xrEnumerateViewConfigurations(mInstance, mSystem, viewConfigurationCount, &tmp, viewConfigurations.data());
	mViewConfiguration = viewConfigurations[0];
	for (uint32_t i = 0; i < viewConfigurationCount; i++)
		if (viewConfigurations[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
			mViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			break;
		}

	xrEnumerateViewConfigurationViews(mInstance, mSystem, mViewConfiguration, 0, &mViewCount, nullptr);
	vector<XrViewConfigurationView> views(mViewCount);
	for (uint32_t i = 0; i < mViewCount; i++) views[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	xrEnumerateViewConfigurationViews(mInstance, mSystem, mViewConfiguration, mViewCount, &tmp, views.data());
	
	// View configuration should have just two views (eyes) that are identical...
	if (mViewCount != 2 ||
		views[0].recommendedSwapchainSampleCount != views[1].recommendedSwapchainSampleCount ||
		views[0].recommendedImageRectWidth != views[1].recommendedImageRectWidth || views[0].recommendedImageRectHeight != views[1].recommendedImageRectHeight) {
		fprintf_color(COLOR_RED, stderr, "%s", "Unsupported OpenXR view configuration.\n");
		Cleanup();
		return false;
	}

	uint32_t sampleCountInt = 1;
	VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
	switch (views[0].recommendedSwapchainSampleCount) {
		case 1:
			sampleCount = VK_SAMPLE_COUNT_1_BIT;
			sampleCountInt = 1;
			break;
		case 2:
			sampleCount = VK_SAMPLE_COUNT_2_BIT;
			sampleCountInt = 2;
			break;
		case 4:
			sampleCount = VK_SAMPLE_COUNT_4_BIT;
			sampleCountInt = 4;
			break;
		case 8:
			sampleCount = VK_SAMPLE_COUNT_8_BIT;
			sampleCountInt = 8;
			break;
		case 16:
		case 32:
		case 64:
			sampleCount = VK_SAMPLE_COUNT_16_BIT;
			sampleCountInt = 16;
			break;
		default:
			fprintf_color(COLOR_YELLOW, stderr, "Unsupported OpenXR recommended sample count: %u.\n", views[0].recommendedSwapchainSampleCount);
			break;
	}
	#pragma endregion

	CreateSession();
	if (!mSession) return false;

	#pragma region Swapchain creation
	uint32_t formatCount;
	xrEnumerateSwapchainFormats(mSession, 0, &formatCount, nullptr);
	vector<int64_t> formats(formatCount);
	xrEnumerateSwapchainFormats(mSession, formatCount, &tmp, formats.data());

	mSwapchainFormat = (VkFormat)formats[0];
	for (uint32_t i = 0; i < formatCount; i++)
		if ((VkFormat)formats[i] == VK_FORMAT_R8G8B8A8_UNORM) {
			mSwapchainFormat = (VkFormat)formats[i];
			break;
		} else if ((VkFormat)formats[i] == VK_FORMAT_B8G8R8A8_UNORM)
			mSwapchainFormat = (VkFormat)formats[i];
	

	uint32_t maxSwapchainLength = 0;

	mSwapchains.resize(mViewCount);
	mSwapchainImages.resize(mViewCount);

	for (uint32_t i = 0; i < mViewCount; i++) {
		XrSwapchainCreateInfo info = {};
		info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
		info.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		info.width = views[i].recommendedImageRectWidth;
		info.height = views[i].recommendedImageRectHeight;
		info.format = mSwapchainFormat;
		info.sampleCount = 1;
		info.faceCount = 1;
		info.arraySize = 1;
		info.mipCount = 1;
		if (XR_FAILED_MSG(xrCreateSwapchain(mSession, &info, &mSwapchains[i]), "xrCreateSwapchain failed")) {
			Cleanup();
			return false;
		}

		uint32_t len;
		if (XR_FAILED_MSG(xrEnumerateSwapchainImages(mSwapchains[i], 0, &len, nullptr), "xrEnumerateSwapchainImages failed")) {
			Cleanup();
			return false;
		}	               

		maxSwapchainLength = max(maxSwapchainLength, len);
	}

	for (uint32_t i = 0; i < mViewCount; i++) {
		mSwapchainImages[i] = new XrSwapchainImageVulkanKHR[maxSwapchainLength];
		for (uint32_t j = 0; j < maxSwapchainLength; j++)
			mSwapchainImages[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
		
		uint32_t len;
		if (XR_FAILED_MSG(xrEnumerateSwapchainImages(mSwapchains[i], maxSwapchainLength, &len, (XrSwapchainImageBaseHeader*)mSwapchainImages[i]), "xrEnumerateSwapchainImages failed")) {
			Cleanup();
			return false;
		}	         
	}

	mProjectionViews.resize(mViewCount);
	for (uint32_t i = 0; i < mViewCount; i++) {
		mProjectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		mProjectionViews[i].subImage.swapchain = mSwapchains[i];
		mProjectionViews[i].subImage.imageRect.offset.x = 0;
		mProjectionViews[i].subImage.imageRect.offset.y = 0;
		mProjectionViews[i].subImage.imageRect.extent.width = views[i].recommendedImageRectWidth;
		mProjectionViews[i].subImage.imageRect.extent.height = views[i].recommendedImageRectHeight;
	}
	#pragma endregion
	
	#pragma region Action creation
	XrActionSetCreateInfo setInfo = {};
	setInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	strcpy(setInfo.actionSetName, "actionset");
	strcpy(setInfo.localizedActionSetName, "Action Set");
	if (XR_FAILED_MSG(xrCreateActionSet(mInstance, &setInfo, &mActionSet), "xrCreateActionSet failed")) {
		Cleanup();
		return false;
	}

	mActionSpaces.resize(2); // 2 hands
	mHandPaths.resize(2);
	XrPath selectClickPath[2];
	XrPath posePath[2];

	xrStringToPath(mInstance, "/user/hand/left", &mHandPaths[0]);
	xrStringToPath(mInstance, "/user/hand/right", &mHandPaths[1]);

	XrActionCreateInfo actionInfo = {};
	actionInfo.type = XR_TYPE_ACTION_CREATE_INFO;
	actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
	actionInfo.countSubactionPaths = mHandPaths.size();
	actionInfo.subactionPaths = mHandPaths.data();
	strcpy(actionInfo.actionName, "triggergrab"); // assuming every controller has some form of main "trigger" button
	strcpy(actionInfo.localizedActionName, "Grab Object with Trigger Button");
	if (XR_FAILED_MSG(xrCreateAction(mActionSet, &actionInfo, &mGrabAction), "xrCreateAction failed for triggergrab")) {
		Cleanup();
		return false;
	}

	actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy(actionInfo.actionName, "handpose");
	strcpy(actionInfo.localizedActionName, "Hand Pose");
	actionInfo.countSubactionPaths = mHandPaths.size();
	actionInfo.subactionPaths = mHandPaths.data();
	if (XR_FAILED_MSG(xrCreateAction(mActionSet, &actionInfo, &mPoseAction), "xrCreateAction failed for handpose")) {
		Cleanup();
		return false;
	}

	xrStringToPath(mInstance, "/user/hand/left/input/select/click", &selectClickPath[0]);
	xrStringToPath(mInstance, "/user/hand/right/input/select/click", &selectClickPath[1]);
	xrStringToPath(mInstance, "/user/hand/left/input/grip/pose", &posePath[0]);
	xrStringToPath(mInstance, "/user/hand/right/input/grip/pose", &posePath[1]);

	XrPath khrSimpleInteractionProfilePath;
	if (XR_FAILED_MSG(xrStringToPath(mInstance, "/interaction_profiles/khr/simple_controller", &khrSimpleInteractionProfilePath), "xrStringToPath failed for /interaction_profiles/khr/simple_controller")) {
		Cleanup();
		return false;
	}

	vector<XrActionSuggestedBinding> bindings(4);
	bindings[0].action = mPoseAction;
	bindings[0].binding = posePath[0];
	bindings[1].action = mPoseAction;
	bindings[1].binding = posePath[1];
	bindings[2].action = mGrabAction;
	bindings[2].binding = selectClickPath[0];
	bindings[3].action = mGrabAction;
	bindings[3].binding = selectClickPath[1];

	XrInteractionProfileSuggestedBinding suggestedBindings = {};
	suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
	suggestedBindings.interactionProfile = khrSimpleInteractionProfilePath;
	suggestedBindings.countSuggestedBindings = bindings.size();
	suggestedBindings.suggestedBindings = bindings.data();
	if (XR_FAILED_MSG(xrSuggestInteractionProfileBindings(mInstance, &suggestedBindings), "xrSuggestInteractionProfileBindings failed")) {
		Cleanup();
		return false;
	}

	// Create action space for each hand
	XrActionSpaceCreateInfo actionSpaceInfo = {};
	actionSpaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	actionSpaceInfo.action = mPoseAction;
	actionSpaceInfo.poseInActionSpace.position = { 0.f, 0.f, 0.f };
	actionSpaceInfo.poseInActionSpace.orientation = { 0.f, 0.f, 0.f, 1.f };
	for (uint32_t i = 0; i < mActionSpaces.size(); i++){
		actionSpaceInfo.subactionPath = mHandPaths[i];
		if (XR_FAILED_MSG(xrCreateActionSpace(mSession, &actionSpaceInfo, &mActionSpaces[i]), "xrCreateActionSpace failed")) {
			Cleanup();
			return false;
		}
	}

	XrSessionActionSetsAttachInfo attachInfo = {};
	attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &mActionSet;
	if (XR_FAILED_MSG(xrAttachSessionActionSets(mSession, &attachInfo), "xrAttachSessionActionSets failed")) {
		Cleanup();
		return false;
	}
	#pragma endregion

	auto camera = make_shared<Camera>(mSystemProperties.systemName, mScene->Instance()->Device(), VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, sampleCount);
	camera->FramebufferWidth(views[0].recommendedImageRectWidth * 2);
	camera->FramebufferHeight(views[0].recommendedImageRectHeight);
	camera->ViewportX(0);
	camera->ViewportY(0);
	camera->ViewportWidth(views[0].recommendedImageRectWidth * 2);
	camera->ViewportHeight(views[0].recommendedImageRectHeight);
	camera->StereoMode(STEREO_SBS_HORIZONTAL);
	mScene->AddObject(camera);
	mHmdCamera = camera.get();

	return true;
}

void XRScene::CreateSession() {
	XrGraphicsBindingVulkanKHR binding = {};
	binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
	binding.instance = *mScene->Instance();
	binding.physicalDevice = mScene->Instance()->Device()->PhysicalDevice();
	binding.device = *mScene->Instance()->Device();
	binding.queueFamilyIndex = mScene->Instance()->Device()->PresentQueueFamilyIndex();
	binding.queueIndex = mScene->Instance()->Device()->PresentQueueIndex();

	XrSessionCreateInfo sessioninfo = {};
	sessioninfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessioninfo.systemId = mSystem;
	sessioninfo.next = &binding;
	if (XR_FAILED_MSG(xrCreateSession(mInstance, &sessioninfo, &mSession), "xrCreateSession failed")) return;
	printf_color(COLOR_GREEN, "%s", "OpenXR Session created.\n");


	uint32_t spaceCount, tmp;
	xrEnumerateReferenceSpaces(mSession, 0, &spaceCount, nullptr);
	vector<XrReferenceSpaceType> referenceSpaces(spaceCount);
	xrEnumerateReferenceSpaces(mSession, spaceCount, &tmp, referenceSpaces.data());

	// prefer Stage, then Local, then View
	mReferenceSpaceType = referenceSpaces[0];
	for (uint32_t i = 0; i < spaceCount; i++)
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
			mReferenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
			break;
		} else if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_LOCAL)
			mReferenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	
	XrReferenceSpaceCreateInfo referenceSpaceInfo = {};
	referenceSpaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	referenceSpaceInfo.referenceSpaceType = mReferenceSpaceType;
	referenceSpaceInfo.poseInReferenceSpace.orientation = { 0.f, 0.f, 0.f, 1.f};
	referenceSpaceInfo.poseInReferenceSpace.position = { 0.f, 0.f, 0.f };
	if (XR_FAILED_MSG(xrCreateReferenceSpace(mSession, &referenceSpaceInfo, &mReferenceSpace), "xrCreateReferenceSpace failed")) {
		xrDestroySession(mSession);
		mSession = XR_NULL_HANDLE;
		return;
	}

	if (mReferenceSpaceType == XR_REFERENCE_SPACE_TYPE_STAGE){
		XrExtent2Df bounds;
		if (XR_SUCCEEDED(xrGetReferenceSpaceBoundsRect(mSession, mReferenceSpaceType, &bounds)))
			printf("XR_REFERENCE_SPACE_TYPE_STAGE: %.3fx%.3f\n", bounds.width, bounds.height);
	}

	XrSessionBeginInfo begin = {};
	begin.type = XR_TYPE_SESSION_BEGIN_INFO;
	begin.primaryViewConfigurationType = mViewConfiguration;
	if (XR_FAILED_MSG(xrBeginSession(mSession, &begin), "xrBeginSession failed")) {
		xrDestroySpace(mReferenceSpace);
		xrDestroySession(mSession);
		mSession = XR_NULL_HANDLE;
		return;
	}
}

void XRScene::BeginFrame() {
	if (!mInstance || !mSession) return;

	mFrameState = {};
	mFrameState.type = XR_TYPE_FRAME_STATE;

	XrFrameWaitInfo frameWaitInfo = {};
	frameWaitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	if (XR_FAILED_MSG(xrWaitFrame(mSession, &frameWaitInfo, &mFrameState), "xrWaitFrame failed")) {
		Cleanup();
		return;
	}

	#pragma region Poll events
	XrEventDataBuffer event = {};
	event.type = XR_TYPE_EVENT_DATA_BUFFER;
	if (XR_FAILED_MSG(xrPollEvent(mInstance, &event), "xrPollEvent failed")) return;

	switch (event.type) {
	case XR_TYPE_EVENT_DATA_EVENTS_LOST:
		// lost an event
		break;

	case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:{
		XrEventDataSessionStateChanged* state = (XrEventDataSessionStateChanged*)&event;
		switch (state->state) {
    	case XR_SESSION_STATE_UNKNOWN:
    	case XR_SESSION_STATE_IDLE:
    	case XR_SESSION_STATE_READY:
    	case XR_SESSION_STATE_SYNCHRONIZED:
    	case XR_SESSION_STATE_VISIBLE:
    	case XR_SESSION_STATE_FOCUSED:
			mVisible = true;
			break;

    	case XR_SESSION_STATE_STOPPING:
    	case XR_SESSION_STATE_EXITING:
		case XR_SESSION_STATE_LOSS_PENDING:
			printf_color(COLOR_YELLOW, "%s", "xrSession state lost\n");
			Cleanup();
			mVisible = false;
			return;
		}
		break;
	}
	case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		printf_color(COLOR_YELLOW, "%s", "xrInstance lost\n");
		Cleanup();
		return;
	}
	#pragma endregion

	if (!mVisible) return;

	XrFrameBeginInfo frameBeginInfo = {};
	frameBeginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
	if (XR_FAILED_MSG(xrBeginFrame(mSession, &frameBeginInfo), "xrBeginFrame failed")) {
		Cleanup();
		return;
	}

	#pragma region Sync actions
	XrActiveActionSet activeActionSet = {};
	activeActionSet.actionSet = mActionSet;

	XrActionsSyncInfo syncInfo = {};
	syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeActionSet;
	XR_FAILED_MSG(xrSyncActions(mSession, &syncInfo), "xrSyncActions failed");

	XrActionStateGetInfo getInfo = {};
	getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
	getInfo.action = mGrabAction;
	getInfo.subactionPath = mHandPaths[0];

	XrActionStateFloat grabValue = {};
	grabValue.type = XR_TYPE_ACTION_STATE_FLOAT;
	XR_FAILED_MSG(xrGetActionStateFloat(mSession, &getInfo, &grabValue), "xrGetActionStateFloat failed for Grab Action");

	getInfo.action = mPoseAction;
	XrActionStatePose poseState = {};
	poseState.type = XR_TYPE_ACTION_STATE_POSE;
	XR_FAILED_MSG(xrGetActionStatePose(mSession, &getInfo, &poseState), "xrGetActionStatePose failed for Pose Action");

	vector<XrSpaceLocation> spaceLocation(mActionSpaces.size());
	for (uint32_t i = 0; i < mActionSpaces.size(); i++) {
		spaceLocation[i].type = XR_TYPE_SPACE_LOCATION;
		XR_FAILED_MSG(xrLocateSpace(mActionSpaces[i], mReferenceSpace, mFrameState.predictedDisplayTime, &spaceLocation[i]), "xrLocateSpace failed");
	}
	#pragma endregion

	// Advance swapchains
	for (uint32_t i = 0; i < mViewCount; i++){
		XrSwapchainImageAcquireInfo swapchainImageAcquireInfo = {};
		swapchainImageAcquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
		uint32_t bufferIndex;
		if (XR_FAILED_MSG(xrAcquireSwapchainImage(mSwapchains[i], &swapchainImageAcquireInfo, &bufferIndex), "xrAcquireSwapchainImage failed")) {
			Cleanup();
			return;
		}

		XrSwapchainImageWaitInfo swapchainImageWaitInfo = {};
		swapchainImageWaitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
		swapchainImageWaitInfo.timeout = 1000;
		if (XR_FAILED_MSG(xrWaitSwapchainImage(mSwapchains[i], &swapchainImageWaitInfo), "xrWaitSwapchainImage failed")) {
			Cleanup();
			return;
		}

		mProjectionViews[i].subImage.imageArrayIndex = bufferIndex;
	}
}

void XRScene::PostRender(CommandBuffer* commandBuffer) {
	#pragma region Locate views (eyes) within mReferenceSpace
	uint32_t tmp;
	vector<XrView> views(mViewCount);
	for (uint32_t i = 0; i < mViewCount; i++) views[i].type = XR_TYPE_VIEW;

	XrViewLocateInfo viewLocateInfo = {};
	viewLocateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	viewLocateInfo.displayTime = mFrameState.predictedDisplayTime;
	viewLocateInfo.space = mReferenceSpace;
	XrViewState viewState = {};
	viewState.type = XR_TYPE_VIEW_STATE;
	if (XR_FAILED_MSG(xrLocateViews(mSession, &viewLocateInfo, &viewState, mViewCount, &tmp, views.data()), "xrLocateViews failed")) {
		Cleanup();
		return;
	}
	#pragma endregion

	vector<float3> positions(mViewCount);
	vector<quaternion> rotations(mViewCount);

	StereoEye eyes[2] { EYE_LEFT, EYE_RIGHT };

	float n = mHmdCamera->Near();

	// Update projection views
	for (uint32_t i = 0; i < mViewCount; i++) {
		mProjectionViews[i].pose = views[i].pose;
		mProjectionViews[i].fov = views[i].fov;

		mHmdCamera->Projection(float4x4::Perspective(
			n * tanf(views[i].fov.angleLeft), n * tanf(views[i].fov.angleRight), n * tanf(views[i].fov.angleDown), n * tanf(views[i].fov.angleUp),
			n, mHmdCamera->Far()), eyes[i]);
			
		positions[i] = float3(views[i].pose.position.x, views[i].pose.position.y, -views[i].pose.position.z);
		rotations[i] = quaternion(-views[i].pose.orientation.x, -views[i].pose.orientation.y, views[i].pose.orientation.z, views[i].pose.orientation.w);
	}

	float3 center = (positions[0] + positions[1]) * .5f;
	quaternion iq = inverse(rotations[0]);

	mHmdCamera->LocalPosition(center);
	mHmdCamera->LocalRotation(rotations[0]);

	mHmdCamera->EyeOffsetTranslate(iq * (positions[0] - center), eyes[0]);
	mHmdCamera->EyeOffsetTranslate(iq * (positions[1] - center), eyes[1]);
	mHmdCamera->EyeOffsetRotate(quaternion(0,0,0,1), eyes[0]);
	mHmdCamera->EyeOffsetRotate(iq * rotations[1], eyes[1]);

	mHmdCamera->SetUniforms();

	#pragma region Copy images
	Texture* src = mHmdCamera->ResolveBuffer();
	VkImage right = mSwapchainImages[0][mProjectionViews[0].subImage.imageArrayIndex].image;
	VkImage left = mSwapchainImages[1][mProjectionViews[1].subImage.imageArrayIndex].image;

	VkImageLayout srcLayout = mHmdCamera->SampleCount() == VK_SAMPLE_COUNT_1_BIT ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;

	src->TransitionImageLayout(srcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);
	Texture::TransitionImageLayout(left, mSwapchainFormat, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
	Texture::TransitionImageLayout(right, mSwapchainFormat, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

	VkImageCopy rgn = {};
	rgn.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	rgn.srcSubresource.layerCount = 1;
	rgn.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	rgn.dstSubresource.layerCount = 1;
	rgn.extent.width = mHmdCamera->FramebufferWidth() / 2;
	rgn.extent.height = mHmdCamera->FramebufferHeight();
	rgn.extent.depth = 1;

	vkCmdCopyImage(*commandBuffer,
		mHmdCamera->ResolveBuffer()->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		left, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rgn);
	rgn.srcOffset.x = mHmdCamera->FramebufferWidth() / 2;
	vkCmdCopyImage(*commandBuffer,
		src->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		right, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rgn);
	
	src->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcLayout, commandBuffer);
	Texture::TransitionImageLayout(left, mSwapchainFormat, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);
	Texture::TransitionImageLayout(right, mSwapchainFormat, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);

	#pragma endregion
}

void XRScene::EndFrame() {
	for (uint32_t i = 0; i < mViewCount; i++) {
		XrSwapchainImageReleaseInfo swapchainImageReleaseInfo = {};
		swapchainImageReleaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
		if (XR_FAILED_MSG(xrReleaseSwapchainImage(mSwapchains[i], &swapchainImageReleaseInfo), "xrReleaseSwapchainImage failed")) {
			Cleanup();
			return;
		}
	}

	XrCompositionLayerProjection projectionLayer = {};
	projectionLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	projectionLayer.space = mReferenceSpace;
	projectionLayer.viewCount = mViewCount;
	projectionLayer.views = mProjectionViews.data();

	XrCompositionLayerBaseHeader* projectionlayers[1];
	projectionlayers[0] = (XrCompositionLayerBaseHeader*)&projectionLayer;

	XrFrameEndInfo frameEndInfo = {};
	frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
	frameEndInfo.displayTime = mFrameState.predictedDisplayTime;
	frameEndInfo.layerCount = 1;
	frameEndInfo.layers = projectionlayers;
	frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	if (XR_FAILED_MSG(xrEndFrame(mSession, &frameEndInfo), "xrEndFrame failed")) {
		Cleanup();
		return;
	}
}

*/