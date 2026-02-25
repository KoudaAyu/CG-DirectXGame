#pragma once

class Game;

class Framework
{
public:

	virtual ~Framework() = default;

	virtual void Initialize();

	virtual void Finalize();

	virtual void Update();

	virtual void Draw() = 0;

	virtual bool IsEndRequest() { return endRequest; }

	// Returns true when application should quit (e.g. WM_QUIT received).
	virtual bool IsQuitRequested() { return false; }

	void Run();

private:
	bool endRequest = false;

	Game* game = nullptr;
};

