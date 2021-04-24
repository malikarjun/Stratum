#include "Window.hpp"

using namespace stm;

Window::Window(Instance& instance, const string& title, vk::Rect2D position) : mInstance(instance), mTitle(title), mClientRect(position) {
	#ifdef WIN32

	mWindowedRect = {};

	SetProcessDPIAware();

	mHwnd = CreateWindowExA(
		NULL,
		"Stratum",
		mTitle.c_str(),
		WS_OVERLAPPEDWINDOW,
		position.offset.x,
		position.offset.y,
		position.extent.width,
		position.extent.height,
		NULL,
		NULL,
		mInstance.hInstance(),
		nullptr );
	if (!mHwnd) throw runtime_error("failed to create window");

	ShowWindow(mHwnd, SW_SHOW);
	
	vk::Win32SurfaceCreateInfoKHR info = {};
	info.hinstance = mInstance.hInstance();
	info.hwnd = mHwnd;
	mSurface = mInstance->createWin32SurfaceKHR(info);

	RECT cr;
	GetClientRect(mHwnd, &cr);
	mClientRect.offset = vk::Offset2D((int32_t)cr.top, (int32_t)cr.left);
	mClientRect.extent = vk::Extent2D((uint32_t)(cr.right - cr.left), (uint32_t)(cr.bottom - cr.top));

	#elif defined(__linux)

	xcbConnection = mInstance.XCBConnection();
	xcbScreen = mInstance->XCBScreen();
	mXCBWindow = xcb_generate_id(xcbConnection);

	uint32_t valueList[] {
		XCB_EVENT_MASK_BUTTON_PRESS		| XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_KEY_PRESS   		| XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_POINTER_MOTION	| XCB_EVENT_MASK_BUTTON_MOTION };
	xcb_create_window(
		xcbConnection,
		XCB_COPY_FROM_PARENT,
		mXCBWindow,
		xcbScreen->root,
		position.offset.x,
		position.offset.y,
		position.extent.width,
		position.extent.height,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		xcbScreen->root_visual,
		XCB_CW_EVENT_MASK, valueList);

	xcb_change_property(
		xcbConnection,
		XCB_PROP_MODE_REPLACE,
		mXCBWindow,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		title.length(),
		title.c_str());

	xcb_intern_atom_cookie_t wmDeleteCookie = xcb_intern_atom(xcbConnection, 0, strlen("WM_DELETE_WINDOW"), "WM_DELETE_WINDOW");
	xcb_intern_atom_cookie_t wmProtocolsCookie = xcb_intern_atom(xcbConnection, 0, strlen("WM_PROTOCOLS"), "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* wmDeleteReply = xcb_intern_atom_reply(xcbConnection, wmDeleteCookie, NULL);
	xcb_intern_atom_reply_t* wmProtocolsReply = xcb_intern_atom_reply(xcbConnection, wmProtocolsCookie, NULL);
	mXCBDeleteWin = wmDeleteReply->atom;
	mXCBProtocols = wmProtocolsReply->atom;
	xcb_change_property(xcbConnection, XCB_PROP_MODE_REPLACE, mXCBWindow, wmProtocolsReply->atom, 4, 32, 1, &wmDeleteReply->atom);

	xcb_map_window(xcbConnection, mXCBWindow);
	xcb_flush(xcbConnection);

	VkXcbSurfaceCreateInfoKHR info = {};
	info.connection = xcbConnection;
	info.window = mXCBWindow;
	mSurface = mInstance->vkCreateXcbSurfaceKHR(*mInstance, &info, nullptr, &mSurface);

	#endif
}
Window::~Window() {
	destroy_swapchain();
	mInstance->destroySurfaceKHR(mSurface);

#ifdef WIN32
	::DestroyWindow(mHwnd);
#elif defined(__linux)
	if (mInstance.XCBConnection() && mXCBWindow)
 		xcb_destroy_window(mInstance.XCBConnection(), mXCBWindow);
#endif
}

const Texture::View& Window::acquire_image(CommandBuffer& commandBuffer) {
	ProfilerRegion ps("Window::acquire_image", commandBuffer);
	if (!mSwapchain) create_swapchain(commandBuffer);
	
	mImageAvailableSemaphoreIndex = (mImageAvailableSemaphoreIndex+1)%mImageAvailableSemaphores.size();

	auto result = (*mSwapchainDevice)->acquireNextImageKHR(mSwapchain, numeric_limits<uint64_t>::max(), **mImageAvailableSemaphores[mImageAvailableSemaphoreIndex], {});
	if (result.result == vk::Result::eErrorOutOfDateKHR || result.result == vk::Result::eSuboptimalKHR) {
		create_swapchain(commandBuffer);
		if (!mSwapchain) return Texture::View(); // swapchain failed to create (happens when window is minimized, etc)
		result = (*mSwapchainDevice)->acquireNextImageKHR(mSwapchain, numeric_limits<uint64_t>::max(), **mImageAvailableSemaphores[mImageAvailableSemaphoreIndex], {});
	}
	mBackBufferIndex = result.value;
	return back_buffer();
}
void Window::present(const vector<vk::Semaphore>& waitSemaphores) {
	ProfilerRegion ps("Window::present");
	vector<vk::SwapchainKHR> swapchains { mSwapchain };
	vector<uint32_t> imageIndices { mBackBufferIndex };
	auto result = mPresentQueueFamily->mQueues[0].presentKHR(vk::PresentInfoKHR(waitSemaphores, swapchains, imageIndices));
	mPresentCount++;
}

#ifdef __linux
xcb_atom_t getReplyAtomFromCookie(xcb_connection_t* connection, xcb_intern_atom_cookie_t cookie) {
	xcb_generic_error_t * error;
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, &error);
	if (error) throw runtime_error("Could not set XCB screen: %d" + to_string(error->error_code));
	return reply->atom;
}
#endif

void Window::resize(uint32_t w, uint32_t h) {
#ifdef WIN32
	RECT r;
	GetWindowRect(mHwnd, &r);
	int x = r.left, y = r.top;
	GetClientRect(mHwnd, &r);
	r.right = r.left + w;
	r.bottom = r.top + h;
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
	SetWindowPos(mHwnd, HWND_TOPMOST, x, y, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED | SWP_NOACTIVATE);
#else
	throw exception("Window::resize not implemented");
#endif
}

void Window::fullscreen(bool fs) {
	#ifdef WIN32
	
	if (fs && !mFullscreen) {
		GetWindowRect(mHwnd, &mWindowedRect);

		UINT WindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
		SetWindowLongW(mHwnd, GWL_STYLE, WindowStyle);

		HMONITOR hMonitor = MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFOEX monitorInfo = {};
		monitorInfo.cbSize = sizeof(MONITORINFOEX);
		GetMonitorInfoA(hMonitor, &monitorInfo);

		SetWindowPos(mHwnd, HWND_TOPMOST,
			monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		ShowWindow(mHwnd, SW_MAXIMIZE);

		mFullscreen = true;
	} else if (!fs && mFullscreen) {
		SetWindowLongA(mHwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(mHwnd, HWND_NOTOPMOST,
			mWindowedRect.left,
			mWindowedRect.top,
			mWindowedRect.right  - mWindowedRect.left,
			mWindowedRect.bottom - mWindowedRect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
		ShowWindow(mHwnd, SW_NORMAL);

		mFullscreen = false;
	}

	#elif defined(__linux)

	if (fs == mFullscreen) return;
	mFullscreen = fs;

   	struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } hints = {0};

    //hints.flags = MWM_HINTS_DECORATIONS;
    //hints.decorations = mFullscreen ? 0 : MWM_DECOR_ALL;

	//xcb_intern_atom_cookie_t cookie = xcb_intern_atom(mInstance.XCBConnection(), 0, 16, "_MOTIF_WM_HINTS");
	//xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(mInstance.XCBConnection(), cookie, NULL);
	//xcb_change_property(mInstance.XCBConnection(), XCB_PROP_MODE_REPLACE, mXCBWindow, reply->atom, reply->atom, 32, sizeof(hints), &hints);

	#endif
}

void Window::create_swapchain(CommandBuffer& commandBuffer) {
	if (mSwapchain) destroy_swapchain();
	mSwapchainDevice = &commandBuffer.mDevice;
	mSwapchainDevice->set_debug_name(mSurface, "WindowSurface");
	
	mPresentQueueFamily = mSwapchainDevice->find_queue_family(mSurface);
	if (!mPresentQueueFamily) throw runtime_error("Device does not support the window surface!");

	vk::SurfaceCapabilitiesKHR capabilities = mSwapchainDevice->physical().getSurfaceCapabilitiesKHR(mSurface);
	auto formats 							 = mSwapchainDevice->physical().getSurfaceFormatsKHR(mSurface);
	auto presentModes 				 = mSwapchainDevice->physical().getSurfacePresentModesKHR(mSurface);
	auto queueFamilyProperties = mSwapchainDevice->physical().getQueueFamilyProperties();

	// get the size of the swapchain
	mSwapchainExtent = capabilities.currentExtent;
	if (mSwapchainExtent.width == 0 || mSwapchainExtent.height == 0 || mSwapchainExtent.width > mSwapchainDevice->limits().maxImageDimension2D || mSwapchainExtent.height > mSwapchainDevice->limits().maxImageDimension2D)
		return; // invalid swapchain size, window invalid

	// select the format of the swapchain
	mSurfaceFormat = formats[0];
	for (const vk::SurfaceFormatKHR& format : formats)
		if (format.format == vk::Format::eR8G8B8A8Unorm)
			mSurfaceFormat = format;
		else if (format.format == vk::Format::eB8G8R8A8Unorm)
			mSurfaceFormat = format;

	vector<vk::PresentModeKHR> preferredPresentModes { vk::PresentModeKHR::eMailbox };
	if (mAllowTearing) preferredPresentModes = { vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifoRelaxed };
	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
	for (auto mode : preferredPresentModes)
		if (ranges::find(presentModes, mode) != presentModes.end())
			presentMode = mode;

	vk::SwapchainCreateInfoKHR createInfo = {};
	createInfo.surface = mSurface;
	createInfo.minImageCount = capabilities.minImageCount + 1;
	createInfo.imageFormat = mSurfaceFormat.format;
	createInfo.imageColorSpace = mSurfaceFormat.colorSpace;
	createInfo.imageExtent = mSwapchainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
	createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	mSwapchain = (*mSwapchainDevice)->createSwapchainKHR(createInfo);
	mSwapchainDevice->set_debug_name(mSwapchain, "Window/Swapchain");

	// get the back buffers
	vector<vk::Image> images = (*mSwapchainDevice)->getSwapchainImagesKHR(mSwapchain);
	mSwapchainImages.resize(images.size());
	mImageAvailableSemaphores.clear();
	mImageAvailableSemaphores.resize(images.size());
	mBackBufferIndex = 0;
	mImageAvailableSemaphoreIndex = 0;
	
	// create per-frame image views and semaphores
	for (uint32_t i = 0; i < images.size(); i++) {
		mSwapchainDevice->set_debug_name(images[i], "swapchain_image");
		mSwapchainImages[i] = Texture::View(
			make_shared<Texture>(images[i], *mSwapchainDevice, "swapchain_image", vk::Extent3D(mSwapchainExtent,1), createInfo.imageFormat, createInfo.imageArrayLayers, 1, vk::SampleCountFlagBits::e1, createInfo.imageUsage),
			0, 1, 0, 1, vk::ImageAspectFlagBits::eColor);
		mSwapchainImages[i].texture().transition_barrier(commandBuffer, vk::ImageLayout::ePresentSrcKHR);
		mImageAvailableSemaphores[i] = make_unique<Semaphore>(*mSwapchainDevice, "Swapchain/ImageAvaiableSemaphore" + to_string(i));
	}
}
void Window::destroy_swapchain() {
	mSwapchainDevice->flush();
	mSwapchainImages.clear();
	mImageAvailableSemaphores.clear();
	if (mSwapchain) (*mSwapchainDevice)->destroySwapchainKHR(mSwapchain);
	mSwapchain = nullptr;
}

void Window::lock_mouse(bool l) {
	#ifdef WIN32
	if (mLockMouse && !l)
		ShowCursor(TRUE);
	else if (!mLockMouse && l)
		ShowCursor(FALSE);
	#else
	// TODO: hide cursor on linux
	#endif

	mLockMouse = l;
}
