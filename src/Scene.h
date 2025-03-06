#pragma once
#include<glad/glad.h>
#include "Window.h"

class Scene {
public:
	Scene() {};
	virtual void OnBufferSwap(Window& win) {};
	virtual void OnWindowLoad(Window& win) {};
	virtual void OnWindowClose(Window& win) {};
};